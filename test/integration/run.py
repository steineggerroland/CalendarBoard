"""Real local MQTT broker -> VirtualEntities publisher -> native firmware core.

Use a Python environment with VirtualEntities requirements and pass a separate
Python executable containing amqtt. Never reads application credentials/config.
"""
import argparse
from datetime import date, datetime, timedelta, timezone
import json
from pathlib import Path
import queue
import selectors
import socket
import subprocess
import sys
import tempfile
import threading
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--virtualentities', required=True, type=Path)
parser.add_argument('--broker-python', required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(args.virtualentities.resolve()))
import paho.mqtt.client as paho
from iot.core.configuration import CalendarBoardConfig, BoardRowConfig, MqttConfiguration
from iot.infrastructure.time.calendar import Appointment, Calendar
from iot.mqtt.mqtt_client import MqttClient
from iot.mqtt.calendar_board_publisher import CalendarBoardPublisher

class Sources:
    def __init__(self, calendar):
        self.calendar = calendar
        self.errors = frozenset()
        self.lock = threading.Lock()

    def calendar_snapshot(self):
        with self.lock:
            return (self.calendar,), self.errors

    def replace(self, calendar=None, failed=False):
        with self.lock:
            if calendar is not None:
                self.calendar = calendar
            self.errors = frozenset({'calendar'}) if failed else frozenset()

class Observer:
    def __init__(self, port):
        self.messages = queue.Queue()
        self.client = paho.Client(paho.CallbackAPIVersion.VERSION2)
        self.subscribed = threading.Event()
        self.client.on_connect = lambda client, *unused: client.subscribe('calendarboard/v1/integration/#', qos=1)
        self.client.on_subscribe = lambda *unused: self.subscribed.set()
        self.client.on_message = lambda client, userdata, message: self.messages.put(message)
        self.client.connect('127.0.0.1', port)
        self.client.loop_start()
        assert self.subscribed.wait(5), 'Subscription not acknowledged'

    def close(self):
        self.client.disconnect()
        self.client.loop_stop()

    def collect(self, predicate, timeout=5):
        found = []
        deadline = time.monotonic() + timeout
        while not predicate(found):
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise AssertionError('Timed out waiting for MQTT messages')
            found.append(self.messages.get(timeout=remaining))
        return found

with tempfile.TemporaryDirectory(prefix='calendarboard-integration-') as temp:
    temp = Path(temp)
    cjson = root / '.pio/libdeps/ota/Arduino_JSON/src'
    subprocess.run(['cc', '-fsanitize=address,undefined', '-g', '-c', str(cjson / 'cjson/cJSON.c'),
                    '-o', str(temp / 'cJSON.o')], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    executable = temp / 'board'
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-g',
                    '-I'+str(root / 'lib/Board'), '-I'+str(cjson), str(root / 'lib/Board/Board.cpp'),
                    str(root / 'lib/Board/Protocol.cpp'), str(root / 'test/integration/board_receiver.cpp'),
                    str(temp / 'cJSON.o'), '-o', str(executable)], check=True)
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0))
        port = probe.getsockname()[1]
    broker_log = (temp / 'broker.log').open('w')
    def start_broker():
        process = subprocess.Popen([args.broker_python, str(root / 'test/integration/broker.py'), str(port)],
                                   stdout=subprocess.PIPE, stderr=broker_log, text=True)
        return process
    def wait_for_broker(process):
        with selectors.DefaultSelector() as selector:
            selector.register(process.stdout, selectors.EVENT_READ)
            assert selector.select(10), 'Broker did not start'
            assert process.stdout.readline().strip() == 'ready', (temp / 'broker.log').read_text()
    broker = start_broker()
    native = None
    publisher = None
    mqtt = None
    observers = []
    try:
        wait_for_broker(broker)
        native = subprocess.Popen([str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        def command(line):
            native.stdin.write(line+'\n'); native.stdin.flush()
            response = native.stdout.readline()
            assert response, 'Native firmware core exited'
            return json.loads(response)
        now = [datetime(2026, 9, 11, 12, 0, 1, tzinfo=timezone.utc)]
        names = ('person1', 'person2', 'person3', 'person4')
        def calendar(important=True, occupied=True):
            appointments = [Appointment('Wichtig: Test' if important else 'Test',
                                        now[0], now[0] + timedelta(minutes=30), '')] if occupied else []
            return Calendar('calendar', '', '', appointments, last_seen_at=now[0],
                            loaded_from=now[0].date(), loaded_until=now[0].date()+timedelta(days=7))
        services = {name: Sources(calendar()) for name in names}
        mqtt = MqttClient(MqttConfiguration('127.0.0.1', 'integration-sender', port))
        publisher = CalendarBoardPublisher(mqtt, CalendarBoardConfig('integration',
                                            tuple(BoardRowConfig(name, name) for name in names)), services, lambda: now[0])
        mqtt.start()
        deadline = time.monotonic()+5
        while not mqtt.is_connected() and time.monotonic() < deadline:
            time.sleep(.02)
        assert mqtt.is_connected()
        # Tick manually to keep the test deterministic; MQTT runs its real network loop.
        publisher.tick()
        observer = Observer(port); observers.append(observer)
        retained = observer.collect(lambda items: sum('/rows/' in item.topic for item in items) == 4)
        assert len(retained) == 4 and all(item.retain for item in retained)
        assert all(not item.topic.endswith('/time') for item in retained)
        def receive(message):
            payload = message.payload.decode()
            if message.topic.endswith('/time'):
                result = command('time '+payload)
            elif '/rows/' in message.topic:
                index = names.index(message.topic.split('/')[-2])
                result = command(f'day {index} '+payload)
            else:
                return
            assert result['accepted'], 'Firmware rejected real publisher message'
            return result
        for message in retained:
            receive(message)
        assert all(color == 0 for row in command('state')['colors'] for color in row)
        print('PASS: late subscriber receives four retained rows, no retained clock', flush=True)
        assert command('request')['request']
        observer.client.publish('calendarboard/v1/integration/sync/request', '{"schema_version":1}', qos=1).wait_for_publish(2)
        deadline = time.monotonic()+5
        while not publisher._sync.is_set() and time.monotonic() < deadline:
            time.sleep(.02)
        assert publisher._sync.is_set(), 'Publisher did not receive board request'
        publisher.tick()
        replies = observer.collect(lambda items: any(item.topic.endswith('/time') for item in items)
                                    and sum('/rows/' in item.topic for item in items) == 4)
        for message in replies:
            receive(message)
        assert [row[14] for row in command('state')['colors']] == [0xff0000]*4
        # First date discovery permits one final sync; once acknowledged no periodic request is needed.
        command('request')
        assert not command('request')['request']
        print('PASS: board request synchronizes time and all four rendered rows', flush=True)
        publisher.tick()
        try:
            unexpected = observer.messages.get(timeout=.2)
            raise AssertionError('Unexpected traffic for unchanged data: '+unexpected.topic)
        except queue.Empty:
            pass
        print('PASS: unchanged calendars produce no MQTT traffic', flush=True)
        command('night on')
        services['person1'].replace(calendar(False))
        publisher.tick()
        changed = observer.collect(lambda items: any('/rows/person1/day' in item.topic for item in items))
        for message in changed:
            receive(message)
        assert all(color == 0 for row in command('state')['colors'] for color in row)
        assert command('night off')['colors'][0][14] == 0xffffff
        services['person1'].replace(failed=True)
        publisher.tick()
        stale = observer.collect(lambda items: any('/rows/person1/day' in item.topic for item in items))
        assert json.loads(stale[-1].payload)['status'] == 'stale'
        for message in stale:
            receive(message)
        assert command('state')['colors'][0][14] == 0xffffff
        services['person1'].replace(calendar(occupied=False))
        publisher.tick()
        for message in observer.collect(lambda items: any('/rows/person1/day' in item.topic for item in items)):
            receive(message)
        assert command('state')['colors'][0][14] == 0
        print('PASS: night mode, source failure, recovery and empty day', flush=True)
        # Midnight without incoming packets preserves the old day, and requests new data.
        command('advance 35999000')
        assert command('state')['date'] == 20260912
        assert command('state')['colors'][1][14] == 0xff0000
        assert command('request')['request']
        now[0] = datetime(2026,9,11,22,tzinfo=timezone.utc)
        for service in services.values():
            service.replace(calendar(occupied=False))
        publisher.tick()
        for message in observer.collect(lambda items: sum('/rows/' in item.topic for item in items) == 4):
            receive(message)
        assert command('state')['date'] == 20260912
        assert command('state')['colors'][1][14] == 0
        print('PASS: offline midnight preserves data until new day arrives', flush=True)
        # Restart the broker without a retained database. Both clients must reconnect automatically.
        broker.terminate()
        broker.wait(timeout=10)
        observer.subscribed.clear()
        broker = start_broker()
        wait_for_broker(broker)
        deadline = time.monotonic()+10
        while (not mqtt.is_connected() or not publisher._sync.is_set()) and time.monotonic() < deadline:
            time.sleep(.02)
        assert mqtt.is_connected(), 'Publisher did not reconnect after broker restart'
        assert publisher._sync.is_set()
        assert observer.subscribed.wait(10), 'Observer did not reconnect'
        publisher.tick()
        reconnect = observer.collect(lambda items: any(item.topic.endswith('/time') for item in items)
                                     and sum('/rows/' in item.topic for item in items) == 4)
        for message in reconnect:
            receive(message)
        print('PASS: broker restart triggers automatic reconnect and republishes time and all rows', flush=True)
        print('Integration test passed (real broker + production publisher + native firmware core).', flush=True)
    finally:
        try:
            if publisher:
                publisher.shutdown()
            for observer in observers:
                observer.close()
            if mqtt:
                mqtt.stop()
            if native:
                native.stdin.close()
                native.wait(timeout=5)
                assert native.returncode == 0, 'Native sanitizer test failed'
        finally:
            broker.terminate()
            broker.wait(timeout=10)
            broker_log.close()

"""Generate protocol fixtures using a local VirtualEntities checkout, without network I/O."""
import argparse
from datetime import date, datetime, timedelta, timezone
from pathlib import Path
import sys
from unittest.mock import Mock

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('virtualentities', type=Path)
args = parser.parse_args()
sys.path.insert(0, str(args.virtualentities.resolve()))

from iot.core.configuration import CalendarBoardConfig, BoardRowConfig
from iot.infrastructure.time.calendar import Appointment, Calendar
from iot.mqtt.calendar_board_publisher import CalendarBoardPublisher

now = datetime(2026, 9, 11, 12, tzinfo=timezone.utc)
calendar = Calendar('test', '', '', [Appointment('Wichtig: Test', now, now + timedelta(hours=1), '')],
                    last_seen_at=now, loaded_from=date(2026, 9, 11), loaded_until=date(2026, 9, 18))
service = Mock()
service.calendar_snapshot.return_value = ((calendar,), frozenset())
mqtt = Mock()
mqtt.publish.return_value = Mock(rc=0, is_published=Mock(return_value=True))
publisher = CalendarBoardPublisher(mqtt, CalendarBoardConfig('test', (BoardRowConfig('person1', 'person1'),)),
                                  {'person1': service}, lambda: now)
publisher.tick()
for call in mqtt.publish.call_args_list:
    name = 'time' if call.args[0].endswith('/time') else 'day'
    (Path(__file__).parent / 'fixtures' / f'{name}.json').write_text(call.args[1])

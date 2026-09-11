"""Ephemeral loopback-only broker for the integration test (requires amqtt)."""
import asyncio
import signal
import sys
from amqtt.broker import Broker

async def main():
    broker = Broker({'listeners': {'default': {'type': 'tcp', 'bind': f'127.0.0.1:{int(sys.argv[1])}'}},
                     'plugins': {'amqtt.plugins.authentication.AnonymousAuthPlugin': {'allow_anonymous': True}}})
    stopped = asyncio.Event()
    asyncio.get_running_loop().add_signal_handler(signal.SIGTERM, stopped.set)
    await broker.start()
    print('ready', flush=True)
    try:
        await stopped.wait()
    finally:
        await broker.shutdown()

asyncio.run(main())

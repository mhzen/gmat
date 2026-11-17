"""Bench serial logger and interactive command sender. No delivery guarantee."""
import argparse
import csv
from datetime import datetime, timezone
import queue
import re
import sys
import threading
import time

FIELDS = 'seq,t_ms,ax,ay,az,gx,gy,gz,state,flags,pressure_pa,temperature_centi_c,rx_time'.split(',')
LIMITS = [(0, 2**32 - 1)] * 2 + [(-32768, 32767)] * 6 + [(0, 6), (0, 3), (30000, 110000), (-4000, 8500)]


def parse(line):
    parts = line.decode('ascii').split(',')
    if len(parts) != len(LIMITS) or any(not re.fullmatch(r'-?[0-9]+', p) for p in parts):
        raise ValueError('expected twelve decimal integers')
    values = list(map(int, parts))
    if any(not low <= v <= high for v, (low, high) in zip(values, LIMITS)):
        raise ValueError('telemetry outside wire ranges')
    return values


class Lines:
    """Bound memory and discard an entire malformed line through LF."""
    def __init__(self):
        self.buffer = bytearray()
        self.overflow = False

    def push(self, data):
        for byte in data:
            if byte == 10:
                yield None if self.overflow else bytes(self.buffer).removesuffix(b'\r')
                self.buffer.clear()
                self.overflow = False
            elif len(self.buffer) < 256:
                self.buffer.append(byte)
            else:
                self.overflow = True


def command_bytes(text):
    if text not in ('ARM', 'DISARM', 'FIRE'):
        raise ValueError('use exact ARM, DISARM or FIRE')
    return (text + '\n').encode('ascii')


def read_commands(commands):
    # Never accumulate commands for later replay after a stalled serial link.
    while True:
        text = sys.stdin.readline(258)
        if not text:
            return
        if not text.endswith('\n'):
            while text and not text.endswith('\n'):
                text = sys.stdin.readline(258)
            print('# rejected overlong/unterminated command', file=sys.stderr)
            continue
        try:
            data = command_bytes(text.removesuffix('\n').removesuffix('\r'))
            commands.put_nowait((data, time.monotonic()))
        except (ValueError, queue.Full) as error:
            print(f'# command rejected: {error}', file=sys.stderr)


def run(port, log, output, commands, on_event=None):
    def event(kind, value):
        if on_event:
            on_event(kind, value)
    writer = csv.writer(log)
    writer.writerow(FIELDS)
    log.flush()
    framer = Lines()
    while True:
        try:
            data, entered = commands.get_nowait()
        except queue.Empty:
            pass
        else:
            if time.monotonic() - entered > 0.5:
                event('status', 'stale command rejected')
                print('# stale command rejected', file=sys.stderr)
            else:
                if port.write(data) != len(data):
                    raise OSError('short serial command write; delivery unknown')
                event('status', 'Written to USB; delivery unknown')
                print('# command written to USB; payload receipt unknown', file=sys.stderr)
        for line in framer.push(port.read(256)):
            event('raw', line.decode('ascii', errors='backslashreplace') if line is not None else '[overlong line]')
            received = datetime.now(timezone.utc).isoformat(timespec='milliseconds')
            if line is not None and line.startswith(b'#'):
                print(line.decode('ascii', errors='backslashreplace'), file=sys.stderr)
                continue
            try:
                if line is None:
                    raise ValueError('overlong serial line')
                values = parse(line)
            except (ValueError, UnicodeError) as error:
                print(f'# telemetry rejected: {error}', file=sys.stderr)
                continue
            writer.writerow(values + [received])
            log.flush()
            event('telemetry', values + [received])
            print(','.join(map(str, values + [received])), file=output, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--log', required=True, help='new CSV path; never overwritten')
    args = parser.parse_args()
    import serial
    try:
        # Exclusive creation prevents accidental destruction of earlier test logs.
        with open(args.log, 'x', newline='', encoding='utf-8') as log:
            with serial.Serial(args.port, 115200, timeout=0.1, write_timeout=0.2) as port:
                commands = queue.Queue(maxsize=1)
                threading.Thread(target=read_commands, args=(commands,), daemon=True).start()
                run(port, log, sys.stdout, commands)
    except KeyboardInterrupt:
        return 0
    except (OSError, serial.SerialException) as error:
        print(f'# bridge stopped: {error}; no reconnect or command retry', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())

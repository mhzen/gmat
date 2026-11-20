"""Serial telemetry logger and USB telemetry reader."""
import argparse
import csv
from datetime import datetime, timezone
import re
import sys

FIELDS = 'seq,t_ms,ax,ay,az,gx,gy,gz,state,pressure_pa,temperature_centi_c,rx_time'.split(',')
LIMITS = [(0, 2**32 - 1)] * 2 + [(-32768, 32767)] * 6 + [(0, 3), (0, 3), (30000, 110000), (-4000, 8500)]


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


def run(port, log, output, on_event=None):
    def event(kind, value):
        if on_event:
            on_event(kind, value)
    writer = csv.writer(log)
    writer.writerow(FIELDS)
    log.flush()
    framer = Lines()
    while True:
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
            state = 'FIRE_ACCEPTED' if values[8] in (2, 3) else 'IDLE'
            row = values[:8] + [state] + values[10:] + [received]
            writer.writerow(row)
            log.flush()
            event('telemetry', values + [received])
            print(','.join(map(str, row)), file=output, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--log', required=True, help='new CSV path; never overwritten')
    args = parser.parse_args()
    import serial
    try:
        # Exclusive creation prevents accidental destruction of earlier test logs.
        with open(args.log, 'x', newline='', encoding='utf-8') as log:
            with serial.Serial(args.port, 115200, timeout=0.1) as port:
                run(port, log, sys.stdout)
    except KeyboardInterrupt:
        return 0
    except (OSError, serial.SerialException) as error:
        print(f'# bridge stopped: {error}; no reconnect', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())

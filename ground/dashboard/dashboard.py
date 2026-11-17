"""Loopback-only bench dashboard."""
import argparse
from collections import deque
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import queue
import sys
import threading
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'bridge'))
import bridge


class State:
    def __init__(self, log):
        self.lock = threading.Lock()
        self.samples = deque(maxlen=300)
        self.terminal = deque(maxlen=200)
        self.commands = queue.Queue(maxsize=1)
        self.connected = False
        self.last = 0
        self.log = log

    def event(self, kind, value):
        with self.lock:
            now = time.monotonic()
            if kind == 'telemetry':
                self.last = now
                self.samples.append([time.time(), value])
            else:
                self.terminal.append(datetime.now(timezone.utc).strftime('%H:%M:%S.%f')[:12] + ' ' + str(value))

    def snapshot(self):
        with self.lock:
            return dict(connected=self.connected, age=time.monotonic()-self.last,
                        samples=list(self.samples), terminal=list(self.terminal), log=self.log)

    def command(self, name):
        data = bridge.command_bytes(name)
        with self.lock:
            if not self.connected or time.monotonic()-self.last > 1:
                raise ValueError('No fresh telemetry')
            p = self.samples[-1][1]
            if name == 'ARM' and p[8] != 0 or name == 'FIRE' and (p[8] != 1 or not p[9] & 1):
                raise ValueError('Payload state rejects command')
            self.commands.put_nowait((data, time.monotonic()))
        self.event('status', name + ' queued; delivery unknown')


def handler(state):
    class Handler(BaseHTTPRequestHandler):
        def reply(self, code, data, mime='application/json'):
            self.send_response(code)
            self.send_header('Content-Type', mime)
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            self.wfile.write(data)

        def do_GET(self):
            if self.path == '/api/snapshot':
                self.reply(200, json.dumps(state.snapshot()).encode())
            elif self.path == '/':
                self.reply(200, Path(__file__).with_name('index.html').read_bytes(), 'text/html; charset=utf-8')
            else:
                self.reply(404, b'{}')

        def do_POST(self):
            host = '127.0.0.1:8000'
            if (self.path != '/api/command' or self.headers.get('Host') != host or
                    self.headers.get('Origin') != 'http://' + host or
                    self.headers.get('Content-Type') != 'application/json'):
                self.reply(403, b'{"error":"Forbidden"}')
                return
            try:
                size = int(self.headers.get('Content-Length', '0'))
                if not 0 < size <= 64:
                    raise ValueError('Invalid body size')
                body = json.loads(self.rfile.read(size))
                if not isinstance(body, dict) or not isinstance(body.get('command'), str):
                    raise ValueError('Invalid command')
                state.command(body['command'])
                self.reply(200, b'{"status":"Queued; delivery unknown"}')
            except (ValueError, queue.Full) as error:
                self.reply(400, json.dumps({'error': str(error) or 'Command queue full'}).encode())

        def log_message(self, *args):
            pass
    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--log', required=True)
    args = parser.parse_args()
    state = State(args.log)
    def worker():
        try:
            import serial
            with open(args.log, 'x', newline='', encoding='utf-8') as log:
                with serial.Serial(args.port, 115200, timeout=0.1, write_timeout=0.2) as port:
                    with state.lock:
                        state.connected = True
                    bridge.run(port, log, sys.stdout, state.commands, state.event)
        except Exception as error:
            state.event('status', f'Bridge stopped: {error}; delivery unknown')
        finally:
            with state.lock:
                state.connected = False
    server = ThreadingHTTPServer(('127.0.0.1', 8000), handler(state))
    threading.Thread(target=worker, daemon=True).start()
    print('Bench dashboard: http://127.0.0.1:8000')
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == '__main__':
    main()

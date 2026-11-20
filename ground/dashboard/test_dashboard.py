import unittest
import http.client
import threading
from unittest.mock import patch
import dashboard


class Tests(unittest.TestCase):
    def test_http_assets_and_removed_command_endpoint(self):
        state = dashboard.State('sim.csv', simulate=True)
        server = dashboard.ThreadingHTTPServer(('127.0.0.1', 0), dashboard.handler(state))
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        connection = http.client.HTTPConnection(*server.server_address, timeout=2)
        try:
            for path in ('/', '/dashboard.css', '/app.js', '/rocket.js', '/motion.mjs', '/vendor/three.module.js'):
                connection.request('GET', path)
                response = connection.getresponse()
                self.assertEqual(response.status, 200, path)
                self.assertTrue(response.read(), path)
            connection.request('GET', '/../dashboard.py')
            response = connection.getresponse()
            self.assertEqual(response.status, 404)
            response.read()
            connection.request('POST', '/api/command', '{"command":"ARM"}',
                               {'Host': '127.0.0.1:8000', 'Origin': 'http://127.0.0.1:8000',
                                'Content-Type': 'application/json'})
            response = connection.getresponse()
            self.assertEqual(response.status, 501)
            response.read()
        finally:
            connection.close()
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)

    def test_simulation(self):
        port = dashboard.SimulatedPort()
        states = set()
        with patch('dashboard.time.sleep'):
            for seq in range(301):
                values = dashboard.bridge.parse(port.read(256).strip())
                self.assertEqual(values[0], seq)
                self.assertEqual(values[1], seq * 200)
                states.add(values[8])
        self.assertEqual(states, set(range(4)))
        s = dashboard.State('sim.csv', simulate=True)
        s.connected = True
        s.event('telemetry', values)
        self.assertTrue(s.snapshot()['simulate'])

    def test_state_history(self):
        s = dashboard.State('test.csv')
        s.connected = True
        for i in range(400):
            s.event('raw', str(i))
            s.event('telemetry', [i, 0, 0, 0, 0, 0, 0, 0, 0, 0, 100653, 2508, 'UTC'])
        self.assertEqual(len(s.snapshot()['samples']), 300)
        self.assertEqual(len(s.snapshot()['terminal']), 200)


if __name__ == '__main__':
    unittest.main()

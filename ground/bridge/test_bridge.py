import io
import queue
import unittest
from unittest.mock import patch
import bridge


class Tests(unittest.TestCase):
    def test_ranges(self):
        self.assertEqual(bridge.parse(b'4294967295,0,-32768,32767,0,0,0,0,6,3,100653,2508')[2], -32768)
        for line in (b'1,2', b'-1,0,0,0,0,0,0,0,0,0',
                     b'0,0,32768,0,0,0,0,0,0,0', b'0,0,0,0,0,0,0,0,7,0',
                     b'0,0,0,0,0,0,0,0,0,4', b' 0,0,0,0,0,0,0,0,0,0', b'\xff'):
            if line.count(b',') == 9:
                line += b',100653,2508'
            with self.assertRaises(ValueError):
                bridge.parse(line)
        with self.assertRaises(ValueError):
            bridge.parse(b'0,0,0,0,0,0,0,0,0,0') # Old schema is not accepted.

    def test_barometer_ranges(self):
        prefix = b'0,0,0,0,0,0,0,0,0,0,'
        for suffix in (b'30000,-4000', b'110000,8500'):
            self.assertEqual(len(bridge.parse(prefix + suffix)), 12)
        for suffix in (b'29999,0', b'110001,0', b'100000,-4001', b'100000,8501'):
            with self.assertRaises(ValueError):
                bridge.parse(prefix + suffix)

    def test_framing(self):
        f = bridge.Lines()
        self.assertEqual(list(f.push(b'AR')), [])
        self.assertEqual(list(f.push(b'M\r\n' + b'x' * 300 + b'\nOK\n')), [b'ARM', None, b'OK'])

    def test_commands(self):
        for text in ('ARM', 'DISARM', 'FIRE'):
            self.assertEqual(bridge.command_bytes(text), (text + '\n').encode())
        for text in ('arm', 'FIRE ', '', 'ARM\nFIRE'):
            with self.assertRaises(ValueError):
                bridge.command_bytes(text)

    def test_logging_disconnect_and_command(self):
        class Port:
            reads = 0
            sent = []
            def write(self, data):
                self.sent.append(data)
                return len(data)
            def read(self, size):
                self.reads += 1
                if self.reads > 1:
                    raise OSError('disconnected')
                return b'# radio disabled\nbad\n0,0,0,0,0,0,0,0,0,0,100653,2508\n'
        port = Port()
        commands = queue.Queue()
        commands.put((b'ARM\n', bridge.time.monotonic()))
        log, output = io.StringIO(), io.StringIO()
        with patch('sys.stderr', io.StringIO()), self.assertRaises(OSError):
            bridge.run(port, log, output, commands)
        self.assertEqual(port.sent, [b'ARM\n'])
        rows = list(bridge.csv.reader(io.StringIO(log.getvalue())))
        self.assertEqual(rows[0], bridge.FIELDS)
        self.assertEqual(len(rows), 2)
        self.assertTrue(rows[1][-1].endswith('+00:00'))
        self.assertIn(','.join(rows[1]), output.getvalue())

    def test_stale_and_short_write(self):
        class Port:
            sent = False
            def write(self, data):
                self.sent = True
                return 1
            def read(self, size):
                raise OSError('disconnected')
        for stale in (False, True):
            port = Port()
            commands = queue.Queue()
            commands.put((b'FIRE\n', bridge.time.monotonic() - (10 if stale else 0)))
            with patch('sys.stderr', io.StringIO()), self.assertRaises(OSError):
                bridge.run(port, io.StringIO(), io.StringIO(), commands)
            self.assertEqual(port.sent, not stale)


if __name__ == '__main__':
    unittest.main()

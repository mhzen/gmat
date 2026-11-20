import io
import unittest
from unittest.mock import patch
import bridge


class Tests(unittest.TestCase):
    def test_ranges(self):
        self.assertEqual(bridge.parse(b'4294967295,0,-32768,32767,0,0,0,0,3,3,100653,2508')[2], -32768)
        for line in (b'1,2', b'-1,0,0,0,0,0,0,0,0,0',
                     b'0,0,32768,0,0,0,0,0,0,0', b'0,0,0,0,0,0,0,0,4,0',
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

    def test_logging_and_disconnect(self):
        class Port:
            reads = 0
            def read(self, size):
                self.reads += 1
                if self.reads > 1:
                    raise OSError('disconnected')
                return (b'# radio disabled\nbad\n0,0,0,0,0,0,0,0,0,0,100653,2508\n'
                        b'1,200,0,0,0,0,0,0,1,1,100654,2509\n'
                        b'2,400,0,0,0,0,0,0,2,2,100655,2510\n'
                        b'3,600,0,0,0,0,0,0,3,2,100656,2511\n')
        port = Port()
        log, output = io.StringIO(), io.StringIO()
        with patch('sys.stderr', io.StringIO()), self.assertRaises(OSError):
            bridge.run(port, log, output)
        rows = list(bridge.csv.reader(io.StringIO(log.getvalue())))
        self.assertEqual(rows[0][8:], ['state', 'pressure_pa', 'temperature_centi_c', 'rx_time'])
        self.assertEqual([row[8] for row in rows[1:]], ['IDLE', 'IDLE', 'FIRE_ACCEPTED', 'FIRE_ACCEPTED'])
        self.assertTrue(all(len(row) == len(rows[0]) for row in rows))
        self.assertEqual(rows[-1][9:11], ['100656', '2511'])
        self.assertTrue(rows[1][-1].endswith('+00:00'))
        self.assertIn(','.join(rows[1]), output.getvalue())


if __name__ == '__main__':
    unittest.main()

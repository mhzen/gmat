import unittest
import dashboard


class Tests(unittest.TestCase):
    def test_state_and_commands(self):
        s = dashboard.State('test.csv')
        with self.assertRaises(ValueError):
            s.command('ARM')
        s.connected = True
        for i in range(400):
            s.event('raw', str(i))
            s.event('telemetry', [i, 0, 0, 0, 0, 0, 0, 0, 0, 0, 100653, 2508, 'UTC'])
        self.assertEqual(len(s.snapshot()['samples']), 300)
        self.assertEqual(len(s.snapshot()['terminal']), 200)
        with self.assertRaises(ValueError):
            s.command('FIRE')
        s.command('ARM')
        self.assertEqual(s.commands.get_nowait()[0], b'ARM\n')
        s.last -= 2
        with self.assertRaises(ValueError):
            s.command('DISARM')


if __name__ == '__main__':
    unittest.main()

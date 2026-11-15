import csv
import io
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


BINARY = Path(sys.argv.pop(1)).resolve()
BACKEND = sys.argv.pop(1)


class ProgramTests(unittest.TestCase):
    def run_program(self, *args):
        with tempfile.TemporaryDirectory() as directory:
            return subprocess.run(
                [str(BINARY), *args], cwd=directory, text=True, capture_output=True,
                env=dict(os.environ, OMP_DYNAMIC="FALSE", OMP_THREAD_LIMIT="4"),
                timeout=30,
            )

    def test_runs_without_arguments_and_prints_one_result(self):
        result = self.run_program()
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = list(csv.DictReader(io.StringIO(result.stdout)))
        self.assertEqual(len(rows), 1)
        row = rows[0]
        self.assertEqual(row['backend'], BACKEND)
        if BACKEND == 'seq':
            self.assertEqual(row['workers'], '1')
        self.assertGreaterEqual(int(row['samples']), 2)
        self.assertGreater(int(row['workers']), 0)
        self.assertGreater(float(row['t_total']), 0)
        if row['mode'] == 'full':
            exact = {'sin': 2, 'gauss1d': 0.746824132812427,
                     'peak': math.atan(5)/5, 'gaussNd': 0.3110809188228764}[row['func']]
            self.assertLess(abs(float(row['estimate']) - exact),
                            6 * float(row['std_err']))
        else:
            self.assertTrue(math.isfinite(float(row['checksum'])))
            self.assertEqual(row['estimate'], '')
            self.assertEqual(row['ci_lo'], '')

    def test_old_arguments_are_rejected_instead_of_silently_ignored(self):
        result = self.run_program('--samples', '1000')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, '')
        self.assertTrue(result.stderr.strip())


if __name__ == '__main__':
    unittest.main()

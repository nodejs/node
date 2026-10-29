# Copyright Node.js contributors. All rights reserved.
# Use of this source code is governed by an MIT-style license that can be
# found in the LICENSE file.

import os
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import test as runner


class TestRunnerOutputTest(unittest.TestCase):
  def test_invalid_utf8(self):
    context = SimpleNamespace(verbose=False, suppress_dialogs=False,
                              abort_on_timeout=False)
    with tempfile.TemporaryDirectory() as directory:
      with patch.object(runner.tempfile, 'tempdir', directory):
        for stream, fd in [('stdout', 1), ('stderr', 2)]:
          for exit_code in [0, 3]:
            with self.subTest(stream=stream, exit_code=exit_code):
              script = (f"import os; os.write({fd}, b'valid \\xc3\\xa9\\ninvalid \\xff\\r\\n'); "
                        f"raise SystemExit({exit_code})")
              result = runner.Execute([sys.executable, '-c', script], context, timeout=10)
              self.assertEqual(getattr(result, stream), 'valid é\ninvalid \ufffd\n')
              self.assertEqual(result.exit_code, exit_code)
              self.assertFalse(result.timed_out)
              self.assertEqual(os.listdir(directory), [])


if __name__ == '__main__':
  unittest.main()

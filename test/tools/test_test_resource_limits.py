import concurrent.futures
import json
import os
import sys
import unittest
from types import SimpleNamespace
from unittest import mock

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import test as runner

if sys.platform != 'win32':
  import resource


@unittest.skipIf(sys.platform == 'win32', 'POSIX resource limits')
class ResourceLimitsTest(unittest.TestCase):
  def setUp(self):
    self.context = SimpleNamespace(verbose=False, suppress_dialogs=False,
                                   abort_on_timeout=False)

  def test_core_limits_are_isolated_between_concurrent_processes(self):
    parent_limits = resource.getrlimit(resource.RLIMIT_CORE)
    code = '''import json, os, resource, sys
print(json.dumps([resource.getrlimit(resource.RLIMIT_CORE),
                  sys.argv[1:], os.environ['RESOURCE_LIMIT_TEST']]))
'''

    def run(index):
      result = runner.Execute([sys.executable, '-c', code, 'a b', '--literal'],
                              self.context, timeout=5,
                              env={'RESOURCE_LIMIT_TEST': str(index)},
                              disable_core_files=True)
      self.assertEqual(result.exit_code, 0, result.stderr)
      self.assertEqual(json.loads(result.stdout), [[0, 0], ['a b', '--literal'], str(index)])

    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
      list(pool.map(run, range(8)))
    self.assertEqual(resource.getrlimit(resource.RLIMIT_CORE), parent_limits)

  def test_wrapper_exec_preserves_the_test_process_id(self):
    processes = []
    original_popen = runner.subprocess.Popen

    def popen(*args, **kwargs):
      self.assertIsNone(kwargs.get('preexec_fn'))
      process = original_popen(*args, **kwargs)
      processes.append(process)
      return process

    with mock.patch.object(runner.subprocess, 'Popen', side_effect=popen):
      result = runner.Execute([sys.executable, '-c', 'import os; print(os.getpid())'],
                              self.context, timeout=5, disable_core_files=True)
    self.assertEqual(result.exit_code, 0, result.stderr)
    self.assertEqual(int(result.stdout), processes[0].pid)

  def test_wrapper_preserves_failures_and_timeouts(self):
    failure = runner.Execute([sys.executable, '-c', 'raise SystemExit(7)'],
                             self.context, timeout=5, disable_core_files=True)
    self.assertEqual(failure.exit_code, 7)
    self.assertFalse(failure.timed_out)
    timeout = runner.Execute([sys.executable, '-c', 'import time; time.sleep(60)'],
                             self.context, timeout=0.1, disable_core_files=True)
    self.assertTrue(timeout.timed_out)
    self.assertNotEqual(timeout.exit_code, 0)


if __name__ == '__main__':
  unittest.main()

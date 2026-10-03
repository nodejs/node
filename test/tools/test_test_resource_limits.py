import concurrent.futures
import json
import os
import runpy
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

  def test_memory_launcher_sets_both_limits_before_exec(self):
    limit = 512 * 1024 * 1024
    helper = os.path.join(ROOT, 'tools', 'test-resource-limits.py')
    command = [sys.executable, '-c', 'pass']
    argv = [helper, '--max-virtual-memory', str(limit), '--'] + command
    with mock.patch.object(sys, 'argv', argv), \
         mock.patch.object(resource, 'setrlimit') as setrlimit, \
         mock.patch.object(os, 'execvpe') as execvpe:
      runpy.run_path(helper, run_name='__main__')
    self.assertEqual(setrlimit.call_args_list,
                     [mock.call(resource.RLIMIT_CORE, (0, 0)),
                      mock.call(resource.RLIMIT_AS, (limit, limit + 1))])
    execvpe.assert_called_once_with(command[0], command, os.environ)

  def test_memory_limits_use_launcher_only_on_linux(self):
    command = [sys.executable, '-c', 'pass']
    for platform in ['linux', 'macos']:
      with self.subTest(platform=platform), \
           mock.patch.object(runner.utils, 'GuessOS', return_value=platform), \
           mock.patch.object(runner, 'RunProcess', return_value=(None, 0, False)) as run:
        runner.Execute(command, self.context, max_virtual_memory=123456)
      actual = run.call_args.kwargs
      self.assertNotIn('preexec_fn', actual)
      if platform == 'linux':
        self.assertEqual(actual['args'],
                         [sys.executable, os.path.join(ROOT, 'tools', 'test-resource-limits.py'),
                          '--max-virtual-memory', '123456', '--'] + command)
      else:
        self.assertEqual(actual['args'], command)

  @unittest.skipUnless(sys.platform.startswith('linux'), 'Linux virtual memory limits')
  def test_concurrent_memory_limits_are_inherited_by_tests(self):
    parent_limits = resource.getrlimit(resource.RLIMIT_AS)
    code = 'import json, resource; print(json.dumps(resource.getrlimit(resource.RLIMIT_AS)))'

    def run(limit):
      result = runner.Execute([sys.executable, '-c', code], self.context,
                              timeout=5, max_virtual_memory=limit)
      self.assertEqual(result.exit_code, 0, result.stderr)
      self.assertEqual(json.loads(result.stdout), [limit, limit + 1])

    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
      list(pool.map(run, [512 * 1024 * 1024, 768 * 1024 * 1024] * 4))
    self.assertEqual(resource.getrlimit(resource.RLIMIT_AS), parent_limits)


if __name__ == '__main__':
  unittest.main()

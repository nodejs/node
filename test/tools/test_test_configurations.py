import os
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest import mock

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
sys.path.insert(1, os.path.join(ROOT, 'test'))
import test as runner
import testpy


class TestConfigurationTest(unittest.TestCase):
  def setUp(self):
    directory = tempfile.TemporaryDirectory()
    self.addCleanup(directory.cleanup)
    self.root = directory.name
    self.context = runner.Context(
        ROOT, False, sys.executable, [], False, 5, lambda args: args,
        False, False, 1, False)
    for filename in ['test-example.js', 'example/test.js',
                     'example/other.js', 'other/test.js']:
      path = os.path.join(self.root, filename)
      os.makedirs(os.path.dirname(path), exist_ok=True)
      with open(path, 'w', encoding='utf8') as source:
        source.write('')

  def module(self, suite):
    return runner.get_module('testcfg', os.path.join(ROOT, 'test', suite))

  def cases(self, config, suite):
    return config.ListTests([suite], runner.SplitPath(suite), 'none', 'release')

  def test_base_case_and_simple_configuration_default_to_parallel(self):
    case = runner.TestCase(self.context, ['example', 'test-example'], 'none', 'release')
    self.assertTrue(case.parallel)
    config = testpy.SimpleTestConfiguration(self.context, self.root, 'example')
    cases = self.cases(config, 'example')
    self.assertEqual(len(cases), 1)
    self.assertTrue(cases[0].parallel)

  def test_serial_configuration_explicitly_disables_parallel_execution(self):
    config = testpy.SerialTestConfiguration(self.context, self.root, 'example')
    cases = self.cases(config, 'example')
    self.assertEqual(len(cases), 1)
    self.assertFalse(cases[0].parallel)

  def test_generic_and_serial_addon_configurations_preserve_discovery(self):
    expected = [('example', 'other'), ('example', 'test'), ('other', 'test')]
    for configuration, parallel in [(testpy.AddonTestConfiguration, True),
                                    (testpy.SerialAddonTestConfiguration, False)]:
      with self.subTest(configuration=configuration.__name__):
        cases = self.cases(configuration(self.context, self.root, 'example'), 'example')
        self.assertEqual(sorted(tuple(case.path[1:]) for case in cases), expected)
        self.assertTrue(all(case.parallel == parallel for case in cases))

  def test_named_suites_explicitly_remain_serial(self):
    for suite in ['pummel', 'known_issues', 'internet',
                  'sequential',
                  'addons', 'js-native-api', 'node-api']:
      with self.subTest(suite=suite):
        config = self.module(suite).GetConfiguration(self.context, self.root)
        cases = self.cases(config, suite)
        self.assertTrue(cases)
        self.assertTrue(all(not case.parallel for case in cases))

  def test_other_suites_use_parallel_default(self):
    for suite in ['parallel', 'abort', 'async-hooks', 'benchmark', 'client-proxy', 'doctool',
                  'embedding', 'es-module', 'ffi', 'module-hooks', 'report',
                  'sqlite', 'test426', 'test-runner', 'tick-processor',
                  'trace_events', 'v8-updates', 'wasi', 'wasm-allocation']:
      with self.subTest(suite=suite):
        config = self.module(suite).GetConfiguration(self.context, self.root)
        cases = self.cases(config, suite)
        self.assertTrue(cases)
        self.assertTrue(all(case.parallel for case in cases))

  def test_known_issues_preserves_negative_context_without_changing_caller(self):
    config = self.module('known_issues').GetConfiguration(self.context, self.root)
    case = self.cases(config, 'known_issues')[0]
    self.assertFalse(self.context.expect_fail)
    self.assertTrue(case.IsNegative())
    self.assertIsNot(case.context, self.context)
    self.assertFalse(case.parallel)

  def test_abort_retains_core_dump_suppression(self):
    config = self.module('abort').GetConfiguration(self.context, self.root)
    case = self.cases(config, 'abort')[0]
    self.assertTrue(case.disable_core_files)
    self.assertTrue(case.parallel)

  def test_skipped_wpt_wrapper_retains_serial_configuration(self):
    config = self.module('wpt').GetConfiguration(self.context, self.root)
    with mock.patch.object(config, '_Discover', return_value=None):
      cases = self.cases(config, 'wpt')
    self.assertEqual(len(cases), 1)
    self.assertEqual(cases[0].path, ['wpt', 'test-example'])
    self.assertFalse(cases[0].parallel)

  def test_sea_uses_requested_jobs_for_disk_space_gate(self):
    sea = self.module('sea')
    for exists, jobs, free, parallel in [(False, 1, 1000, False),
                                        (True, 1, 299, False),
                                        (True, 1, 300, True),
                                        (True, 4, 1199, False),
                                        (True, 4, 1200, True)]:
      with self.subTest(exists=exists, jobs=jobs, free=free):
        self.context.jobs = jobs
        with mock.patch.object(sea.os.path, 'isfile', return_value=exists), \
             mock.patch.object(sea.os.path, 'getsize', return_value=100), \
             mock.patch.object(sea.shutil, 'disk_usage', return_value=SimpleNamespace(free=free)):
          config = sea.GetConfiguration(self.context, self.root)
        self.assertEqual(self.cases(config, 'sea')[0].parallel, parallel)


if __name__ == '__main__':
  unittest.main()

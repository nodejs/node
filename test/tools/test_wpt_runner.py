import contextlib
import json
import os
import shlex
import sys
import tempfile
import unittest
import warnings
from unittest import mock

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import test as runner

wpt = runner.get_module('testcfg', os.path.join(ROOT, 'test', 'wpt'))


class WPTConfigurationTest(unittest.TestCase):
  def setUp(self):
    self.stack = contextlib.ExitStack()
    self.addCleanup(self.stack.close)
    self.stack.enter_context(warnings.catch_warnings())
    # SimpleTestCase's existing source reader does not explicitly close files.
    warnings.filterwarnings('ignore', category=ResourceWarning, module='testpy',
                            message=r'unclosed file .*test-example\.js')
    self.root = self.stack.enter_context(tempfile.TemporaryDirectory())
    self.wrapper = os.path.join(self.root, 'test-example.js')
    with open(self.wrapper, 'w', encoding='utf8') as source:
      source.write('// Flags: --expose-gc\n// Env: GROUP_TEST=kept\n')
    self.context = runner.Context(
      ROOT, False, sys.executable, [], False, 5, lambda args: args,
      False, False, 1, False)
    self.config = wpt.GetConfiguration(self.context, self.root)
    self.manifest = {'version': 1, 'serial': False, 'tests': [
      {'source': 'nested/a.any.js', 'key': 'nested/a.any.html', 'id': 'nested/a.any.html',
       'selector': 'example/nested/a.any.html'},
      {'source': 'nested/a.any.js', 'key': 'nested/a.any.worker.html',
       'id': 'nested/a.any.worker.html', 'selector': 'example/nested/a.any.worker.html'},
      {'source': 'z.any.js', 'key': 'z.any.html', 'id': 'z.any.html', 'selector': 'example/z.any.html'},
    ]}
    self.discovery = self.stack.enter_context(mock.patch.object(
      runner, 'Execute', side_effect=self.discover))

  def discover(self, command, context, timeout=None, env=None, **kwargs):
    self.assertEqual(json.loads(env['NODE_TEST_WPT']), {'mode': 'list'})
    self.assertEqual(command[-1], self.wrapper)
    return runner.CommandOutput(
      0, False, wpt.MANIFEST_PREFIX + json.dumps(self.manifest) + '\n', '')

  def cases(self, selector='wpt/test-example'):
    return self.config.ListTests(['wpt'], runner.SplitPath(selector), 'none', 'release')

  def test_discovery_creates_one_case_per_status_group(self):
    cases = self.cases()
    self.assertEqual([case.group for case in cases], self.manifest['tests'])
    self.assertTrue(all(case.parallel for case in cases))
    self.assertEqual([case.GetName() for case in cases],
                     ['wpt/test-example/' + group['id'] for group in self.manifest['tests']])
    self.assertEqual(len({tuple(case.path) for case in cases}), 3)
    self.assertEqual(self.discovery.call_count, 1)

  def test_group_request_preserves_wrapper_startup_flags(self):
    self.config.additional_flags = ['--trace-warnings']
    case = self.cases('wpt/test-example/nested/a.any.worker.html')[0]
    configuration = case.GetRunConfiguration()
    self.assertEqual(configuration['command'],
                     [sys.executable, '--expose-gc', '--trace-warnings', self.wrapper])
    self.assertEqual(configuration['envs']['GROUP_TEST'], 'kept')
    self.assertEqual(json.loads(configuration['envs']['NODE_TEST_WPT']), {
      'mode': 'run', 'source': 'nested/a.any.js', 'key': 'nested/a.any.worker.html',
    })
    self.assertEqual(case.GetReportingName(configuration['command']),
                     'wpt/test-example/nested/a.any.worker.html')

  def test_group_selection_and_serial_suites(self):
    self.manifest['serial'] = True
    cases = self.cases('wpt/test-example/nested/*')
    self.assertEqual(len(cases), 2)
    self.assertTrue(all(not case.parallel for case in cases))
    selected = self.cases('wpt/test-example/nested/a.any.worker.html')
    self.assertEqual([case.group['id'] for case in selected], ['nested/a.any.worker.html'])

  def test_variant_queries_are_literal_and_base_selects_all_queries(self):
    group = self.manifest['tests'][0]
    variants = ['', '?q=[a]+(b)|c', '?q=aaab', '?q=*', '?q=source.js']
    self.manifest['tests'] = [
      {**group, 'id': group['id'] + variant, 'selector': group['selector'] + variant,
       'variant': variant} for variant in variants]
    base = 'wpt/test-example/' + group['id']
    self.assertEqual([case.group['variant'] for case in self.cases(base)], variants)
    for variant in variants[1:]:
      with self.subTest(variant=variant):
        selector = base + variant
        self.assertEqual(runner.NormalizePath(selector), selector)
        selected = self.cases(selector)
        self.assertEqual(len(selected), 1)
        case = selected[0]
        self.assertEqual(case.GetName(), selector)
        request = json.loads(case.GetRunConfiguration()['envs']['NODE_TEST_WPT'])
        self.assertEqual(request, {'mode': 'run', 'source': group['source'],
                                  'key': group['key'], 'variant': variant})
    empty = self.cases(base)[0]
    self.assertEqual(json.loads(empty.GetRunConfiguration()['envs']['NODE_TEST_WPT'])['variant'], '')

  def test_discovery_is_cached_and_uses_existing_directory(self):
    absent = os.path.join(self.root, 'not-created')
    with mock.patch.dict(os.environ, {'NODE_TEST_DIR': absent}):
      self.cases()
      self.cases('wpt/test-example/nested/*')
    self.assertEqual(self.discovery.call_count, 1)
    self.assertFalse(os.path.exists(absent))
    command, _, _, env = self.discovery.call_args.args
    self.assertIn('--expose-gc', command)
    self.assertEqual(env['NODE_TEST_DIR'], self.root)

  def test_whole_wrapper_feature_skip_keeps_original_test(self):
    self.discovery.side_effect = None
    self.discovery.return_value = runner.CommandOutput(0, False, '1..0 # Skipped: no feature\n', '')
    cases = self.cases()
    self.assertEqual(len(cases), 1)
    self.assertEqual(cases[0].path, ['wpt', 'test-example'])
    self.assertEqual(cases[0].GetRunConfiguration()['command'][-1], self.wrapper)
    self.assertNotIn('NODE_TEST_WPT', cases[0].GetRunConfiguration()['envs'])

  def test_malformed_discovery_is_an_error(self):
    self.manifest['tests'] = 'not a list'
    with self.assertRaisesRegex(RuntimeError, 'WPT discovery failed'):
      self.cases()

  def test_failure_commands_preserve_actual_command_and_selection(self):
    self.config.additional_flags = ["--title=space ' $|?", '--trace-warnings']
    query = "?q=space ' | $(echo)"
    for group in self.manifest['tests'][:2]:
      group.update(id=group['id'] + query, selector=group['selector'] + query, variant=query)
    self.manifest['tests'].append({'source': 'empty.any.js', 'key': 'empty.any.html',
                                   'id': 'empty.any.html', 'selector': 'example/empty.any.html', 'variant': ''})
    cases = self.cases()
    self.assertEqual(len(cases), 4)
    self.context.processor = lambda args: ['valgrind', '--tool=memcheck', *args, 'suffix with |']
    with mock.patch.object(sys, 'platform', 'linux'):
      for case, group in zip(cases, self.manifest['tests']):
        command = case.GetRunConfiguration()['command']
        expected = [sys.executable, '--expose-gc', "--title=space ' $|?", '--trace-warnings',
                    self.wrapper]
        self.assertEqual(command, expected)
        command = self.context.processor(command)
        rerun = ['valgrind', '--tool=memcheck', *expected, group['selector'], 'suffix with |']
        self.assertEqual(shlex.split(case.GetFailureCommand(command)), rerun)
        failure = runner.TestOutput(case, command,
                                    runner.CommandOutput(1, False, '', 'probe failure'), False)
        printer = runner.ProgressIndicator([], runner.RUN, 0)
        self.assertIn('Command: ' + shlex.join(rerun), printer.GetFailureOutput(failure))

  def test_failure_command_quotes_powershell_metacharacters(self):
    case = self.cases()[0]
    case.file = 'test driver.js'
    case.group['selector'] = "example/a.any.html?q=O'Brien|x"
    command = ['node', case.file]
    with mock.patch.object(sys, 'platform', 'win32'):
      self.assertEqual(case.GetFailureCommand(command),
                       "& 'node' 'test driver.js' 'example/a.any.html?q=O''Brien|x'")

  def test_ordinary_failure_command_is_unchanged(self):
    case = runner.TestCase(self.context, ['parallel', 'test-example'], 'none', 'release')
    command = [sys.executable, '--expose-gc', 'test/parallel/path with spaces.js']
    self.assertEqual(case.GetFailureCommand(command), runner.EscapeCommand(command))


if __name__ == '__main__':
  unittest.main()

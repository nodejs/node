#!/usr/bin/env python
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Global system tests for V8 test runners and fuzzers.

This hooks up the framework under tools/testrunner testing high-level scenarios
with different test suite extensions and build configurations.
"""

# TODO(machenbach): Mock out util.GuessOS to make these tests really platform
# independent.
# TODO(machenbach): Move coverage recording to a global test entry point to
# include other unittest suites in the coverage report.
# TODO(machenbach): Coverage data from multiprocessing doesn't work.
# TODO(majeski): Add some tests for the fuzzers.

import collections
import contextlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

from cStringIO import StringIO

TOOLS_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEST_DATA_ROOT = os.path.join(TOOLS_ROOT, 'unittests', 'testdata')
RUN_TESTS_PY = os.path.join(TOOLS_ROOT, 'run-tests.py')

Result = collections.namedtuple(
    'Result', ['stdout', 'stderr', 'returncode'])

Result.__str__ = lambda self: (
    '\nReturncode: %s\nStdout:\n%s\nStderr:\n%s\n' %
    (self.returncode, self.stdout, self.stderr))


@contextlib.contextmanager
def temp_dir():
  """Wrapper making a temporary directory available."""
  path = None
  try:
    path = tempfile.mkdtemp('v8_test_')
    yield path
  finally:
    if path:
      shutil.rmtree(path)


@contextlib.contextmanager
def temp_base(baseroot='testroot1'):
  """Wrapper that sets up a temporary V8 test root.

  Args:
    baseroot: The folder with the test root blueprint. Relevant files will be
        copied to the temporary test root, to guarantee a fresh setup with no
        dirty state.
  """
  basedir = os.path.join(TEST_DATA_ROOT, baseroot)
  with temp_dir() as tempbase:
    builddir = os.path.join(tempbase, 'out', 'Release')
    testroot = os.path.join(tempbase, 'test')
    os.makedirs(builddir)
    shutil.copy(os.path.join(basedir, 'v8_build_config.json'), builddir)
    shutil.copy(os.path.join(basedir, 'd8_mocked.py'), builddir)

    for suite in os.listdir(os.path.join(basedir, 'test')):
      os.makedirs(os.path.join(testroot, suite))
      for entry in os.listdir(os.path.join(basedir, 'test', suite)):
        shutil.copy(
            os.path.join(basedir, 'test', suite, entry),
            os.path.join(testroot, suite))
    yield tempbase


@contextlib.contextmanager
def capture():
  """Wrapper that replaces system stdout/stderr an provides the streams."""
  oldout = sys.stdout
  olderr = sys.stderr
  try:
    stdout=StringIO()
    stderr=StringIO()
    sys.stdout = stdout
    sys.stderr = stderr
    yield stdout, stderr
  finally:
    sys.stdout = oldout
    sys.stderr = olderr


def run_tests(basedir, *args, **kwargs):
  """Executes the test runner with captured output."""
  with capture() as (stdout, stderr):
    sys_args = ['--command-prefix', sys.executable] + list(args)
    if kwargs.get('infra_staging', False):
      sys_args.append('--infra-staging')
    else:
      sys_args.append('--no-infra-staging')
    code = standard_runner.StandardTestRunner(basedir=basedir).execute(sys_args)
    return Result(stdout.getvalue(), stderr.getvalue(), code)


def override_build_config(basedir, **kwargs):
  """Override the build config with new values provided as kwargs."""
  path = os.path.join(basedir, 'out', 'Release', 'v8_build_config.json')
  with open(path) as f:
    config = json.load(f)
    config.update(kwargs)
  with open(path, 'w') as f:
    json.dump(config, f)


class SystemTest(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    # Try to set up python coverage and run without it if not available.
    cls._cov = None
    try:
      import coverage
      if int(coverage.__version__.split('.')[0]) < 4:
        cls._cov = None
        print 'Python coverage version >= 4 required.'
        raise ImportError()
      cls._cov = coverage.Coverage(
          source=([os.path.join(TOOLS_ROOT, 'testrunner')]),
          omit=['*unittest*', '*__init__.py'],
      )
      cls._cov.exclude('raise NotImplementedError')
      cls._cov.exclude('if __name__ == .__main__.:')
      cls._cov.exclude('except TestRunnerError:')
      cls._cov.exclude('except KeyboardInterrupt:')
      cls._cov.exclude('if options.verbose:')
      cls._cov.exclude('if verbose:')
      cls._cov.exclude('pass')
      cls._cov.exclude('assert False')
      cls._cov.start()
    except ImportError:
      print 'Running without python coverage.'
    sys.path.append(TOOLS_ROOT)
    global standard_runner
    from testrunner import standard_runner
    from testrunner.local import command
    from testrunner.local import pool
    command.setup_testing()
    pool.setup_testing()

  @classmethod
  def tearDownClass(cls):
    if cls._cov:
      cls._cov.stop()
      print ''
      print cls._cov.report(show_missing=True)

  def testPass(self):
    """Test running only passing tests in two variants.

    Also test printing durations.
    """
    with temp_base() as basedir:
      result = run_tests(
          basedir,
          '--mode=Release',
          '--progress=verbose',
          '--variants=default,stress',
          '--time',
          'sweet/bananas',
          'sweet/raspberries',
      )
      self.assertIn('Done running sweet/bananas: pass', result.stdout, result)
      # TODO(majeski): Implement for test processors
      # self.assertIn('Total time:', result.stderr, result)
      # self.assertIn('sweet/bananas', result.stderr, result)
      self.assertEqual(0, result.returncode, result)

  def testShardedProc(self):
    with temp_base() as basedir:
      for shard in [1, 2]:
        result = run_tests(
            basedir,
            '--mode=Release',
            '--progress=verbose',
            '--variants=default,stress',
            '--shard-count=2',
            '--shard-run=%d' % shard,
            'sweet/bananas',
            'sweet/raspberries',
            infra_staging=False,
        )
        # One of the shards gets one variant of each test.
        self.assertIn('2 tests ran', result.stdout, result)
        if shard == 1:
          self.assertIn('Done running sweet/bananas', result.stdout, result)
        else:
          self.assertIn('Done running sweet/raspberries', result.stdout, result)
        self.assertEqual(0, result.returncode, result)

  @unittest.skip("incompatible with test processors")
  def testSharded(self):
    """Test running a particular shard."""
    with temp_base() as basedir:
      for shard in [1, 2]:
        result = run_tests(
            basedir,
            '--mode=Release',
            '--progress=verbose',
            '--variants=default,stress',
            '--shard-count=2',
            '--shard-run=%d' % shard,
            'sweet/bananas',
            'sweet/raspberries',
        )
        # One of the shards gets one variant of each test.
        self.assertIn('Running 2 tests', result.stdout, result)
        self.assertIn('Done running sweet/bananas', result.stdout, result)
        self.assertIn('Done running sweet/raspberries', result.stdout, result)
        self.assertEqual(0, result.returncode, result)

  def testFail(self):
    """Test running only failing tests in two variants."""
    with temp_base() as basedir:
      result = run_tests(
          basedir,
          '--mode=Release',
          '--progress=verbose',
          '--variants=default,stress',
          'sweet/strawberries',
          infra_staging=False,
      )
      self.assertIn('Done running sweet/strawberries: FAIL', result.stdout, result)
      self.assertEqual(1, result.returncode, result)

  def check_cleaned_json_output(
      self, expected_results_name, actual_json, basedir):
    # Check relevant properties of the json output.
    with open(actual_json) as f:
      json_output = json.load(f)[0]
      pretty_json = json.dumps(json_output, indent=2, sort_keys=True)

    # Replace duration in actual output as it's non-deterministic. Also
    # replace the python executable prefix as it has a different absolute
    # path dependent on where this runs.
    def replace_variable_data(data):
      data['duration'] = 1
      data['command'] = ' '.join(
          ['/usr/bin/python'] + data['command'].split()[1:])
      data['command'] = data['command'].replace(basedir + '/', '')
    for data in json_output['slowest_tests']:
      replace_variable_data(data)
    for data in json_output['results']:
      replace_variable_data(data)
    json_output['duration_mean'] = 1

    with open(os.path.join(TEST_DATA_ROOT, expected_results_name)) as f:
      expected_test_results = json.load(f)

    msg = None  # Set to pretty_json for bootstrapping.
    self.assertDictEqual(json_output, expected_test_results, msg)

  def testFailWithRerunAndJSON(self):
    """Test re-running a failing test and output to json."""
    with temp_base() as basedir:
      json_path = os.path.join(basedir, 'out.json')
      result = run_tests(
          basedir,
          '--mode=Release',
          '--progress=verbose',
          '--variants=default',
          '--rerun-failures-count=2',
          '--random-seed=123',
          '--json-test-results', json_path,
          'sweet/strawberries',
          infra_staging=False,
      )
      self.assertIn('Done running sweet/strawberries: FAIL', result.stdout, result)
      # With test processors we don't count reruns as separated failures.
      # TODO(majeski): fix it?
      self.assertIn('1 tests failed', result.stdout, result)
      self.assertEqual(0, result.returncode, result)

      # TODO(majeski): Previously we only reported the variant flags in the
      # flags field of the test result.
      # After recent changes we report all flags, including the file names.
      # This is redundant to the command. Needs investigation.
      self.maxDiff = None
      self.check_cleaned_json_output(
          'expected_test_results1.json', json_path, basedir)

  def testFlakeWithRerunAndJSON(self):
    """Test re-running a failing test and output to json."""
    with temp_base(baseroot='testroot2') as basedir:
      json_path = os.path.join(basedir, 'out.json')
      result = run_tests(
          basedir,
          '--mode=Release',
          '--progress=verbose',
          '--variants=default',
          '--rerun-failures-count=2',
          '--random-seed=123',
          '--json-test-results', json_path,
          'sweet',
          infra_staging=False,
      )
      self.assertIn(
        'Done running sweet/bananaflakes: pass', result.stdout, result)
      self.assertIn('All tests succeeded', result.stdout, result)
      self.assertEqual(0, result.returncode, result)
      self.maxDiff = None
      self.check_cleaned_json_output(
          'expected_test_results2.json', json_path, basedir)

  def testAutoDetect(self):
    """Fake a build with several auto-detected options.

    Using all those options at once doesn't really make much sense. This is
    merely for getting coverage.
    """
    with temp_base() as basedir:
      override_build_config(
          basedir, dcheck_always_on=True, is_asan=True, is_cfi=True,
          is_msan=True, is_tsan=True, is_ubsan_vptr=True, target_cpu='x86',
          v8_enable_i18n_support=False, v8_target_cpu='x86',
          v8_use_snapshot=False, v8_enable_embedded_builtins=False,
          v8_enable_verify_csa=False, v8_enable_lite_mode=False,
          v8_enable_pointer_compression=False)
      result = run_tests(
          basedir,
          '--mode=Release',
          '--progress=verbose',
          '--variants=default',
          'sweet/bananas',
      )
      expect_text = (
          '>>> Autodetected:\n'
          'asan\n'
          'cfi_vptr\n'
          'dcheck_always_on\n'
          'msan\n'
          'no_i18n\n'
          'no_snap\n'
          'tsan\n'
          'ubsan_vptr\n'
          '>>> Running tests for ia32.release')
      self.assertIn(expect_text, result.stdout, result)
      self.assertEqual(0, result.returncode, result)
      # TODO(machenbach): Test some more implications of the auto-detected
      # options, e.g. that the right env variables are set.

  def testSkips(self):
    """Test skipping tests in status file for a specific variant."""
    with temp_base() as basedir:
      result = run_tests(
          basedir,
          '--mode=Release',
          '--progress=verbose',
          '--variants=nooptimization',
          'sweet/strawberries',
          infra_staging=False,
      )
      self.assertIn('0 tests ran', result.stdout, result)
      self.assertEqual(2, result.returncode, result)

  def testRunSkips(self):
    """Inverse the above. Test parameter to keep running skipped tests."""
    with temp_base() as basedir:
      result = run_tests(
          basedir,
          '--mode=Release',
          '--progress=verbose',
          '--variants=nooptimization',
          '--run-skipped',
          'sweet/strawberries',
      )
      self.assertIn('1 tests failed', result.stdout, result)
      self.assertIn('1 tests ran', result.stdout, result)
      self.assertEqual(1, result.returncode, result)

  def testDefault(self):
    """Test using default test suites, though no tests are run since they don't
    exist in a test setting.
    """
    with temp_base() as basedir:
      result = run_tests(
          basedir,
          '--mode=Release',
          infra_staging=False,
      )
      self.assertIn('0 tests ran', result.stdout, result)
      self.assertEqual(2, result.returncode, result)

  def testNoBuildConfig(self):
    """Test failing run when build config is not found."""
    with temp_base() as basedir:
      result = run_tests(basedir)
      self.assertIn('Failed to load build config', result.stdout, result)
      self.assertEqual(5, result.returncode, result)

  def testGNOption(self):
    """Test using gn option, but no gn build folder is found."""
    with temp_base() as basedir:
      # TODO(machenbach): This should fail gracefully.
      with self.assertRaises(OSError):
        run_tests(basedir, '--gn')

  def testInconsistentMode(self):
    """Test failing run when attempting to wrongly override the mode."""
    with temp_base() as basedir:
      override_build_config(basedir, is_debug=True)
      result = run_tests(basedir, '--mode=Release')
      self.assertIn('execution mode (release) for release is inconsistent '
                    'with build config (debug)', result.stdout, result)
      self.assertEqual(5, result.returncode, result)

  def testInconsistentArch(self):
    """Test failing run when attempting to wrongly override the arch."""
    with temp_base() as basedir:
      result = run_tests(basedir, '--mode=Release', '--arch=ia32')
      self.assertIn(
          '--arch value (ia32) inconsistent with build config (x64).',
          result.stdout, result)
      self.assertEqual(5, result.returncode, result)

  def testWrongVariant(self):
    """Test using a bogus variant."""
    with temp_base() as basedir:
      result = run_tests(basedir, '--mode=Release', '--variants=meh')
      self.assertEqual(5, result.returncode, result)

  def testModeFromBuildConfig(self):
    """Test auto-detection of mode from build config."""
    with temp_base() as basedir:
      result = run_tests(basedir, '--outdir=out/Release', 'sweet/bananas')
      self.assertIn('Running tests for x64.release', result.stdout, result)
      self.assertEqual(0, result.returncode, result)

  @unittest.skip("not available with test processors")
  def testReport(self):
    """Test the report feature.

    This also exercises various paths in statusfile logic.
    """
    with temp_base() as basedir:
      result = run_tests(
          basedir,
          '--mode=Release',
          '--variants=default',
          'sweet',
          '--report',
      )
      self.assertIn(
          '3 tests are expected to fail that we should fix',
          result.stdout, result)
      self.assertEqual(1, result.returncode, result)

  @unittest.skip("not available with test processors")
  def testWarnUnusedRules(self):
    """Test the unused-rules feature."""
    with temp_base() as basedir:
      result = run_tests(
          basedir,
          '--mode=Release',
          '--variants=default,nooptimization',
          'sweet',
          '--warn-unused',
      )
      self.assertIn( 'Unused rule: carrots', result.stdout, result)
      self.assertIn( 'Unused rule: regress/', result.stdout, result)
      self.assertEqual(1, result.returncode, result)

  @unittest.skip("not available with test processors")
  def testCatNoSources(self):
    """Test printing sources, but the suite's tests have none available."""
    with temp_base() as basedir:
      result = run_tests(
          basedir,
          '--mode=Release',
          '--variants=default',
          'sweet/bananas',
          '--cat',
      )
      self.assertIn('begin source: sweet/bananas', result.stdout, result)
      self.assertIn('(no source available)', result.stdout, result)
      self.assertEqual(0, result.returncode, result)

  def testPredictable(self):
    """Test running a test in verify-predictable mode.

    The test will fail because of missing allocation output. We verify that and
    that the predictable flags are passed and printed after failure.
    """
    with temp_base() as basedir:
      override_build_config(basedir, v8_enable_verify_predictable=True)
      result = run_tests(
          basedir,
          '--mode=Release',
          '--progress=verbose',
          '--variants=default',
          'sweet/bananas',
          infra_staging=False,
      )
      self.assertIn('1 tests ran', result.stdout, result)
      self.assertIn('Done running sweet/bananas: FAIL', result.stdout, result)
      self.assertIn('Test had no allocation output', result.stdout, result)
      self.assertIn('--predictable --verify_predictable', result.stdout, result)
      self.assertEqual(1, result.returncode, result)

  def testSlowArch(self):
    """Test timeout factor manipulation on slow architecture."""
    with temp_base() as basedir:
      override_build_config(basedir, v8_target_cpu='arm64')
      result = run_tests(
          basedir,
          '--mode=Release',
          '--progress=verbose',
          '--variants=default',
          'sweet/bananas',
      )
      # TODO(machenbach): We don't have a way for testing if the correct
      # timeout was used.
      self.assertEqual(0, result.returncode, result)

  def testRandomSeedStressWithDefault(self):
    """Test using random-seed-stress feature has the right number of tests."""
    with temp_base() as basedir:
      result = run_tests(
          basedir,
          '--mode=Release',
          '--progress=verbose',
          '--variants=default',
          '--random-seed-stress-count=2',
          'sweet/bananas',
          infra_staging=False,
      )
      self.assertIn('2 tests ran', result.stdout, result)
      self.assertEqual(0, result.returncode, result)

  def testRandomSeedStressWithSeed(self):
    """Test using random-seed-stress feature passing a random seed."""
    with temp_base() as basedir:
      result = run_tests(
          basedir,
          '--mode=Release',
          '--progress=verbose',
          '--variants=default',
          '--random-seed-stress-count=2',
          '--random-seed=123',
          'sweet/strawberries',
      )
      self.assertIn('2 tests ran', result.stdout, result)
      # We use a failing test so that the command is printed and we can verify
      # that the right random seed was passed.
      self.assertIn('--random-seed=123', result.stdout, result)
      self.assertEqual(1, result.returncode, result)

  def testSpecificVariants(self):
    """Test using NO_VARIANTS modifiers in status files skips the desire tests.

    The test runner cmd line configures 4 tests to run (2 tests * 2 variants).
    But the status file applies a modifier to each skipping one of the
    variants.
    """
    with temp_base() as basedir:
      override_build_config(basedir, v8_use_snapshot=False)
      result = run_tests(
          basedir,
          '--mode=Release',
          '--progress=verbose',
          '--variants=default,stress',
          'sweet/bananas',
          'sweet/raspberries',
      )
      # Both tests are either marked as running in only default or only
      # slow variant.
      self.assertIn('2 tests ran', result.stdout, result)
      self.assertEqual(0, result.returncode, result)

  def testStatusFilePresubmit(self):
    """Test that the fake status file is well-formed."""
    with temp_base() as basedir:
      from testrunner.local import statusfile
      self.assertTrue(statusfile.PresubmitCheck(
          os.path.join(basedir, 'test', 'sweet', 'sweet.status')))

  def testDotsProgress(self):
    with temp_base() as basedir:
      result = run_tests(
          basedir,
          '--mode=Release',
          '--progress=dots',
          'sweet/cherries',
          'sweet/bananas',
          '--no-sorting', '-j1', # make results order deterministic
          infra_staging=False,
      )
      self.assertIn('2 tests ran', result.stdout, result)
      self.assertIn('F.', result.stdout, result)
      self.assertEqual(1, result.returncode, result)

  def testMonoProgress(self):
    self._testCompactProgress('mono')

  def testColorProgress(self):
    self._testCompactProgress('color')

  def _testCompactProgress(self, name):
    with temp_base() as basedir:
      result = run_tests(
          basedir,
          '--mode=Release',
          '--progress=%s' % name,
          'sweet/cherries',
          'sweet/bananas',
          infra_staging=False,
      )
      if name == 'color':
        expected = ('\033[32m+   1\033[0m|'
                    '\033[31m-   1\033[0m]: Done')
      else:
        expected = '+   1|-   1]: Done'
      self.assertIn(expected, result.stdout)
      self.assertIn('sweet/cherries', result.stdout)
      self.assertIn('sweet/bananas', result.stdout)
      self.assertEqual(1, result.returncode, result)

  def testExitAfterNFailures(self):
    with temp_base() as basedir:
      result = run_tests(
          basedir,
          '--mode=Release',
          '--progress=verbose',
          '--exit-after-n-failures=2',
          '-j1',
          'sweet/mangoes',       # PASS
          'sweet/strawberries',  # FAIL
          'sweet/blackberries',  # FAIL
          'sweet/raspberries',   # should not run
      )
      self.assertIn('sweet/mangoes: pass', result.stdout, result)
      self.assertIn('sweet/strawberries: FAIL', result.stdout, result)
      self.assertIn('Too many failures, exiting...', result.stdout, result)
      self.assertIn('sweet/blackberries: FAIL', result.stdout, result)
      self.assertNotIn('Done running sweet/raspberries', result.stdout, result)
      self.assertIn('2 tests failed', result.stdout, result)
      self.assertIn('3 tests ran', result.stdout, result)
      self.assertEqual(1, result.returncode, result)

if __name__ == '__main__':
  unittest.main()

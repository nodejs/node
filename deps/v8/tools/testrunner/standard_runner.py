#!/usr/bin/env python
#
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# for py2/py3 compatibility
from __future__ import print_function
from functools import reduce

import datetime
import json
import os
import sys
import tempfile

# Adds testrunner to the path hence it has to be imported at the beggining.
import base_runner

from testrunner.local import utils
from testrunner.local.variants import ALL_VARIANTS
from testrunner.objects import predictable
from testrunner.testproc.execution import ExecutionProc
from testrunner.testproc.filter import StatusFileFilterProc, NameFilterProc
from testrunner.testproc.loader import LoadProc
from testrunner.testproc.seed import SeedProc
from testrunner.testproc.variant import VariantProc


ARCH_GUESS = utils.DefaultArch()

VARIANTS = ['default']

MORE_VARIANTS = [
  'jitless',
  'stress',
  'stress_js_bg_compile_wasm_code_gc',
  'stress_incremental_marking',
]

VARIANT_ALIASES = {
  # The default for developer workstations.
  'dev': VARIANTS,
  # Additional variants, run on all bots.
  'more': MORE_VARIANTS,
  # Shortcut for the two above ('more' first - it has the longer running tests)
  'exhaustive': MORE_VARIANTS + VARIANTS,
  # Additional variants, run on a subset of bots.
  'extra': ['nooptimization', 'future', 'no_wasm_traps', 'turboprop'],
}

GC_STRESS_FLAGS = ['--gc-interval=500', '--stress-compaction',
                   '--concurrent-recompilation-queue-length=64',
                   '--concurrent-recompilation-delay=500',
                   '--concurrent-recompilation',
                   '--stress-flush-bytecode',
                   '--wasm-code-gc', '--stress-wasm-code-gc']

RANDOM_GC_STRESS_FLAGS = ['--random-gc-interval=5000',
                          '--stress-compaction-random']


PREDICTABLE_WRAPPER = os.path.join(
    base_runner.BASE_DIR, 'tools', 'predictable_wrapper.py')


class StandardTestRunner(base_runner.BaseTestRunner):
  def __init__(self, *args, **kwargs):
    super(StandardTestRunner, self).__init__(*args, **kwargs)

    self.sancov_dir = None
    self._variants = None

  @property
  def framework_name(self):
    return 'standard_runner'

  def _get_default_suite_names(self):
    return ['default']

  def _add_parser_options(self, parser):
    parser.add_option('--novfp3',
                      help='Indicates that V8 was compiled without VFP3'
                      ' support',
                      default=False, action='store_true')

    # Variants
    parser.add_option('--no-variants', '--novariants',
                      help='Deprecated. '
                           'Equivalent to passing --variants=default',
                      default=False, dest='no_variants', action='store_true')
    parser.add_option('--variants',
                      help='Comma-separated list of testing variants;'
                      ' default: "%s"' % ','.join(VARIANTS))
    parser.add_option('--exhaustive-variants',
                      default=False, action='store_true',
                      help='Deprecated. '
                           'Equivalent to passing --variants=exhaustive')

    # Filters
    parser.add_option('--slow-tests', default='dontcare',
                      help='Regard slow tests (run|skip|dontcare)')
    parser.add_option('--pass-fail-tests', default='dontcare',
                      help='Regard pass|fail tests (run|skip|dontcare)')
    parser.add_option('--quickcheck', default=False, action='store_true',
                      help=('Quick check mode (skip slow tests)'))
    parser.add_option('--dont-skip-slow-simulator-tests',
                      help='Don\'t skip more slow tests when using a'
                      ' simulator.',
                      default=False, action='store_true',
                      dest='dont_skip_simulator_slow_tests')

    # Stress modes
    parser.add_option('--gc-stress',
                      help='Switch on GC stress mode',
                      default=False, action='store_true')
    parser.add_option('--random-gc-stress',
                      help='Switch on random GC stress mode',
                      default=False, action='store_true')
    parser.add_option('--random-seed-stress-count', default=1, type='int',
                      dest='random_seed_stress_count',
                      help='Number of runs with different random seeds. Only '
                           'with test processors: 0 means infinite '
                           'generation.')

    # Extra features.
    parser.add_option('--time', help='Print timing information after running',
                      default=False, action='store_true')

    # Noop
    parser.add_option('--cfi-vptr',
                      help='Run tests with UBSAN cfi_vptr option.',
                      default=False, action='store_true')
    parser.add_option('--infra-staging', help='Use new test runner features',
                      dest='infra_staging', default=None,
                      action='store_true')
    parser.add_option('--no-infra-staging',
                      help='Opt out of new test runner features',
                      dest='infra_staging', default=None,
                      action='store_false')
    parser.add_option('--no-sorting', '--nosorting',
                      help='Don\'t sort tests according to duration of last'
                      ' run.',
                      default=False, dest='no_sorting', action='store_true')
    parser.add_option('--no-presubmit', '--nopresubmit',
                      help='Skip presubmit checks (deprecated)',
                      default=False, dest='no_presubmit', action='store_true')

    # Unimplemented for test processors
    parser.add_option('--sancov-dir',
                      help='Directory where to collect coverage data')
    parser.add_option('--cat', help='Print the source of the tests',
                      default=False, action='store_true')
    parser.add_option('--flakiness-results',
                      help='Path to a file for storing flakiness json.')
    parser.add_option('--warn-unused', help='Report unused rules',
                      default=False, action='store_true')
    parser.add_option('--report', default=False, action='store_true',
                      help='Print a summary of the tests to be run')

  def _process_options(self, options):
    if options.sancov_dir:
      self.sancov_dir = options.sancov_dir
      if not os.path.exists(self.sancov_dir):
        print('sancov-dir %s doesn\'t exist' % self.sancov_dir)
        raise base_runner.TestRunnerError()

    if options.gc_stress:
      options.extra_flags += GC_STRESS_FLAGS

    if options.random_gc_stress:
      options.extra_flags += RANDOM_GC_STRESS_FLAGS

    if self.build_config.asan:
      options.extra_flags.append('--invoke-weak-callbacks')

    if self.build_config.no_snap:
      # Speed up slow nosnap runs. Allocation verification is covered by
      # running mksnapshot on other builders.
      options.extra_flags.append('--no-turbo-verify-allocation')

    if options.novfp3:
      options.extra_flags.append('--noenable-vfp3')

    if options.no_variants:  # pragma: no cover
      print ('Option --no-variants is deprecated. '
             'Pass --variants=default instead.')
      assert not options.variants
      options.variants = 'default'

    if options.exhaustive_variants:  # pragma: no cover
      # TODO(machenbach): Switch infra to --variants=exhaustive after M65.
      print ('Option --exhaustive-variants is deprecated. '
             'Pass --variants=exhaustive instead.')
      # This is used on many bots. It includes a larger set of default
      # variants.
      # Other options for manipulating variants still apply afterwards.
      assert not options.variants
      options.variants = 'exhaustive'

    if options.quickcheck:
      assert not options.variants
      options.variants = 'stress,default'
      options.slow_tests = 'skip'
      options.pass_fail_tests = 'skip'

    if self.build_config.predictable:
      options.variants = 'default'
      options.extra_flags.append('--predictable')
      options.extra_flags.append('--verify-predictable')
      options.extra_flags.append('--no-inline-new')
      # Add predictable wrapper to command prefix.
      options.command_prefix = (
          [sys.executable, PREDICTABLE_WRAPPER] + options.command_prefix)

    # TODO(machenbach): Figure out how to test a bigger subset of variants on
    # msan.
    if self.build_config.msan:
      options.variants = 'default'

    if options.variants == 'infra_staging':
      options.variants = 'exhaustive'

    self._variants = self._parse_variants(options.variants)

    def CheckTestMode(name, option):  # pragma: no cover
      if option not in ['run', 'skip', 'dontcare']:
        print('Unknown %s mode %s' % (name, option))
        raise base_runner.TestRunnerError()
    CheckTestMode('slow test', options.slow_tests)
    CheckTestMode('pass|fail test', options.pass_fail_tests)
    if self.build_config.no_i18n:
      base_runner.TEST_MAP['bot_default'].remove('intl')
      base_runner.TEST_MAP['default'].remove('intl')
      # TODO(machenbach): uncomment after infra side lands.
      # base_runner.TEST_MAP['d8_default'].remove('intl')

    if options.time and not options.json_test_results:
      # We retrieve the slowest tests from the JSON output file, so create
      # a temporary output file (which will automatically get deleted on exit)
      # if the user didn't specify one.
      self._temporary_json_output_file = tempfile.NamedTemporaryFile(
          prefix="v8-test-runner-")
      options.json_test_results = self._temporary_json_output_file.name

  def _parse_variants(self, aliases_str):
    # Use developer defaults if no variant was specified.
    aliases_str = aliases_str or 'dev'
    aliases = aliases_str.split(',')
    user_variants = set(reduce(
        list.__add__, [VARIANT_ALIASES.get(a, [a]) for a in aliases]))

    result = [v for v in ALL_VARIANTS if v in user_variants]
    if len(result) == len(user_variants):
      return result

    for v in user_variants:
      if v not in ALL_VARIANTS:
        print('Unknown variant: %s' % v)
        raise base_runner.TestRunnerError()
    assert False, 'Unreachable'

  def _setup_env(self):
    super(StandardTestRunner, self)._setup_env()

    symbolizer_option = self._get_external_symbolizer_option()

    if self.sancov_dir:
      os.environ['ASAN_OPTIONS'] = ':'.join([
        'coverage=1',
        'coverage_dir=%s' % self.sancov_dir,
        symbolizer_option,
        'allow_user_segv_handler=1',
      ])

  def _get_statusfile_variables(self, options):
    variables = (
        super(StandardTestRunner, self)._get_statusfile_variables(options))

    simulator_run = (
      not options.dont_skip_simulator_slow_tests and
      self.build_config.arch in [
        'arm64', 'arm', 'mipsel', 'mips', 'mips64', 'mips64el', 'ppc',
        'ppc64', 's390', 's390x'] and
      bool(ARCH_GUESS) and
      self.build_config.arch != ARCH_GUESS)

    variables.update({
      'gc_stress': options.gc_stress or options.random_gc_stress,
      'gc_fuzzer': options.random_gc_stress,
      'novfp3': options.novfp3,
      'simulator_run': simulator_run,
    })
    return variables

  def _do_execute(self, tests, args, options):
    jobs = options.j

    print('>>> Running with test processors')
    loader = LoadProc(tests)
    results = self._create_result_tracker(options)
    indicators = self._create_progress_indicators(
        tests.test_count_estimate, options)

    outproc_factory = None
    if self.build_config.predictable:
      outproc_factory = predictable.get_outproc
    execproc = ExecutionProc(jobs, outproc_factory)
    sigproc = self._create_signal_proc()

    procs = [
      loader,
      NameFilterProc(args) if args else None,
      StatusFileFilterProc(options.slow_tests, options.pass_fail_tests),
      VariantProc(self._variants),
      StatusFileFilterProc(options.slow_tests, options.pass_fail_tests),
      self._create_predictable_filter(),
      self._create_shard_proc(options),
      self._create_seed_proc(options),
      sigproc,
    ] + indicators + [
      results,
      self._create_timeout_proc(options),
      self._create_rerun_proc(options),
      execproc,
    ]

    self._prepare_procs(procs)

    loader.load_initial_tests(initial_batch_size=options.j * 2)

    # This starts up worker processes and blocks until all tests are
    # processed.
    execproc.run()

    for indicator in indicators:
      indicator.finished()

    if tests.test_count_estimate:
      percentage = float(results.total) / tests.test_count_estimate * 100
    else:
      percentage = 0

    print (('>>> %d base tests produced %d (%d%s)'
           ' non-filtered tests') % (
        tests.test_count_estimate, results.total, percentage, '%'))

    print('>>> %d tests ran' % (results.total - results.remaining))

    exit_code = utils.EXIT_CODE_PASS
    if results.failed:
      exit_code = utils.EXIT_CODE_FAILURES
    if not results.total:
      exit_code = utils.EXIT_CODE_NO_TESTS

    if options.time:
      self._print_durations(options)

    # Indicate if a SIGINT or SIGTERM happened.
    return max(exit_code, sigproc.exit_code)

  def _print_durations(self, options):

    def format_duration(duration_in_seconds):
      duration = datetime.timedelta(seconds=duration_in_seconds)
      time = (datetime.datetime.min + duration).time()
      return time.strftime('%M:%S:') + '%03i' % int(time.microsecond / 1000)

    def _duration_results_text(test):
      return [
        'Test: %s' % test['name'],
        'Flags: %s' % ' '.join(test['flags']),
        'Command: %s' % test['command'],
        'Duration: %s' % format_duration(test['duration']),
      ]

    assert os.path.exists(options.json_test_results)
    complete_results = []
    with open(options.json_test_results, "r") as f:
      complete_results = json.loads(f.read())
    output = complete_results[0]
    lines = []
    for test in output['slowest_tests']:
      suffix = ''
      if test.get('marked_slow') is False:
        suffix = ' *'
      lines.append(
          '%s %s%s' % (format_duration(test['duration']),
                       test['name'], suffix))

    # Slowest tests duration details.
    lines.extend(['', 'Details:', ''])
    for test in output['slowest_tests']:
      lines.extend(_duration_results_text(test))
    print("\n".join(lines))

  def _create_predictable_filter(self):
    if not self.build_config.predictable:
      return None
    return predictable.PredictableFilterProc()

  def _create_seed_proc(self, options):
    if options.random_seed_stress_count == 1:
      return None
    return SeedProc(options.random_seed_stress_count, options.random_seed,
                    options.j * 4)


if __name__ == '__main__':
  sys.exit(StandardTestRunner().execute())

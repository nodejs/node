#!/usr/bin/env python3
#
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import random
import sys

# Adds testrunner to the path hence it has to be imported at the beggining.
from . import base_runner

from testrunner.local import utils

from testrunner.testproc import fuzzer
from testrunner.testproc.base import TestProcProducer
from testrunner.testproc.combiner import CombinerProc
from testrunner.testproc.execution import ExecutionProc
from testrunner.testproc.expectation import ExpectationProc
from testrunner.testproc.filter import StatusFileFilterProc, NameFilterProc
from testrunner.testproc.loader import LoadProc
from testrunner.testproc.progress import ResultsTracker
from testrunner.utils import random_utils


DEFAULT_SUITES = ["mjsunit", "webkit", "benchmarks"]


class NumFuzzer(base_runner.BaseTestRunner):
  def __init__(self, *args, **kwargs):
    super(NumFuzzer, self).__init__(*args, **kwargs)

  @property
  def framework_name(self):
    return 'num_fuzzer'

  def _add_parser_options(self, parser):
    parser.add_option("--fuzzer-random-seed", default=0,
                      help="Default seed for initializing fuzzer random "
                      "generator")
    parser.add_option("--tests-count", default=5, type="int",
                      help="Number of tests to generate from each base test. "
                           "Can be combined with --total-timeout-sec with "
                           "value 0 to provide infinite number of subtests. "
                           "When --combine-tests is set it indicates how many "
                           "tests to create in total")

    # Stress gc
    parser.add_option("--stress-marking", default=0, type="int",
                      help="probability [0-10] of adding --stress-marking "
                           "flag to the test")
    parser.add_option("--stress-scavenge", default=0, type="int",
                      help="probability [0-10] of adding --stress-scavenge "
                           "flag to the test")
    parser.add_option("--stress-compaction", default=0, type="int",
                      help="probability [0-10] of adding --stress-compaction "
                           "flag to the test")
    parser.add_option("--stress-gc", default=0, type="int",
                      help="probability [0-10] of adding --random-gc-interval "
                           "flag to the test")

    # Stress stack size
    parser.add_option("--stress-stack-size", default=0, type="int",
                      help="probability [0-10] of adding --stack-size "
                           "flag to the test")

    # Stress tasks
    parser.add_option("--stress-delay-tasks", default=0, type="int",
                      help="probability [0-10] of adding --stress-delay-tasks "
                           "flag to the test")
    parser.add_option("--stress-thread-pool-size", default=0, type="int",
                      help="probability [0-10] of adding --thread-pool-size "
                           "flag to the test")

    # Stress compiler
    parser.add_option("--stress-deopt", default=0, type="int",
                      help="probability [0-10] of adding --deopt-every-n-times "
                           "flag to the test")
    parser.add_option("--stress-deopt-min", default=1, type="int",
                      help="extends --stress-deopt to have minimum interval "
                           "between deopt points")
    parser.add_option("--stress-interrupt-budget", default=0, type="int",
                      help="probability [0-10] of adding the --interrupt-budget "
                           "flag to the test")

    # Combine multiple tests
    parser.add_option("--combine-tests", default=False, action="store_true",
                      help="Combine multiple tests as one and run with "
                           "try-catch wrapper")
    parser.add_option("--combine-max", default=100, type="int",
                      help="Maximum number of tests to combine")
    parser.add_option("--combine-min", default=2, type="int",
                      help="Minimum number of tests to combine")

    # Miscellaneous
    parser.add_option("--variants", default='default',
                      help="Comma-separated list of testing variants")

    return parser


  def _process_options(self, options):
    if not options.fuzzer_random_seed:
      options.fuzzer_random_seed = random_utils.random_seed()

    if options.total_timeout_sec:
      options.tests_count = 0

    if options.combine_tests:
      if options.combine_min > options.combine_max:
        print(('min_group_size (%d) cannot be larger than max_group_size (%d)' %
               options.min_group_size, options.max_group_size))
        raise base_runner.TestRunnerError()

    if options.variants != 'default':
      print ('Only default testing variant is supported with numfuzz')
      raise base_runner.TestRunnerError()

    return True

  def _get_default_suite_names(self):
    return DEFAULT_SUITES

  def _runner_flags(self):
    """Extra default flags specific to the test runner implementation."""
    return [
      '--no-abort-on-contradictory-flags',
      '--testing-d8-test-runner',
      '--no-fail'
    ]

  def _get_statusfile_variables(self, options):
    variables = (
        super(NumFuzzer, self)._get_statusfile_variables(options))
    variables.update({
      'deopt_fuzzer': bool(options.stress_deopt),
      'endurance_fuzzer': bool(options.combine_tests),
      'gc_stress': bool(options.stress_gc),
      'gc_fuzzer': bool(max([options.stress_marking,
                             options.stress_scavenge,
                             options.stress_compaction,
                             options.stress_gc,
                             options.stress_delay_tasks,
                             options.stress_stack_size,
                             options.stress_thread_pool_size])),
    })
    return variables

  def _do_execute(self, tests, args, options):
    loader = LoadProc(tests)
    fuzzer_rng = random.Random(options.fuzzer_random_seed)

    combiner = self._create_combiner(fuzzer_rng, options)
    results = self._create_result_tracker(options)
    execproc = ExecutionProc(options.j)
    sigproc = self._create_signal_proc()
    indicators = self._create_progress_indicators(
      tests.test_count_estimate, options)
    procs = [
      loader,
      NameFilterProc(args) if args else None,
      StatusFileFilterProc(None, None),
      # TODO(majeski): Improve sharding when combiner is present. Maybe select
      # different random seeds for shards instead of splitting tests.
      self._create_shard_proc(options),
      ExpectationProc(),
      combiner,
      self._create_fuzzer(fuzzer_rng, options),
      sigproc,
    ] + indicators + [
      results,
      self._create_timeout_proc(options),
      self._create_rerun_proc(options),
      execproc,
    ]
    self._prepare_procs(procs)
    loader.load_initial_tests(initial_batch_size=float('inf'))

    # TODO(majeski): maybe some notification from loader would be better?
    if combiner:
      combiner.generate_initial_tests(options.j * 4)

    # This starts up worker processes and blocks until all tests are
    # processed.
    execproc.run()

    for indicator in indicators:
      indicator.finished()

    print('>>> %d tests ran' % results.total)
    if results.failed:
      return utils.EXIT_CODE_FAILURES

    # Indicate if a SIGINT or SIGTERM happened.
    return sigproc.exit_code

  def _is_testsuite_supported(self, suite, options):
    return not options.combine_tests or suite.test_combiner_available()

  def _create_combiner(self, rng, options):
    if not options.combine_tests:
      return None
    return CombinerProc(rng, options.combine_min, options.combine_max,
                        options.tests_count)

  def _create_fuzzer(self, rng, options):
    return fuzzer.FuzzerProc(
        rng,
        self._tests_count(options),
        self._create_fuzzer_configs(options),
        self._disable_analysis(options),
    )

  def _tests_count(self, options):
    if options.combine_tests:
      return 1
    return options.tests_count

  def _disable_analysis(self, options):
    """Disable analysis phase when options are used that don't support it."""
    return options.combine_tests

  def _create_fuzzer_configs(self, options):
    fuzzers = []
    def add(name, prob, *args):
      if prob:
        fuzzers.append(fuzzer.create_fuzzer_config(name, prob, *args))

    add('compaction', options.stress_compaction)
    add('marking', options.stress_marking)
    add('scavenge', options.stress_scavenge)
    add('gc_interval', options.stress_gc)
    add('stack', options.stress_stack_size)
    add('threads', options.stress_thread_pool_size)
    add('delay', options.stress_delay_tasks)
    add('deopt', options.stress_deopt, options.stress_deopt_min)
    return fuzzers


if __name__ == '__main__':
  sys.exit(NumFuzzer().execute())

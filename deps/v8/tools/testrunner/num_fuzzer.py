#!/usr/bin/env python
#
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import random
import sys

# Adds testrunner to the path hence it has to be imported at the beggining.
import base_runner

from testrunner.local import utils

from testrunner.testproc import fuzzer
from testrunner.testproc.base import TestProcProducer
from testrunner.testproc.combiner import CombinerProc
from testrunner.testproc.execution import ExecutionProc
from testrunner.testproc.expectation import ForgiveTimeoutProc
from testrunner.testproc.filter import StatusFileFilterProc, NameFilterProc
from testrunner.testproc.loader import LoadProc
from testrunner.testproc.progress import ResultsTracker, TestsCounter
from testrunner.utils import random_utils


DEFAULT_SUITES = ["mjsunit", "webkit", "benchmarks"]


class NumFuzzer(base_runner.BaseTestRunner):
  def __init__(self, *args, **kwargs):
    super(NumFuzzer, self).__init__(*args, **kwargs)

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
    parser.add_option("--stress-thread-pool-size", default=0, type="int",
                      help="probability [0-10] of adding --thread-pool-size "
                           "flag to the test")

    # Stress deopt
    parser.add_option("--stress-deopt", default=0, type="int",
                      help="probability [0-10] of adding --deopt-every-n-times "
                           "flag to the test")
    parser.add_option("--stress-deopt-min", default=1, type="int",
                      help="extends --stress-deopt to have minimum interval "
                           "between deopt points")

    # Stress interrupt budget
    parser.add_option("--stress-interrupt-budget", default=0, type="int",
                      help="probability [0-10] of adding --interrupt-budget "
                           "flag to the test")

    # Combine multiple tests
    parser.add_option("--combine-tests", default=False, action="store_true",
                      help="Combine multiple tests as one and run with "
                           "try-catch wrapper")
    parser.add_option("--combine-max", default=100, type="int",
                      help="Maximum number of tests to combine")
    parser.add_option("--combine-min", default=2, type="int",
                      help="Minimum number of tests to combine")

    return parser


  def _process_options(self, options):
    if not options.fuzzer_random_seed:
      options.fuzzer_random_seed = random_utils.random_seed()

    if options.total_timeout_sec:
      options.tests_count = 0

    if options.combine_tests:
      if options.combine_min > options.combine_max:
        print ('min_group_size (%d) cannot be larger than max_group_size (%d)' %
               options.min_group_size, options.max_group_size)
        raise base_runner.TestRunnerError()

    return True

  def _get_default_suite_names(self):
    return DEFAULT_SUITES

  def _timeout_scalefactor(self, options):
    factor = super(NumFuzzer, self)._timeout_scalefactor(options)
    if options.stress_interrupt_budget:
      # TODO(machenbach): This should be moved to a more generic config.
      # Fuzzers have too much timeout in debug mode.
      factor = max(int(factor * 0.25), 1)
    return factor

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
                             options.stress_thread_pool_size])),
    })
    return variables

  def _do_execute(self, tests, args, options):
    loader = LoadProc()
    fuzzer_rng = random.Random(options.fuzzer_random_seed)

    combiner = self._create_combiner(fuzzer_rng, options)
    results = ResultsTracker()
    execproc = ExecutionProc(options.j)
    sigproc = self._create_signal_proc()
    indicators = self._create_progress_indicators(options)
    procs = [
      loader,
      NameFilterProc(args) if args else None,
      StatusFileFilterProc(None, None),
      # TODO(majeski): Improve sharding when combiner is present. Maybe select
      # different random seeds for shards instead of splitting tests.
      self._create_shard_proc(options),
      ForgiveTimeoutProc(),
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
    loader.load_tests(tests)

    # TODO(majeski): maybe some notification from loader would be better?
    if combiner:
      combiner.generate_initial_tests(options.j * 4)

    # This starts up worker processes and blocks until all tests are
    # processed.
    execproc.run()

    for indicator in indicators:
      indicator.finished()

    print '>>> %d tests ran' % results.total
    if results.failed:
      return utils.EXIT_CODE_FAILURES

    # Indicate if a SIGINT or SIGTERM happened.
    return sigproc.exit_code

  def _load_suites(self, names, options):
    suites = super(NumFuzzer, self)._load_suites(names, options)
    if options.combine_tests:
      suites = [s for s in suites if s.test_combiner_available()]
    if options.stress_interrupt_budget:
      # Changing interrupt budget forces us to suppress certain test assertions.
      for suite in suites:
        suite.do_suppress_internals()
    return suites

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
    return options.combine_tests or options.stress_interrupt_budget

  def _create_fuzzer_configs(self, options):
    fuzzers = []
    def add(name, prob, *args):
      if prob:
        fuzzers.append(fuzzer.create_fuzzer_config(name, prob, *args))

    add('compaction', options.stress_compaction)
    add('marking', options.stress_marking)
    add('scavenge', options.stress_scavenge)
    add('gc_interval', options.stress_gc)
    add('threads', options.stress_thread_pool_size)
    add('interrupt_budget', options.stress_interrupt_budget)
    add('deopt', options.stress_deopt, options.stress_deopt_min)
    return fuzzers


if __name__ == '__main__':
  sys.exit(NumFuzzer().execute())

#!/usr/bin/env python
#
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import multiprocessing
import random
import shlex
import sys

# Adds testrunner to the path hence it has to be imported at the beggining.
import base_runner

from testrunner.local import progress
from testrunner.local import utils
from testrunner.objects import context

from testrunner.testproc import fuzzer
from testrunner.testproc.base import TestProcProducer
from testrunner.testproc.combiner import CombinerProc
from testrunner.testproc.execution import ExecutionProc
from testrunner.testproc.filter import StatusFileFilterProc, NameFilterProc
from testrunner.testproc.loader import LoadProc
from testrunner.testproc.progress import ResultsTracker, TestsCounter
from testrunner.testproc.rerun import RerunProc
from testrunner.testproc.timeout import TimeoutProc


DEFAULT_SUITES = ["mjsunit", "webkit", "benchmarks"]
TIMEOUT_DEFAULT = 60

# Double the timeout for these:
SLOW_ARCHS = ["arm",
              "mipsel"]


class NumFuzzer(base_runner.BaseTestRunner):
  def __init__(self, *args, **kwargs):
    super(NumFuzzer, self).__init__(*args, **kwargs)

  def _add_parser_options(self, parser):
    parser.add_option("--command-prefix",
                      help="Prepended to each shell command used to run a test",
                      default="")
    parser.add_option("--dump-results-file", help="Dump maximum limit reached")
    parser.add_option("--extra-flags",
                      help="Additional flags to pass to each test command",
                      default="")
    parser.add_option("--isolates", help="Whether to test isolates",
                      default=False, action="store_true")
    parser.add_option("-j", help="The number of parallel tasks to run",
                      default=0, type="int")
    parser.add_option("--json-test-results",
                      help="Path to a file for storing json results.")
    parser.add_option("-p", "--progress",
                      help=("The style of progress indicator"
                            " (verbose, dots, color, mono)"),
                      choices=progress.PROGRESS_INDICATORS.keys(),
                      default="mono")
    parser.add_option("-t", "--timeout", help="Timeout in seconds",
                      default= -1, type="int")
    parser.add_option("--random-seed", default=0,
                      help="Default seed for initializing random generator")
    parser.add_option("--fuzzer-random-seed", default=0,
                      help="Default seed for initializing fuzzer random "
                      "generator")
    parser.add_option("--rerun-failures-count",
                      help=("Number of times to rerun each failing test case."
                            " Very slow tests will be rerun only once."),
                      default=0, type="int")
    parser.add_option("--rerun-failures-max",
                      help="Maximum number of failing test cases to rerun.",
                      default=100, type="int")
    parser.add_option("--swarming",
                      help="Indicates running test driver on swarming.",
                      default=False, action="store_true")
    parser.add_option("--tests-count", default=5, type="int",
                      help="Number of tests to generate from each base test. "
                           "Can be combined with --total-timeout-sec with "
                           "value 0 to provide infinite number of subtests. "
                           "When --combine-tests is set it indicates how many "
                           "tests to create in total")
    parser.add_option("--total-timeout-sec", default=0, type="int",
                      help="How long should fuzzer run. It overrides "
                           "--tests-count")

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

    # Stress deopt
    parser.add_option("--stress-deopt", default=0, type="int",
                      help="probability [0-10] of adding --deopt-every-n-times "
                           "flag to the test")
    parser.add_option("--stress-deopt-min", default=1, type="int",
                      help="extends --stress-deopt to have minimum interval "
                           "between deopt points")

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
    options.command_prefix = shlex.split(options.command_prefix)
    options.extra_flags = shlex.split(options.extra_flags)
    if options.j == 0:
      options.j = multiprocessing.cpu_count()
    while options.random_seed == 0:
      options.random_seed = random.SystemRandom().randint(-2147483648,
                                                          2147483647)
    while options.fuzzer_random_seed == 0:
      options.fuzzer_random_seed = random.SystemRandom().randint(-2147483648,
                                                                 2147483647)

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

  def _do_execute(self, suites, args, options):
    print(">>> Running tests for %s.%s" % (self.build_config.arch,
                                           self.mode_name))

    ctx = self._create_context(options)
    tests = self._load_tests(options, suites, ctx)
    progress_indicator = progress.IndicatorNotifier()
    progress_indicator.Register(
        progress.PROGRESS_INDICATORS[options.progress]())
    if options.json_test_results:
      progress_indicator.Register(progress.JsonTestProgressIndicator(
          options.json_test_results,
          self.build_config.arch,
          self.mode_options.execution_mode,
          ctx.random_seed))

    loader = LoadProc()
    fuzzer_rng = random.Random(options.fuzzer_random_seed)

    combiner = self._create_combiner(fuzzer_rng, options)
    results = ResultsTracker()
    execproc = ExecutionProc(options.j, ctx)
    indicators = progress_indicator.ToProgressIndicatorProcs()
    procs = [
      loader,
      NameFilterProc(args) if args else None,
      StatusFileFilterProc(None, None),
      # TODO(majeski): Improve sharding when combiner is present. Maybe select
      # different random seeds for shards instead of splitting tests.
      self._create_shard_proc(options),
      combiner,
      self._create_fuzzer(fuzzer_rng, options)
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
    execproc.start()

    for indicator in indicators:
      indicator.finished()

    print '>>> %d tests ran' % results.total
    if results.failed:
      print '>>> %d tests failed' % results.failed

    if results.failed:
      return 1
    if results.remaining:
      return 2
    return 0

  def _create_context(self, options):
    # Populate context object.
    timeout = options.timeout
    if timeout == -1:
      # Simulators are slow, therefore allow a longer default timeout.
      if self.build_config.arch in SLOW_ARCHS:
        timeout = 2 * TIMEOUT_DEFAULT;
      else:
        timeout = TIMEOUT_DEFAULT;

    timeout *= self.mode_options.timeout_scalefactor
    ctx = context.Context(self.build_config.arch,
                          self.mode_options.execution_mode,
                          self.outdir,
                          self.mode_options.flags, options.verbose,
                          timeout, options.isolates,
                          options.command_prefix,
                          options.extra_flags,
                          False,  # Keep i18n on by default.
                          options.random_seed,
                          True,  # No sorting of test cases.
                          options.rerun_failures_count,
                          options.rerun_failures_max,
                          False,  # No no_harness mode.
                          False,  # Don't use perf data.
                          False)  # Coverage not supported.
    return ctx

  def _load_tests(self, options, suites, ctx):
    if options.combine_tests:
      suites = [s for s in suites if s.test_combiner_available()]

    # Find available test suites and read test cases from them.
    deopt_fuzzer = bool(options.stress_deopt)
    gc_stress = bool(options.stress_gc)
    gc_fuzzer = bool(max([options.stress_marking,
                          options.stress_scavenge,
                          options.stress_compaction,
                          options.stress_gc]))

    variables = {
      "arch": self.build_config.arch,
      "asan": self.build_config.asan,
      "byteorder": sys.byteorder,
      "dcheck_always_on": self.build_config.dcheck_always_on,
      "deopt_fuzzer": deopt_fuzzer,
      "gc_fuzzer": gc_fuzzer,
      "gc_stress": gc_stress,
      "gcov_coverage": self.build_config.gcov_coverage,
      "isolates": options.isolates,
      "mode": self.mode_options.status_mode,
      "msan": self.build_config.msan,
      "no_harness": False,
      "no_i18n": self.build_config.no_i18n,
      "no_snap": self.build_config.no_snap,
      "novfp3": False,
      "predictable": self.build_config.predictable,
      "simulator": utils.UseSimulator(self.build_config.arch),
      "simulator_run": False,
      "system": utils.GuessOS(),
      "tsan": self.build_config.tsan,
      "ubsan_vptr": self.build_config.ubsan_vptr,
    }

    tests = []
    for s in suites:
      s.ReadStatusFile(variables)
      s.ReadTestCases(ctx)
      tests += s.tests
    return tests

  def _prepare_procs(self, procs):
    procs = filter(None, procs)
    for i in xrange(0, len(procs) - 1):
      procs[i].connect_to(procs[i + 1])
    procs[0].setup()

  def _create_combiner(self, rng, options):
    if not options.combine_tests:
      return None
    return CombinerProc(rng, options.combine_min, options.combine_max,
                        options.tests_count)

  def _create_fuzzer(self, rng, options):
    if options.combine_tests:
      count = 1
      disable_analysis = True
    else:
      count = options.tests_count
      disable_analysis = False
    return fuzzer.FuzzerProc(
        rng,
        count,
        self._create_fuzzer_configs(options),
        disable_analysis,
    )

  def _create_fuzzer_configs(self, options):
    fuzzers = []
    def add(name, prob, *args):
      if prob:
        fuzzers.append(fuzzer.create_fuzzer_config(name, prob, *args))

    add('compaction', options.stress_compaction)
    add('marking', options.stress_marking)
    add('scavenge', options.stress_scavenge)
    add('gc_interval', options.stress_gc)
    add('deopt', options.stress_deopt, options.stress_deopt_min)
    return fuzzers

  def _create_timeout_proc(self, options):
    if not options.total_timeout_sec:
      return None
    return TimeoutProc(options.total_timeout_sec)

  def _create_rerun_proc(self, options):
    if not options.rerun_failures_count:
      return None
    return RerunProc(options.rerun_failures_count,
                     options.rerun_failures_max)

if __name__ == '__main__':
  sys.exit(NumFuzzer().execute())

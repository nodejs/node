#!/usr/bin/env python
#
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from collections import OrderedDict
from os.path import join
import multiprocessing
import os
import random
import shlex
import subprocess
import sys
import time

# Adds testrunner to the path hence it has to be imported at the beggining.
import base_runner

from testrunner.local import execution
from testrunner.local import progress
from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.local import verbose
from testrunner.local.variants import ALL_VARIANTS
from testrunner.objects import context
from testrunner.objects import predictable
from testrunner.testproc.execution import ExecutionProc
from testrunner.testproc.filter import StatusFileFilterProc, NameFilterProc
from testrunner.testproc.loader import LoadProc
from testrunner.testproc.progress import (VerboseProgressIndicator,
                                          ResultsTracker,
                                          TestsCounter)
from testrunner.testproc.rerun import RerunProc
from testrunner.testproc.variant import VariantProc


TIMEOUT_DEFAULT = 60

# Variants ordered by expected runtime (slowest first).
VARIANTS = ["default"]

MORE_VARIANTS = [
  "stress",
  "stress_incremental_marking",
  "nooptimization",
  "stress_background_compile",
  "wasm_traps",
]

VARIANT_ALIASES = {
  # The default for developer workstations.
  "dev": VARIANTS,
  # Additional variants, run on all bots.
  "more": MORE_VARIANTS,
  # Shortcut for the two above ("more" first - it has the longer running tests).
  "exhaustive": MORE_VARIANTS + VARIANTS,
  # Additional variants, run on a subset of bots.
  "extra": ["future", "liftoff", "trusted"],
}

GC_STRESS_FLAGS = ["--gc-interval=500", "--stress-compaction",
                   "--concurrent-recompilation-queue-length=64",
                   "--concurrent-recompilation-delay=500",
                   "--concurrent-recompilation"]

# Double the timeout for these:
SLOW_ARCHS = ["arm",
              "mips",
              "mipsel",
              "mips64",
              "mips64el",
              "s390",
              "s390x",
              "arm64"]

PREDICTABLE_WRAPPER = os.path.join(
    base_runner.BASE_DIR, 'tools', 'predictable_wrapper.py')


class StandardTestRunner(base_runner.BaseTestRunner):
    def __init__(self, *args, **kwargs):
        super(StandardTestRunner, self).__init__(*args, **kwargs)

        self.sancov_dir = None

    def _get_default_suite_names(self):
      return ['default']

    def _do_execute(self, suites, args, options):
      if options.swarming:
        # Swarming doesn't print how isolated commands are called. Lets make
        # this less cryptic by printing it ourselves.
        print ' '.join(sys.argv)

        if utils.GuessOS() == "macos":
          # TODO(machenbach): Temporary output for investigating hanging test
          # driver on mac.
          print "V8 related processes running on this host:"
          try:
            print subprocess.check_output(
              "ps -e | egrep 'd8|cctest|unittests'", shell=True)
          except Exception:
            pass

      return self._execute(args, options, suites)

    def _add_parser_options(self, parser):
      parser.add_option("--sancov-dir",
                        help="Directory where to collect coverage data")
      parser.add_option("--cfi-vptr",
                        help="Run tests with UBSAN cfi_vptr option.",
                        default=False, action="store_true")
      parser.add_option("--novfp3",
                        help="Indicates that V8 was compiled without VFP3"
                        " support",
                        default=False, action="store_true")
      parser.add_option("--cat", help="Print the source of the tests",
                        default=False, action="store_true")
      parser.add_option("--slow-tests",
                        help="Regard slow tests (run|skip|dontcare)",
                        default="dontcare")
      parser.add_option("--pass-fail-tests",
                        help="Regard pass|fail tests (run|skip|dontcare)",
                        default="dontcare")
      parser.add_option("--gc-stress",
                        help="Switch on GC stress mode",
                        default=False, action="store_true")
      parser.add_option("--command-prefix",
                        help="Prepended to each shell command used to run a"
                        " test",
                        default="")
      parser.add_option("--extra-flags",
                        help="Additional flags to pass to each test command",
                        action="append", default=[])
      parser.add_option("--infra-staging", help="Use new test runner features",
                        default=False, action="store_true")
      parser.add_option("--isolates", help="Whether to test isolates",
                        default=False, action="store_true")
      parser.add_option("-j", help="The number of parallel tasks to run",
                        default=0, type="int")
      parser.add_option("--no-harness", "--noharness",
                        help="Run without test harness of a given suite",
                        default=False, action="store_true")
      parser.add_option("--no-presubmit", "--nopresubmit",
                        help='Skip presubmit checks (deprecated)',
                        default=False, dest="no_presubmit", action="store_true")
      parser.add_option("--no-sorting", "--nosorting",
                        help="Don't sort tests according to duration of last"
                        " run.",
                        default=False, dest="no_sorting", action="store_true")
      parser.add_option("--no-variants", "--novariants",
                        help="Deprecated. "
                             "Equivalent to passing --variants=default",
                        default=False, dest="no_variants", action="store_true")
      parser.add_option("--variants",
                        help="Comma-separated list of testing variants;"
                        " default: \"%s\"" % ",".join(VARIANTS))
      parser.add_option("--exhaustive-variants",
                        default=False, action="store_true",
                        help="Deprecated. "
                             "Equivalent to passing --variants=exhaustive")
      parser.add_option("-p", "--progress",
                        help=("The style of progress indicator"
                              " (verbose, dots, color, mono)"),
                        choices=progress.PROGRESS_INDICATORS.keys(),
                        default="mono")
      parser.add_option("--quickcheck", default=False, action="store_true",
                        help=("Quick check mode (skip slow tests)"))
      parser.add_option("--report", help="Print a summary of the tests to be"
                        " run",
                        default=False, action="store_true")
      parser.add_option("--json-test-results",
                        help="Path to a file for storing json results.")
      parser.add_option("--flakiness-results",
                        help="Path to a file for storing flakiness json.")
      parser.add_option("--rerun-failures-count",
                        help=("Number of times to rerun each failing test case."
                              " Very slow tests will be rerun only once."),
                        default=0, type="int")
      parser.add_option("--rerun-failures-max",
                        help="Maximum number of failing test cases to rerun.",
                        default=100, type="int")
      parser.add_option("--dont-skip-slow-simulator-tests",
                        help="Don't skip more slow tests when using a"
                        " simulator.",
                        default=False, action="store_true",
                        dest="dont_skip_simulator_slow_tests")
      parser.add_option("--swarming",
                        help="Indicates running test driver on swarming.",
                        default=False, action="store_true")
      parser.add_option("--time", help="Print timing information after running",
                        default=False, action="store_true")
      parser.add_option("-t", "--timeout", help="Timeout in seconds",
                        default=TIMEOUT_DEFAULT, type="int")
      parser.add_option("--warn-unused", help="Report unused rules",
                        default=False, action="store_true")
      parser.add_option("--junitout", help="File name of the JUnit output")
      parser.add_option("--junittestsuite",
                        help="The testsuite name in the JUnit output file",
                        default="v8tests")
      parser.add_option("--random-seed", default=0, dest="random_seed",
                        help="Default seed for initializing random generator",
                        type=int)
      parser.add_option("--random-seed-stress-count", default=1, type="int",
                        dest="random_seed_stress_count",
                        help="Number of runs with different random seeds")

    def _process_options(self, options):
      global VARIANTS

      if options.sancov_dir:
        self.sancov_dir = options.sancov_dir
        if not os.path.exists(self.sancov_dir):
          print("sancov-dir %s doesn't exist" % self.sancov_dir)
          raise base_runner.TestRunnerError()

      options.command_prefix = shlex.split(options.command_prefix)
      options.extra_flags = sum(map(shlex.split, options.extra_flags), [])

      if options.gc_stress:
        options.extra_flags += GC_STRESS_FLAGS

      if self.build_config.asan:
        options.extra_flags.append("--invoke-weak-callbacks")
        options.extra_flags.append("--omit-quit")

      if options.novfp3:
        options.extra_flags.append("--noenable-vfp3")

      if options.no_variants:  # pragma: no cover
        print ("Option --no-variants is deprecated. "
               "Pass --variants=default instead.")
        assert not options.variants
        options.variants = "default"

      if options.exhaustive_variants:  # pragma: no cover
        # TODO(machenbach): Switch infra to --variants=exhaustive after M65.
        print ("Option --exhaustive-variants is deprecated. "
               "Pass --variants=exhaustive instead.")
        # This is used on many bots. It includes a larger set of default
        # variants.
        # Other options for manipulating variants still apply afterwards.
        assert not options.variants
        options.variants = "exhaustive"

      if options.quickcheck:
        assert not options.variants
        options.variants = "stress,default"
        options.slow_tests = "skip"
        options.pass_fail_tests = "skip"

      if self.build_config.predictable:
        options.variants = "default"
        options.extra_flags.append("--predictable")
        options.extra_flags.append("--verify_predictable")
        options.extra_flags.append("--no-inline-new")
        # Add predictable wrapper to command prefix.
        options.command_prefix = (
            [sys.executable, PREDICTABLE_WRAPPER] + options.command_prefix)

      # TODO(machenbach): Figure out how to test a bigger subset of variants on
      # msan.
      if self.build_config.msan:
        options.variants = "default"

      if options.j == 0:
        options.j = multiprocessing.cpu_count()

      if options.random_seed_stress_count <= 1 and options.random_seed == 0:
        options.random_seed = self._random_seed()

      # Use developer defaults if no variant was specified.
      options.variants = options.variants or "dev"

      if options.variants == "infra_staging":
        options.variants = "exhaustive"
        options.infra_staging = True

      # Resolve variant aliases and dedupe.
      # TODO(machenbach): Don't mutate global variable. Rather pass mutated
      # version as local variable.
      VARIANTS = list(set(reduce(
          list.__add__,
          (VARIANT_ALIASES.get(v, [v]) for v in options.variants.split(",")),
          [],
      )))

      if not set(VARIANTS).issubset(ALL_VARIANTS):
        print "All variants must be in %s" % str(ALL_VARIANTS)
        raise base_runner.TestRunnerError()

      def CheckTestMode(name, option):  # pragma: no cover
        if not option in ["run", "skip", "dontcare"]:
          print "Unknown %s mode %s" % (name, option)
          raise base_runner.TestRunnerError()
      CheckTestMode("slow test", options.slow_tests)
      CheckTestMode("pass|fail test", options.pass_fail_tests)
      if self.build_config.no_i18n:
        base_runner.TEST_MAP["bot_default"].remove("intl")
        base_runner.TEST_MAP["default"].remove("intl")
        # TODO(machenbach): uncomment after infra side lands.
        # base_runner.TEST_MAP["d8_default"].remove("intl")

    def _setup_env(self):
      super(StandardTestRunner, self)._setup_env()

      symbolizer_option = self._get_external_symbolizer_option()

      if self.sancov_dir:
        os.environ['ASAN_OPTIONS'] = ":".join([
          'coverage=1',
          'coverage_dir=%s' % self.sancov_dir,
          symbolizer_option,
          "allow_user_segv_handler=1",
        ])

    def _random_seed(self):
      seed = 0
      while not seed:
        seed = random.SystemRandom().randint(-2147483648, 2147483647)
      return seed

    def _execute(self, args, options, suites):
      print(">>> Running tests for %s.%s" % (self.build_config.arch,
                                             self.mode_name))
      # Populate context object.

      # Simulators are slow, therefore allow a longer timeout.
      if self.build_config.arch in SLOW_ARCHS:
        options.timeout *= 2

      options.timeout *= self.mode_options.timeout_scalefactor

      if self.build_config.predictable:
        # Predictable mode is slower.
        options.timeout *= 2

      ctx = context.Context(self.build_config.arch,
                            self.mode_options.execution_mode,
                            self.outdir,
                            self.mode_options.flags,
                            options.verbose,
                            options.timeout,
                            options.isolates,
                            options.command_prefix,
                            options.extra_flags,
                            self.build_config.no_i18n,
                            options.random_seed,
                            options.no_sorting,
                            options.rerun_failures_count,
                            options.rerun_failures_max,
                            options.no_harness,
                            use_perf_data=not options.swarming,
                            sancov_dir=self.sancov_dir,
                            infra_staging=options.infra_staging)

      # TODO(all): Combine "simulator" and "simulator_run".
      # TODO(machenbach): In GN we can derive simulator run from
      # target_arch != v8_target_arch in the dumped build config.
      simulator_run = (
        not options.dont_skip_simulator_slow_tests and
        self.build_config.arch in [
          'arm64', 'arm', 'mipsel', 'mips', 'mips64', 'mips64el', 'ppc',
          'ppc64', 's390', 's390x'] and
        bool(base_runner.ARCH_GUESS) and
        self.build_config.arch != base_runner.ARCH_GUESS)
      # Find available test suites and read test cases from them.
      variables = {
        "arch": self.build_config.arch,
        "asan": self.build_config.asan,
        "byteorder": sys.byteorder,
        "dcheck_always_on": self.build_config.dcheck_always_on,
        "deopt_fuzzer": False,
        "gc_fuzzer": False,
        "gc_stress": options.gc_stress,
        "gcov_coverage": self.build_config.gcov_coverage,
        "isolates": options.isolates,
        "mode": self.mode_options.status_mode,
        "msan": self.build_config.msan,
        "no_harness": options.no_harness,
        "no_i18n": self.build_config.no_i18n,
        "no_snap": self.build_config.no_snap,
        "novfp3": options.novfp3,
        "predictable": self.build_config.predictable,
        "simulator": utils.UseSimulator(self.build_config.arch),
        "simulator_run": simulator_run,
        "system": utils.GuessOS(),
        "tsan": self.build_config.tsan,
        "ubsan_vptr": self.build_config.ubsan_vptr,
      }

      progress_indicator = progress.IndicatorNotifier()
      progress_indicator.Register(
        progress.PROGRESS_INDICATORS[options.progress]())
      if options.junitout:  # pragma: no cover
        progress_indicator.Register(progress.JUnitTestProgressIndicator(
            options.junitout, options.junittestsuite))
      if options.json_test_results:
        progress_indicator.Register(progress.JsonTestProgressIndicator(
          options.json_test_results,
          self.build_config.arch,
          self.mode_options.execution_mode,
          ctx.random_seed))
      if options.flakiness_results:  # pragma: no cover
        progress_indicator.Register(progress.FlakinessTestProgressIndicator(
            options.flakiness_results))

      if options.infra_staging:
        for s in suites:
          s.ReadStatusFile(variables)
          s.ReadTestCases(ctx)

        return self._run_test_procs(suites, args, options, progress_indicator,
                                    ctx)

      all_tests = []
      num_tests = 0
      for s in suites:
        s.ReadStatusFile(variables)
        s.ReadTestCases(ctx)
        if len(args) > 0:
          s.FilterTestCasesByArgs(args)
        all_tests += s.tests

        # First filtering by status applying the generic rules (tests without
        # variants)
        if options.warn_unused:
          tests = [(t.name, t.variant) for t in s.tests]
          s.statusfile.warn_unused_rules(tests, check_variant_rules=False)
        s.FilterTestCasesByStatus(options.slow_tests, options.pass_fail_tests)

        if options.cat:
          verbose.PrintTestSource(s.tests)
          continue
        variant_gen = s.CreateLegacyVariantsGenerator(VARIANTS)
        variant_tests = [ t.create_variant(v, flags)
                          for t in s.tests
                          for v in variant_gen.FilterVariantsByTest(t)
                          for flags in variant_gen.GetFlagSets(t, v) ]

        if options.random_seed_stress_count > 1:
          # Duplicate test for random seed stress mode.
          def iter_seed_flags():
            for _ in range(0, options.random_seed_stress_count):
              # Use given random seed for all runs (set by default in
              # execution.py) or a new random seed if none is specified.
              if options.random_seed:
                yield []
              else:
                yield ["--random-seed=%d" % self._random_seed()]
          s.tests = [
            t.create_variant(t.variant, flags, 'seed-stress-%d' % n)
            for t in variant_tests
            for n, flags in enumerate(iter_seed_flags())
          ]
        else:
          s.tests = variant_tests

        # Second filtering by status applying also the variant-dependent rules.
        if options.warn_unused:
          tests = [(t.name, t.variant) for t in s.tests]
          s.statusfile.warn_unused_rules(tests, check_variant_rules=True)

        s.FilterTestCasesByStatus(options.slow_tests, options.pass_fail_tests)
        s.tests = self._shard_tests(s.tests, options)

        for t in s.tests:
          t.cmd = t.get_command(ctx)

        num_tests += len(s.tests)

      if options.cat:
        return 0  # We're done here.

      if options.report:
        verbose.PrintReport(all_tests)

      # Run the tests.
      start_time = time.time()

      if self.build_config.predictable:
        outproc_factory = predictable.get_outproc
      else:
        outproc_factory = None

      runner = execution.Runner(suites, progress_indicator, ctx,
                                outproc_factory)
      exit_code = runner.Run(options.j)
      overall_duration = time.time() - start_time

      if options.time:
        verbose.PrintTestDurations(suites, runner.outputs, overall_duration)

      if num_tests == 0:
        print("Warning: no tests were run!")

      if exit_code == 1 and options.json_test_results:
        print("Force exit code 0 after failures. Json test results file "
              "generated with failure information.")
        exit_code = 0

      if self.sancov_dir:
        # If tests ran with sanitizer coverage, merge coverage files in the end.
        try:
          print "Merging sancov files."
          subprocess.check_call([
            sys.executable,
            join(self.basedir, "tools", "sanitizers", "sancov_merger.py"),
            "--coverage-dir=%s" % self.sancov_dir])
        except:
          print >> sys.stderr, "Error: Merging sancov files failed."
          exit_code = 1

      return exit_code

    def _shard_tests(self, tests, options):
      shard_run, shard_count = self._get_shard_info(options)

      if shard_count < 2:
        return tests
      count = 0
      shard = []
      for test in tests:
        if count % shard_count == shard_run - 1:
          shard.append(test)
        count += 1
      return shard

    def _run_test_procs(self, suites, args, options, progress_indicator,
                        context):
      jobs = options.j

      print '>>> Running with test processors'
      loader = LoadProc()
      tests_counter = TestsCounter()
      results = ResultsTracker()
      indicators = progress_indicator.ToProgressIndicatorProcs()
      execproc = ExecutionProc(jobs, context)

      procs = [
        loader,
        NameFilterProc(args) if args else None,
        StatusFileFilterProc(options.slow_tests, options.pass_fail_tests),
        self._create_shard_proc(options),
        tests_counter,
        VariantProc(VARIANTS),
        StatusFileFilterProc(options.slow_tests, options.pass_fail_tests),
      ] + indicators + [
        results,
        self._create_rerun_proc(context),
        execproc,
      ]

      procs = filter(None, procs)

      for i in xrange(0, len(procs) - 1):
        procs[i].connect_to(procs[i + 1])

      tests = [t for s in suites for t in s.tests]
      tests.sort(key=lambda t: t.is_slow, reverse=True)

      loader.setup()
      loader.load_tests(tests)

      print '>>> Running %d base tests' % tests_counter.total
      tests_counter.remove_from_chain()

      execproc.start()

      for indicator in indicators:
        indicator.finished()

      print '>>> %d tests ran' % results.total

      exit_code = 0
      if results.failed:
        exit_code = 1
      if results.remaining:
        exit_code = 2


      if exit_code == 1 and options.json_test_results:
        print("Force exit code 0 after failures. Json test results file "
              "generated with failure information.")
        exit_code = 0
      return exit_code

    def _create_rerun_proc(self, ctx):
      if not ctx.rerun_failures_count:
        return None
      return RerunProc(ctx.rerun_failures_count,
                       ctx.rerun_failures_max)



if __name__ == '__main__':
  sys.exit(StandardTestRunner().execute())

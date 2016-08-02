#!/usr/bin/env python
#
# Copyright 2012 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


from collections import OrderedDict
import itertools
import multiprocessing
import optparse
import os
from os.path import join
import platform
import random
import shlex
import subprocess
import sys
import time

from testrunner.local import execution
from testrunner.local import progress
from testrunner.local import testsuite
from testrunner.local.testsuite import ALL_VARIANTS
from testrunner.local import utils
from testrunner.local import verbose
from testrunner.network import network_execution
from testrunner.objects import context


# Base dir of the v8 checkout to be used as cwd.
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

ARCH_GUESS = utils.DefaultArch()

# Map of test name synonyms to lists of test suites. Should be ordered by
# expected runtimes (suites with slow test cases first). These groups are
# invoked in seperate steps on the bots.
TEST_MAP = {
  # This needs to stay in sync with test/bot_default.isolate.
  "bot_default": [
    "mjsunit",
    "cctest",
    "webkit",
    "fuzzer",
    "message",
    "preparser",
    "intl",
    "unittests",
  ],
  # This needs to stay in sync with test/default.isolate.
  "default": [
    "mjsunit",
    "cctest",
    "fuzzer",
    "message",
    "preparser",
    "intl",
    "unittests",
  ],
  # This needs to stay in sync with test/ignition.isolate.
  "ignition": [
    "mjsunit",
    "cctest",
    "webkit",
    "message",
  ],
  # This needs to stay in sync with test/optimize_for_size.isolate.
  "optimize_for_size": [
    "mjsunit",
    "cctest",
    "webkit",
    "intl",
  ],
  "unittests": [
    "unittests",
  ],
}

TIMEOUT_DEFAULT = 60

VARIANTS = ["default", "stress", "turbofan"]

EXHAUSTIVE_VARIANTS = VARIANTS + [
  "ignition",
  "nocrankshaft",
  "turbofan_opt",
]

DEBUG_FLAGS = ["--nohard-abort", "--nodead-code-elimination",
               "--nofold-constants", "--enable-slow-asserts",
               "--debug-code", "--verify-heap"]
RELEASE_FLAGS = ["--nohard-abort", "--nodead-code-elimination",
                 "--nofold-constants"]

MODES = {
  "debug": {
    "flags": DEBUG_FLAGS,
    "timeout_scalefactor": 4,
    "status_mode": "debug",
    "execution_mode": "debug",
    "output_folder": "debug",
  },
  "optdebug": {
    "flags": DEBUG_FLAGS,
    "timeout_scalefactor": 4,
    "status_mode": "debug",
    "execution_mode": "debug",
    "output_folder": "optdebug",
  },
  "release": {
    "flags": RELEASE_FLAGS,
    "timeout_scalefactor": 1,
    "status_mode": "release",
    "execution_mode": "release",
    "output_folder": "release",
  },
  # Normal trybot release configuration. There, dchecks are always on which
  # implies debug is set. Hence, the status file needs to assume debug-like
  # behavior/timeouts.
  "tryrelease": {
    "flags": RELEASE_FLAGS,
    "timeout_scalefactor": 1,
    "status_mode": "debug",
    "execution_mode": "release",
    "output_folder": "release",
  },
  # This mode requires v8 to be compiled with dchecks and slow dchecks.
  "slowrelease": {
    "flags": RELEASE_FLAGS + ["--enable-slow-asserts"],
    "timeout_scalefactor": 2,
    "status_mode": "debug",
    "execution_mode": "release",
    "output_folder": "release",
  },
}

GC_STRESS_FLAGS = ["--gc-interval=500", "--stress-compaction",
                   "--concurrent-recompilation-queue-length=64",
                   "--concurrent-recompilation-delay=500",
                   "--concurrent-recompilation"]

SUPPORTED_ARCHS = ["android_arm",
                   "android_arm64",
                   "android_ia32",
                   "android_x64",
                   "arm",
                   "ia32",
                   "x87",
                   "mips",
                   "mipsel",
                   "mips64",
                   "mips64el",
                   "nacl_ia32",
                   "nacl_x64",
                   "s390",
                   "s390x",
                   "ppc",
                   "ppc64",
                   "x64",
                   "x32",
                   "arm64"]
# Double the timeout for these:
SLOW_ARCHS = ["android_arm",
              "android_arm64",
              "android_ia32",
              "android_x64",
              "arm",
              "mips",
              "mipsel",
              "mips64",
              "mips64el",
              "nacl_ia32",
              "nacl_x64",
              "s390",
              "s390x",
              "x87",
              "arm64"]


def BuildOptions():
  result = optparse.OptionParser()
  result.usage = '%prog [options] [tests]'
  result.description = """TESTS: %s""" % (TEST_MAP["default"])
  result.add_option("--arch",
                    help=("The architecture to run tests for, "
                          "'auto' or 'native' for auto-detect: %s" % SUPPORTED_ARCHS),
                    default="ia32,x64,arm")
  result.add_option("--arch-and-mode",
                    help="Architecture and mode in the format 'arch.mode'",
                    default=None)
  result.add_option("--asan",
                    help="Regard test expectations for ASAN",
                    default=False, action="store_true")
  result.add_option("--sancov-dir",
                    help="Directory where to collect coverage data")
  result.add_option("--cfi-vptr",
                    help="Run tests with UBSAN cfi_vptr option.",
                    default=False, action="store_true")
  result.add_option("--buildbot",
                    help="Adapt to path structure used on buildbots",
                    default=False, action="store_true")
  result.add_option("--dcheck-always-on",
                    help="Indicates that V8 was compiled with DCHECKs enabled",
                    default=False, action="store_true")
  result.add_option("--novfp3",
                    help="Indicates that V8 was compiled without VFP3 support",
                    default=False, action="store_true")
  result.add_option("--cat", help="Print the source of the tests",
                    default=False, action="store_true")
  result.add_option("--slow-tests",
                    help="Regard slow tests (run|skip|dontcare)",
                    default="dontcare")
  result.add_option("--pass-fail-tests",
                    help="Regard pass|fail tests (run|skip|dontcare)",
                    default="dontcare")
  result.add_option("--gc-stress",
                    help="Switch on GC stress mode",
                    default=False, action="store_true")
  result.add_option("--gcov-coverage",
                    help="Uses executables instrumented for gcov coverage",
                    default=False, action="store_true")
  result.add_option("--command-prefix",
                    help="Prepended to each shell command used to run a test",
                    default="")
  result.add_option("--download-data", help="Download missing test suite data",
                    default=False, action="store_true")
  result.add_option("--download-data-only",
                    help="Download missing test suite data and exit",
                    default=False, action="store_true")
  result.add_option("--extra-flags",
                    help="Additional flags to pass to each test command",
                    default="")
  result.add_option("--ignition", help="Skip tests which don't run in ignition",
                    default=False, action="store_true")
  result.add_option("--ignition-turbofan",
                    help="Skip tests which don't run in ignition_turbofan",
                    default=False, action="store_true")
  result.add_option("--isolates", help="Whether to test isolates",
                    default=False, action="store_true")
  result.add_option("-j", help="The number of parallel tasks to run",
                    default=0, type="int")
  result.add_option("-m", "--mode",
                    help="The test modes in which to run (comma-separated,"
                    " uppercase for ninja and buildbot builds): %s" % MODES.keys(),
                    default="release,debug")
  result.add_option("--no-harness", "--noharness",
                    help="Run without test harness of a given suite",
                    default=False, action="store_true")
  result.add_option("--no-i18n", "--noi18n",
                    help="Skip internationalization tests",
                    default=False, action="store_true")
  result.add_option("--no-network", "--nonetwork",
                    help="Don't distribute tests on the network",
                    default=(utils.GuessOS() != "linux"),
                    dest="no_network", action="store_true")
  result.add_option("--no-presubmit", "--nopresubmit",
                    help='Skip presubmit checks',
                    default=False, dest="no_presubmit", action="store_true")
  result.add_option("--no-snap", "--nosnap",
                    help='Test a build compiled without snapshot.',
                    default=False, dest="no_snap", action="store_true")
  result.add_option("--no-sorting", "--nosorting",
                    help="Don't sort tests according to duration of last run.",
                    default=False, dest="no_sorting", action="store_true")
  result.add_option("--no-stress", "--nostress",
                    help="Don't run crankshaft --always-opt --stress-op test",
                    default=False, dest="no_stress", action="store_true")
  result.add_option("--no-variants", "--novariants",
                    help="Don't run any testing variants",
                    default=False, dest="no_variants", action="store_true")
  result.add_option("--variants",
                    help="Comma-separated list of testing variants: %s" % VARIANTS)
  result.add_option("--exhaustive-variants",
                    default=False, action="store_true",
                    help="Use exhaustive set of default variants.")
  result.add_option("--outdir", help="Base directory with compile output",
                    default="out")
  result.add_option("--predictable",
                    help="Compare output of several reruns of each test",
                    default=False, action="store_true")
  result.add_option("-p", "--progress",
                    help=("The style of progress indicator"
                          " (verbose, dots, color, mono)"),
                    choices=progress.PROGRESS_INDICATORS.keys(), default="mono")
  result.add_option("--quickcheck", default=False, action="store_true",
                    help=("Quick check mode (skip slow tests)"))
  result.add_option("--report", help="Print a summary of the tests to be run",
                    default=False, action="store_true")
  result.add_option("--json-test-results",
                    help="Path to a file for storing json results.")
  result.add_option("--rerun-failures-count",
                    help=("Number of times to rerun each failing test case. "
                          "Very slow tests will be rerun only once."),
                    default=0, type="int")
  result.add_option("--rerun-failures-max",
                    help="Maximum number of failing test cases to rerun.",
                    default=100, type="int")
  result.add_option("--shard-count",
                    help="Split testsuites into this number of shards",
                    default=1, type="int")
  result.add_option("--shard-run",
                    help="Run this shard from the split up tests.",
                    default=1, type="int")
  result.add_option("--shell", help="DEPRECATED! use --shell-dir", default="")
  result.add_option("--shell-dir", help="Directory containing executables",
                    default="")
  result.add_option("--dont-skip-slow-simulator-tests",
                    help="Don't skip more slow tests when using a simulator.",
                    default=False, action="store_true",
                    dest="dont_skip_simulator_slow_tests")
  result.add_option("--stress-only",
                    help="Only run tests with --always-opt --stress-opt",
                    default=False, action="store_true")
  result.add_option("--swarming",
                    help="Indicates running test driver on swarming.",
                    default=False, action="store_true")
  result.add_option("--time", help="Print timing information after running",
                    default=False, action="store_true")
  result.add_option("-t", "--timeout", help="Timeout in seconds",
                    default=TIMEOUT_DEFAULT, type="int")
  result.add_option("--tsan",
                    help="Regard test expectations for TSAN",
                    default=False, action="store_true")
  result.add_option("-v", "--verbose", help="Verbose output",
                    default=False, action="store_true")
  result.add_option("--valgrind", help="Run tests through valgrind",
                    default=False, action="store_true")
  result.add_option("--warn-unused", help="Report unused rules",
                    default=False, action="store_true")
  result.add_option("--junitout", help="File name of the JUnit output")
  result.add_option("--junittestsuite",
                    help="The testsuite name in the JUnit output file",
                    default="v8tests")
  result.add_option("--random-seed", default=0, dest="random_seed", type="int",
                    help="Default seed for initializing random generator")
  result.add_option("--random-seed-stress-count", default=1, type="int",
                    dest="random_seed_stress_count",
                    help="Number of runs with different random seeds")
  result.add_option("--msan",
                    help="Regard test expectations for MSAN",
                    default=False, action="store_true")
  return result


def RandomSeed():
  seed = 0
  while not seed:
    seed = random.SystemRandom().randint(-2147483648, 2147483647)
  return seed


def BuildbotToV8Mode(config):
  """Convert buildbot build configs to configs understood by the v8 runner.

  V8 configs are always lower case and without the additional _x64 suffix for
  64 bit builds on windows with ninja.
  """
  mode = config[:-4] if config.endswith('_x64') else config
  return mode.lower()

def SetupEnvironment(options):
  """Setup additional environment variables."""

  # Many tests assume an English interface.
  os.environ['LANG'] = 'en_US.UTF-8'

  symbolizer = 'external_symbolizer_path=%s' % (
      os.path.join(
          BASE_DIR, 'third_party', 'llvm-build', 'Release+Asserts', 'bin',
          'llvm-symbolizer',
      )
  )

  if options.asan:
    os.environ['ASAN_OPTIONS'] = symbolizer

  if options.sancov_dir:
    assert os.path.exists(options.sancov_dir)
    os.environ['ASAN_OPTIONS'] = ":".join([
      'coverage=1',
      'coverage_dir=%s' % options.sancov_dir,
      symbolizer,
    ])

  if options.cfi_vptr:
    os.environ['UBSAN_OPTIONS'] = ":".join([
      'print_stacktrace=1',
      'print_summary=1',
      'symbolize=1',
      symbolizer,
    ])

  if options.msan:
    os.environ['MSAN_OPTIONS'] = symbolizer

  if options.tsan:
    suppressions_file = os.path.join(
        BASE_DIR, 'tools', 'sanitizers', 'tsan_suppressions.txt')
    os.environ['TSAN_OPTIONS'] = " ".join([
      symbolizer,
      'suppressions=%s' % suppressions_file,
      'exit_code=0',
      'report_thread_leaks=0',
      'history_size=7',
      'report_destroy_locked=0',
    ])

def ProcessOptions(options):
  global ALL_VARIANTS
  global EXHAUSTIVE_VARIANTS
  global VARIANTS

  # Architecture and mode related stuff.
  if options.arch_and_mode:
    options.arch_and_mode = [arch_and_mode.split(".")
        for arch_and_mode in options.arch_and_mode.split(",")]
    options.arch = ",".join([tokens[0] for tokens in options.arch_and_mode])
    options.mode = ",".join([tokens[1] for tokens in options.arch_and_mode])
  options.mode = options.mode.split(",")
  for mode in options.mode:
    if not BuildbotToV8Mode(mode) in MODES:
      print "Unknown mode %s" % mode
      return False
  if options.arch in ["auto", "native"]:
    options.arch = ARCH_GUESS
  options.arch = options.arch.split(",")
  for arch in options.arch:
    if not arch in SUPPORTED_ARCHS:
      print "Unknown architecture %s" % arch
      return False

  # Store the final configuration in arch_and_mode list. Don't overwrite
  # predefined arch_and_mode since it is more expressive than arch and mode.
  if not options.arch_and_mode:
    options.arch_and_mode = itertools.product(options.arch, options.mode)

  # Special processing of other options, sorted alphabetically.

  if options.buildbot:
    # Buildbots run presubmit tests as a separate step.
    options.no_presubmit = True
    options.no_network = True
  if options.download_data_only:
    options.no_presubmit = True
  if options.command_prefix:
    print("Specifying --command-prefix disables network distribution, "
          "running tests locally.")
    options.no_network = True
  options.command_prefix = shlex.split(options.command_prefix)
  options.extra_flags = shlex.split(options.extra_flags)

  if options.gc_stress:
    options.extra_flags += GC_STRESS_FLAGS

  if options.asan:
    options.extra_flags.append("--invoke-weak-callbacks")
    options.extra_flags.append("--omit-quit")

  if options.novfp3:
    options.extra_flags.append("--noenable-vfp3")

  if options.exhaustive_variants:
    # This is used on many bots. It includes a larger set of default variants.
    # Other options for manipulating variants still apply afterwards.
    VARIANTS = EXHAUSTIVE_VARIANTS

  if options.msan:
    VARIANTS = ["default"]

  if options.tsan:
    VARIANTS = ["default"]

  if options.j == 0:
    options.j = multiprocessing.cpu_count()

  if options.random_seed_stress_count <= 1 and options.random_seed == 0:
    options.random_seed = RandomSeed()

  def excl(*args):
    """Returns true if zero or one of multiple arguments are true."""
    return reduce(lambda x, y: x + y, args) <= 1

  if not excl(options.no_stress, options.stress_only, options.no_variants,
              bool(options.variants)):
    print("Use only one of --no-stress, --stress-only, --no-variants, "
          "or --variants.")
    return False
  if options.quickcheck:
    VARIANTS = ["default", "stress"]
    options.slow_tests = "skip"
    options.pass_fail_tests = "skip"
  if options.no_stress:
    VARIANTS = ["default", "nocrankshaft"]
  if options.no_variants:
    VARIANTS = ["default"]
  if options.stress_only:
    VARIANTS = ["stress"]
  if options.variants:
    VARIANTS = options.variants.split(",")
    if not set(VARIANTS).issubset(ALL_VARIANTS):
      print "All variants must be in %s" % str(ALL_VARIANTS)
      return False
  if options.predictable:
    VARIANTS = ["default"]
    options.extra_flags.append("--predictable")
    options.extra_flags.append("--verify_predictable")
    options.extra_flags.append("--no-inline-new")

  if not options.shell_dir:
    if options.shell:
      print "Warning: --shell is deprecated, use --shell-dir instead."
      options.shell_dir = os.path.dirname(options.shell)
  if options.valgrind:
    run_valgrind = os.path.join("tools", "run-valgrind.py")
    # This is OK for distributed running, so we don't need to set no_network.
    options.command_prefix = (["python", "-u", run_valgrind] +
                              options.command_prefix)
  def CheckTestMode(name, option):
    if not option in ["run", "skip", "dontcare"]:
      print "Unknown %s mode %s" % (name, option)
      return False
    return True
  if not CheckTestMode("slow test", options.slow_tests):
    return False
  if not CheckTestMode("pass|fail test", options.pass_fail_tests):
    return False
  if options.no_i18n:
    TEST_MAP["bot_default"].remove("intl")
    TEST_MAP["default"].remove("intl")
  return True


def ShardTests(tests, options):
  # Read gtest shard configuration from environment (e.g. set by swarming).
  # If none is present, use values passed on the command line.
  shard_count = int(os.environ.get('GTEST_TOTAL_SHARDS', options.shard_count))
  shard_run = os.environ.get('GTEST_SHARD_INDEX')
  if shard_run is not None:
    # The v8 shard_run starts at 1, while GTEST_SHARD_INDEX starts at 0.
    shard_run = int(shard_run) + 1
  else:
    shard_run = options.shard_run

  if options.shard_count > 1:
    # Log if a value was passed on the cmd line and it differs from the
    # environment variables.
    if options.shard_count != shard_count:
      print("shard_count from cmd line differs from environment variable "
            "GTEST_TOTAL_SHARDS")
    if options.shard_run > 1 and options.shard_run != shard_run:
      print("shard_run from cmd line differs from environment variable "
            "GTEST_SHARD_INDEX")

  if shard_count < 2:
    return tests
  if shard_run < 1 or shard_run > shard_count:
    print "shard-run not a valid number, should be in [1:shard-count]"
    print "defaulting back to running all tests"
    return tests
  count = 0
  shard = []
  for test in tests:
    if count % shard_count == shard_run - 1:
      shard.append(test)
    count += 1
  return shard


def Main():
  # Use the v8 root as cwd as some test cases use "load" with relative paths.
  os.chdir(BASE_DIR)

  parser = BuildOptions()
  (options, args) = parser.parse_args()
  if not ProcessOptions(options):
    parser.print_help()
    return 1
  SetupEnvironment(options)

  if options.swarming:
    # Swarming doesn't print how isolated commands are called. Lets make this
    # less cryptic by printing it ourselves.
    print ' '.join(sys.argv)

  exit_code = 0
  if not options.no_presubmit:
    print ">>> running presubmit tests"
    exit_code = subprocess.call(
        [sys.executable, join(BASE_DIR, "tools", "presubmit.py")])

  suite_paths = utils.GetSuitePaths(join(BASE_DIR, "test"))

  # Use default tests if no test configuration was provided at the cmd line.
  if len(args) == 0:
    args = ["default"]

  # Expand arguments with grouped tests. The args should reflect the list of
  # suites as otherwise filters would break.
  def ExpandTestGroups(name):
    if name in TEST_MAP:
      return [suite for suite in TEST_MAP[name]]
    else:
      return [name]
  args = reduce(lambda x, y: x + y,
         [ExpandTestGroups(arg) for arg in args],
         [])

  args_suites = OrderedDict() # Used as set
  for arg in args:
    args_suites[arg.split('/')[0]] = True
  suite_paths = [ s for s in args_suites if s in suite_paths ]

  suites = []
  for root in suite_paths:
    suite = testsuite.TestSuite.LoadTestSuite(
        os.path.join(BASE_DIR, "test", root))
    if suite:
      suites.append(suite)

  if options.download_data or options.download_data_only:
    for s in suites:
      s.DownloadData()

  if options.download_data_only:
    return exit_code

  for (arch, mode) in options.arch_and_mode:
    try:
      code = Execute(arch, mode, args, options, suites)
    except KeyboardInterrupt:
      return 2
    exit_code = exit_code or code
  return exit_code


def Execute(arch, mode, args, options, suites):
  print(">>> Running tests for %s.%s" % (arch, mode))

  shell_dir = options.shell_dir
  if not shell_dir:
    if options.buildbot:
      # TODO(machenbach): Get rid of different output folder location on
      # buildbot. Currently this is capitalized Release and Debug.
      shell_dir = os.path.join(BASE_DIR, options.outdir, mode)
      mode = BuildbotToV8Mode(mode)
    else:
      shell_dir = os.path.join(
          BASE_DIR,
          options.outdir,
          "%s.%s" % (arch, MODES[mode]["output_folder"]),
      )
  if not os.path.exists(shell_dir):
      raise Exception('Could not find shell_dir: "%s"' % shell_dir)

  # Populate context object.
  mode_flags = MODES[mode]["flags"]

  # Simulators are slow, therefore allow a longer timeout.
  if arch in SLOW_ARCHS:
    options.timeout *= 2

  options.timeout *= MODES[mode]["timeout_scalefactor"]

  if options.predictable:
    # Predictable mode is slower.
    options.timeout *= 2

  # TODO(machenbach): Remove temporary verbose output on windows after
  # debugging driver-hung-up on XP.
  verbose_output = (
      options.verbose or
      utils.IsWindows() and options.progress == "verbose"
  )
  ctx = context.Context(arch, MODES[mode]["execution_mode"], shell_dir,
                        mode_flags, verbose_output,
                        options.timeout,
                        options.isolates,
                        options.command_prefix,
                        options.extra_flags,
                        options.no_i18n,
                        options.random_seed,
                        options.no_sorting,
                        options.rerun_failures_count,
                        options.rerun_failures_max,
                        options.predictable,
                        options.no_harness,
                        use_perf_data=not options.swarming,
                        sancov_dir=options.sancov_dir)

  # TODO(all): Combine "simulator" and "simulator_run".
  simulator_run = not options.dont_skip_simulator_slow_tests and \
      arch in ['arm64', 'arm', 'mipsel', 'mips', 'mips64', 'mips64el', \
               'ppc', 'ppc64'] and \
      ARCH_GUESS and arch != ARCH_GUESS
  # Find available test suites and read test cases from them.
  variables = {
    "arch": arch,
    "asan": options.asan,
    "deopt_fuzzer": False,
    "gc_stress": options.gc_stress,
    "gcov_coverage": options.gcov_coverage,
    "ignition": options.ignition,
    "ignition_turbofan": options.ignition_turbofan,
    "isolates": options.isolates,
    "mode": MODES[mode]["status_mode"],
    "no_i18n": options.no_i18n,
    "no_snap": options.no_snap,
    "simulator_run": simulator_run,
    "simulator": utils.UseSimulator(arch),
    "system": utils.GuessOS(),
    "tsan": options.tsan,
    "msan": options.msan,
    "dcheck_always_on": options.dcheck_always_on,
    "novfp3": options.novfp3,
    "predictable": options.predictable,
    "byteorder": sys.byteorder,
  }
  all_tests = []
  num_tests = 0
  for s in suites:
    s.ReadStatusFile(variables)
    s.ReadTestCases(ctx)
    if len(args) > 0:
      s.FilterTestCasesByArgs(args)
    all_tests += s.tests
    s.FilterTestCasesByStatus(options.warn_unused, options.slow_tests,
                              options.pass_fail_tests)
    if options.cat:
      verbose.PrintTestSource(s.tests)
      continue
    variant_gen = s.CreateVariantGenerator(VARIANTS)
    variant_tests = [ t.CopyAddingFlags(v, flags)
                      for t in s.tests
                      for v in variant_gen.FilterVariantsByTest(t)
                      for flags in variant_gen.GetFlagSets(t, v) ]

    if options.random_seed_stress_count > 1:
      # Duplicate test for random seed stress mode.
      def iter_seed_flags():
        for i in range(0, options.random_seed_stress_count):
          # Use given random seed for all runs (set by default in execution.py)
          # or a new random seed if none is specified.
          if options.random_seed:
            yield []
          else:
            yield ["--random-seed=%d" % RandomSeed()]
      s.tests = [
        t.CopyAddingFlags(t.variant, flags)
        for t in variant_tests
        for flags in iter_seed_flags()
      ]
    else:
      s.tests = variant_tests

    s.tests = ShardTests(s.tests, options)
    num_tests += len(s.tests)

  if options.cat:
    return 0  # We're done here.

  if options.report:
    verbose.PrintReport(all_tests)

  # Run the tests, either locally or distributed on the network.
  start_time = time.time()
  progress_indicator = progress.IndicatorNotifier()
  progress_indicator.Register(progress.PROGRESS_INDICATORS[options.progress]())
  if options.junitout:
    progress_indicator.Register(progress.JUnitTestProgressIndicator(
        options.junitout, options.junittestsuite))
  if options.json_test_results:
    progress_indicator.Register(progress.JsonTestProgressIndicator(
        options.json_test_results, arch, MODES[mode]["execution_mode"],
        ctx.random_seed))

  run_networked = not options.no_network
  if not run_networked:
    if verbose_output:
      print("Network distribution disabled, running tests locally.")
  elif utils.GuessOS() != "linux":
    print("Network distribution is only supported on Linux, sorry!")
    run_networked = False
  peers = []
  if run_networked:
    peers = network_execution.GetPeers()
    if not peers:
      print("No connection to distribution server; running tests locally.")
      run_networked = False
    elif len(peers) == 1:
      print("No other peers on the network; running tests locally.")
      run_networked = False
    elif num_tests <= 100:
      print("Less than 100 tests, running them locally.")
      run_networked = False

  if run_networked:
    runner = network_execution.NetworkedRunner(suites, progress_indicator,
                                               ctx, peers, BASE_DIR)
  else:
    runner = execution.Runner(suites, progress_indicator, ctx)

  exit_code = runner.Run(options.j)
  overall_duration = time.time() - start_time

  if options.time:
    verbose.PrintTestDurations(suites, overall_duration)

  if num_tests == 0:
    print("Warning: no tests were run!")

  if exit_code == 1 and options.json_test_results:
    print("Force exit code 0 after failures. Json test results file generated "
          "with failure information.")
    exit_code = 0

  if options.sancov_dir:
    # If tests ran with sanitizer coverage, merge coverage files in the end.
    try:
      print "Merging sancov files."
      subprocess.check_call([
        sys.executable,
        join(BASE_DIR, "tools", "sanitizers", "sancov_merger.py"),
        "--coverage-dir=%s" % options.sancov_dir])
    except:
      print >> sys.stderr, "Error: Merging sancov files failed."
      exit_code = 1

  return exit_code


if __name__ == "__main__":
  sys.exit(Main())

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


import json
import math
import multiprocessing
import optparse
import os
from os.path import join
import random
import shlex
import subprocess
import sys
import time

from testrunner.local import execution
from testrunner.local import progress
from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.local import verbose
from testrunner.objects import context


# Base dir of the v8 checkout to be used as cwd.
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

ARCH_GUESS = utils.DefaultArch()
DEFAULT_TESTS = ["mjsunit", "webkit"]
TIMEOUT_DEFAULT = 60
TIMEOUT_SCALEFACTOR = {"debug"   : 4,
                       "release" : 1 }

MODE_FLAGS = {
    "debug"   : ["--nohard-abort", "--nodead-code-elimination",
                 "--nofold-constants", "--enable-slow-asserts",
                 "--debug-code", "--verify-heap",
                 "--noconcurrent-recompilation"],
    "release" : ["--nohard-abort", "--nodead-code-elimination",
                 "--nofold-constants", "--noconcurrent-recompilation"]}

SUPPORTED_ARCHS = ["android_arm",
                   "android_ia32",
                   "arm",
                   "ia32",
                   "ppc",
                   "ppc64",
                   "s390",
                   "s390x",
                   "mipsel",
                   "nacl_ia32",
                   "nacl_x64",
                   "x64"]
# Double the timeout for these:
SLOW_ARCHS = ["android_arm",
              "android_ia32",
              "arm",
              "mipsel",
              "nacl_ia32",
              "nacl_x64"]
MAX_DEOPT = 1000000000
DISTRIBUTION_MODES = ["smooth", "random"]


class RandomDistribution:
  def __init__(self, seed=None):
    seed = seed or random.randint(1, sys.maxint)
    print "Using random distribution with seed %d" % seed
    self._random = random.Random(seed)

  def Distribute(self, n, m):
    if n > m:
      n = m
    return self._random.sample(xrange(1, m + 1), n)


class SmoothDistribution:
  """Distribute n numbers into the interval [1:m].
  F1: Factor of the first derivation of the distribution function.
  F2: Factor of the second derivation of the distribution function.
  With F1 and F2 set to 0, the distribution will be equal.
  """
  def __init__(self, factor1=2.0, factor2=0.2):
    self._factor1 = factor1
    self._factor2 = factor2

  def Distribute(self, n, m):
    if n > m:
      n = m
    if n <= 1:
      return [ 1 ]

    result = []
    x = 0.0
    dx = 1.0
    ddx = self._factor1
    dddx = self._factor2
    for i in range(0, n):
      result += [ x ]
      x += dx
      dx += ddx
      ddx += dddx

    # Project the distribution into the interval [0:M].
    result = [ x * m / result[-1] for x in result ]

    # Equalize by n. The closer n is to m, the more equal will be the
    # distribution.
    for (i, x) in enumerate(result):
      # The value of x if it was equally distributed.
      equal_x = i / float(n - 1) * float(m - 1) + 1

      # Difference factor between actual and equal distribution.
      diff = 1 - (x / equal_x)

      # Equalize x dependent on the number of values to distribute.
      result[i] = int(x + (i + 1) * diff)
    return result


def Distribution(options):
  if options.distribution_mode == "random":
    return RandomDistribution(options.seed)
  if options.distribution_mode == "smooth":
    return SmoothDistribution(options.distribution_factor1,
                              options.distribution_factor2)


def BuildOptions():
  result = optparse.OptionParser()
  result.add_option("--arch",
                    help=("The architecture to run tests for, "
                          "'auto' or 'native' for auto-detect"),
                    default="ia32,x64,arm")
  result.add_option("--arch-and-mode",
                    help="Architecture and mode in the format 'arch.mode'",
                    default=None)
  result.add_option("--asan",
                    help="Regard test expectations for ASAN",
                    default=False, action="store_true")
  result.add_option("--buildbot",
                    help="Adapt to path structure used on buildbots",
                    default=False, action="store_true")
  result.add_option("--dcheck-always-on",
                    help="Indicates that V8 was compiled with DCHECKs enabled",
                    default=False, action="store_true")
  result.add_option("--command-prefix",
                    help="Prepended to each shell command used to run a test",
                    default="")
  result.add_option("--coverage", help=("Exponential test coverage "
                    "(range 0.0, 1.0) -- 0.0: one test, 1.0 all tests (slow)"),
                    default=0.4, type="float")
  result.add_option("--coverage-lift", help=("Lifts test coverage for tests "
                    "with a small number of deopt points (range 0, inf)"),
                    default=20, type="int")
  result.add_option("--download-data", help="Download missing test suite data",
                    default=False, action="store_true")
  result.add_option("--distribution-factor1", help=("Factor of the first "
                    "derivation of the distribution function"), default=2.0,
                    type="float")
  result.add_option("--distribution-factor2", help=("Factor of the second "
                    "derivation of the distribution function"), default=0.7,
                    type="float")
  result.add_option("--distribution-mode", help=("How to select deopt points "
                    "for a given test (smooth|random)"),
                    default="smooth")
  result.add_option("--dump-results-file", help=("Dump maximum number of "
                    "deopt points per test to a file"))
  result.add_option("--extra-flags",
                    help="Additional flags to pass to each test command",
                    default="")
  result.add_option("--isolates", help="Whether to test isolates",
                    default=False, action="store_true")
  result.add_option("-j", help="The number of parallel tasks to run",
                    default=0, type="int")
  result.add_option("-m", "--mode",
                    help="The test modes in which to run (comma-separated)",
                    default="release,debug")
  result.add_option("--outdir", help="Base directory with compile output",
                    default="out")
  result.add_option("-p", "--progress",
                    help=("The style of progress indicator"
                          " (verbose, dots, color, mono)"),
                    choices=progress.PROGRESS_INDICATORS.keys(),
                    default="mono")
  result.add_option("--shard-count",
                    help="Split testsuites into this number of shards",
                    default=1, type="int")
  result.add_option("--shard-run",
                    help="Run this shard from the split up tests.",
                    default=1, type="int")
  result.add_option("--shell-dir", help="Directory containing executables",
                    default="")
  result.add_option("--seed", help="The seed for the random distribution",
                    type="int")
  result.add_option("-t", "--timeout", help="Timeout in seconds",
                    default= -1, type="int")
  result.add_option("-v", "--verbose", help="Verbose output",
                    default=False, action="store_true")
  result.add_option("--random-seed", default=0, dest="random_seed",
                    help="Default seed for initializing random generator")
  return result


def ProcessOptions(options):
  global VARIANT_FLAGS

  # Architecture and mode related stuff.
  if options.arch_and_mode:
    tokens = options.arch_and_mode.split(".")
    options.arch = tokens[0]
    options.mode = tokens[1]
  options.mode = options.mode.split(",")
  for mode in options.mode:
    if not mode.lower() in ["debug", "release"]:
      print "Unknown mode %s" % mode
      return False
  if options.arch in ["auto", "native"]:
    options.arch = ARCH_GUESS
  options.arch = options.arch.split(",")
  for arch in options.arch:
    if not arch in SUPPORTED_ARCHS:
      print "Unknown architecture %s" % arch
      return False

  # Special processing of other options, sorted alphabetically.
  options.command_prefix = shlex.split(options.command_prefix)
  options.extra_flags = shlex.split(options.extra_flags)
  if options.j == 0:
    options.j = multiprocessing.cpu_count()
  while options.random_seed == 0:
    options.random_seed = random.SystemRandom().randint(-2147483648, 2147483647)
  if not options.distribution_mode in DISTRIBUTION_MODES:
    print "Unknown distribution mode %s" % options.distribution_mode
    return False
  if options.distribution_factor1 < 0.0:
    print ("Distribution factor1 %s is out of range. Defaulting to 0.0"
        % options.distribution_factor1)
    options.distribution_factor1 = 0.0
  if options.distribution_factor2 < 0.0:
    print ("Distribution factor2 %s is out of range. Defaulting to 0.0"
        % options.distribution_factor2)
    options.distribution_factor2 = 0.0
  if options.coverage < 0.0 or options.coverage > 1.0:
    print ("Coverage %s is out of range. Defaulting to 0.4"
        % options.coverage)
    options.coverage = 0.4
  if options.coverage_lift < 0:
    print ("Coverage lift %s is out of range. Defaulting to 0"
        % options.coverage_lift)
    options.coverage_lift = 0
  return True


def ShardTests(tests, shard_count, shard_run):
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

  exit_code = 0

  suite_paths = utils.GetSuitePaths(join(BASE_DIR, "test"))

  if len(args) == 0:
    suite_paths = [ s for s in suite_paths if s in DEFAULT_TESTS ]
  else:
    args_suites = set()
    for arg in args:
      suite = arg.split(os.path.sep)[0]
      if not suite in args_suites:
        args_suites.add(suite)
    suite_paths = [ s for s in suite_paths if s in args_suites ]

  suites = []
  for root in suite_paths:
    suite = testsuite.TestSuite.LoadTestSuite(
        os.path.join(BASE_DIR, "test", root))
    if suite:
      suites.append(suite)

  if options.download_data:
    for s in suites:
      s.DownloadData()

  for mode in options.mode:
    for arch in options.arch:
      try:
        code = Execute(arch, mode, args, options, suites, BASE_DIR)
        exit_code = exit_code or code
      except KeyboardInterrupt:
        return 2
  return exit_code


def CalculateNTests(m, options):
  """Calculates the number of tests from m deopt points with exponential
  coverage.
  The coverage is expected to be between 0.0 and 1.0.
  The 'coverage lift' lifts the coverage for tests with smaller m values.
  """
  c = float(options.coverage)
  l = float(options.coverage_lift)
  return int(math.pow(m, (m * c + l) / (m + l)))


def Execute(arch, mode, args, options, suites, workspace):
  print(">>> Running tests for %s.%s" % (arch, mode))

  dist = Distribution(options)

  shell_dir = options.shell_dir
  if not shell_dir:
    if options.buildbot:
      shell_dir = os.path.join(workspace, options.outdir, mode)
      mode = mode.lower()
    else:
      shell_dir = os.path.join(workspace, options.outdir,
                               "%s.%s" % (arch, mode))
  shell_dir = os.path.relpath(shell_dir)

  # Populate context object.
  mode_flags = MODE_FLAGS[mode]
  timeout = options.timeout
  if timeout == -1:
    # Simulators are slow, therefore allow a longer default timeout.
    if arch in SLOW_ARCHS:
      timeout = 2 * TIMEOUT_DEFAULT;
    else:
      timeout = TIMEOUT_DEFAULT;

  timeout *= TIMEOUT_SCALEFACTOR[mode]
  ctx = context.Context(arch, mode, shell_dir,
                        mode_flags, options.verbose,
                        timeout, options.isolates,
                        options.command_prefix,
                        options.extra_flags,
                        False,  # Keep i18n on by default.
                        options.random_seed,
                        True,  # No sorting of test cases.
                        0,  # Don't rerun failing tests.
                        0,  # No use of a rerun-failing-tests maximum.
                        False,  # No predictable mode.
                        False,  # No no_harness mode.
                        False,  # Don't use perf data.
                        False)  # Coverage not supported.

  # Find available test suites and read test cases from them.
  variables = {
    "arch": arch,
    "asan": options.asan,
    "deopt_fuzzer": True,
    "gc_stress": False,
    "gcov_coverage": False,
    "ignition": False,
    "ignition_turbofan": False,
    "isolates": options.isolates,
    "mode": mode,
    "no_i18n": False,
    "no_snap": False,
    "simulator": utils.UseSimulator(arch),
    "system": utils.GuessOS(),
    "tsan": False,
    "msan": False,
    "dcheck_always_on": options.dcheck_always_on,
    "novfp3": False,
    "predictable": False,
    "byteorder": sys.byteorder,
  }
  all_tests = []
  num_tests = 0
  test_id = 0

  # Remember test case prototypes for the fuzzing phase.
  test_backup = dict((s, []) for s in suites)

  for s in suites:
    s.ReadStatusFile(variables)
    s.ReadTestCases(ctx)
    if len(args) > 0:
      s.FilterTestCasesByArgs(args)
    all_tests += s.tests
    s.FilterTestCasesByStatus(False)
    test_backup[s] = s.tests
    analysis_flags = ["--deopt-every-n-times", "%d" % MAX_DEOPT,
                      "--print-deopt-stress"]
    s.tests = [ t.CopyAddingFlags(t.variant, analysis_flags) for t in s.tests ]
    num_tests += len(s.tests)
    for t in s.tests:
      t.id = test_id
      test_id += 1

  if num_tests == 0:
    print "No tests to run."
    return 0

  print(">>> Collection phase")
  progress_indicator = progress.PROGRESS_INDICATORS[options.progress]()
  runner = execution.Runner(suites, progress_indicator, ctx)

  exit_code = runner.Run(options.j)

  print(">>> Analysis phase")
  num_tests = 0
  test_id = 0
  for s in suites:
    test_results = {}
    for t in s.tests:
      for line in t.output.stdout.splitlines():
        if line.startswith("=== Stress deopt counter: "):
          test_results[t.path] = MAX_DEOPT - int(line.split(" ")[-1])
    for t in s.tests:
      if t.path not in test_results:
        print "Missing results for %s" % t.path
    if options.dump_results_file:
      results_dict = dict((t.path, n) for (t, n) in test_results.iteritems())
      with file("%s.%d.txt" % (dump_results_file, time.time()), "w") as f:
        f.write(json.dumps(results_dict))

    # Reset tests and redistribute the prototypes from the collection phase.
    s.tests = []
    if options.verbose:
      print "Test distributions:"
    for t in test_backup[s]:
      max_deopt = test_results.get(t.path, 0)
      if max_deopt == 0:
        continue
      n_deopt = CalculateNTests(max_deopt, options)
      distribution = dist.Distribute(n_deopt, max_deopt)
      if options.verbose:
        print "%s %s" % (t.path, distribution)
      for i in distribution:
        fuzzing_flags = ["--deopt-every-n-times", "%d" % i]
        s.tests.append(t.CopyAddingFlags(t.variant, fuzzing_flags))
    num_tests += len(s.tests)
    for t in s.tests:
      t.id = test_id
      test_id += 1

  if num_tests == 0:
    print "No tests to run."
    return 0

  print(">>> Deopt fuzzing phase (%d test cases)" % num_tests)
  progress_indicator = progress.PROGRESS_INDICATORS[options.progress]()
  runner = execution.Runner(suites, progress_indicator, ctx)

  code = runner.Run(options.j)
  return exit_code or code


if __name__ == "__main__":
  sys.exit(Main())

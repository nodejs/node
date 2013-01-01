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


import multiprocessing
import optparse
import os
from os.path import join
import subprocess
import sys
import time

from testrunner.local import execution
from testrunner.local import progress
from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.local import verbose
from testrunner.network import network_execution
from testrunner.objects import context


ARCH_GUESS = utils.DefaultArch()
DEFAULT_TESTS = ["mjsunit", "cctest", "message", "preparser"]
TIMEOUT_DEFAULT = 60
TIMEOUT_SCALEFACTOR = {"debug"   : 4,
                       "release" : 1 }

# Use this to run several variants of the tests.
VARIANT_FLAGS = [[],
                 ["--stress-opt", "--always-opt"],
                 ["--nocrankshaft"]]
MODE_FLAGS = {
    "debug"   : ["--nobreak-on-abort", "--nodead-code-elimination",
                 "--enable-slow-asserts", "--debug-code", "--verify-heap"],
    "release" : ["--nobreak-on-abort", "--nodead-code-elimination"]}

SUPPORTED_ARCHS = ["android_arm",
                   "android_ia32",
                   "arm",
                   "ia32",
                   "mipsel",
                   "x64"]
# Double the timeout for these:
SLOW_ARCHS = ["android_arm",
              "android_ia32",
              "arm",
              "mipsel"]


def BuildOptions():
  result = optparse.OptionParser()
  result.add_option("--arch",
                    help=("The architecture to run tests for, "
                          "'auto' or 'native' for auto-detect"),
                    default="ia32,x64,arm")
  result.add_option("--arch-and-mode",
                    help="Architecture and mode in the format 'arch.mode'",
                    default=None)
  result.add_option("--buildbot",
                    help="Adapt to path structure used on buildbots",
                    default=False, action="store_true")
  result.add_option("--cat", help="Print the source of the tests",
                    default=False, action="store_true")
  result.add_option("--command-prefix",
                    help="Prepended to each shell command used to run a test",
                    default="")
  result.add_option("--download-data", help="Download missing test suite data",
                    default=False, action="store_true")
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
  result.add_option("--no-network", "--nonetwork",
                    help="Don't distribute tests on the network",
                    default=(utils.GuessOS() != "linux"),
                    dest="no_network", action="store_true")
  result.add_option("--no-presubmit", "--nopresubmit",
                    help='Skip presubmit checks',
                    default=False, dest="no_presubmit", action="store_true")
  result.add_option("--no-stress", "--nostress",
                    help="Don't run crankshaft --always-opt --stress-op test",
                    default=False, dest="no_stress", action="store_true")
  result.add_option("--outdir", help="Base directory with compile output",
                    default="out")
  result.add_option("-p", "--progress",
                    help=("The style of progress indicator"
                          " (verbose, dots, color, mono)"),
                    choices=progress.PROGRESS_INDICATORS.keys(), default="mono")
  result.add_option("--report", help="Print a summary of the tests to be run",
                    default=False, action="store_true")
  result.add_option("--shard-count",
                    help="Split testsuites into this number of shards",
                    default=1, type="int")
  result.add_option("--shard-run",
                    help="Run this shard from the split up tests.",
                    default=1, type="int")
  result.add_option("--shell", help="DEPRECATED! use --shell-dir", default="")
  result.add_option("--shell-dir", help="Directory containing executables",
                    default="")
  result.add_option("--stress-only",
                    help="Only run tests with --always-opt --stress-opt",
                    default=False, action="store_true")
  result.add_option("--time", help="Print timing information after running",
                    default=False, action="store_true")
  result.add_option("-t", "--timeout", help="Timeout in seconds",
                    default= -1, type="int")
  result.add_option("-v", "--verbose", help="Verbose output",
                    default=False, action="store_true")
  result.add_option("--valgrind", help="Run tests through valgrind",
                    default=False, action="store_true")
  result.add_option("--warn-unused", help="Report unused rules",
                    default=False, action="store_true")
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
    if not mode in ["debug", "release"]:
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

  if options.buildbot:
    # Buildbots run presubmit tests as a separate step.
    options.no_presubmit = True
    options.no_network = True
  if options.command_prefix:
    print("Specifying --command-prefix disables network distribution, "
          "running tests locally.")
    options.no_network = True
  if options.j == 0:
    options.j = multiprocessing.cpu_count()
  if options.no_stress:
    VARIANT_FLAGS = [[], ["--nocrankshaft"]]
  if not options.shell_dir:
    if options.shell:
      print "Warning: --shell is deprecated, use --shell-dir instead."
      options.shell_dir = os.path.dirname(options.shell)
  if options.stress_only:
    VARIANT_FLAGS = [["--stress-opt", "--always-opt"]]
  if options.valgrind:
    run_valgrind = os.path.join("tools", "run-valgrind.py")
    # This is OK for distributed running, so we don't need to set no_network.
    options.command_prefix = ("python -u " + run_valgrind +
                              options.command_prefix)
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
  parser = BuildOptions()
  (options, args) = parser.parse_args()
  if not ProcessOptions(options):
    parser.print_help()
    return 1

  exit_code = 0
  workspace = os.path.abspath(join(os.path.dirname(sys.argv[0]), ".."))
  if not options.no_presubmit:
    print ">>> running presubmit tests"
    code = subprocess.call(
        [sys.executable, join(workspace, "tools", "presubmit.py")])
    exit_code = code

  suite_paths = utils.GetSuitePaths(join(workspace, "test"))

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
        os.path.join(workspace, "test", root))
    if suite:
      suites.append(suite)

  if options.download_data:
    for s in suites:
      s.DownloadData()

  for mode in options.mode:
    for arch in options.arch:
      code = Execute(arch, mode, args, options, suites, workspace)
      exit_code = exit_code or code
  return exit_code


def Execute(arch, mode, args, options, suites, workspace):
  print(">>> Running tests for %s.%s" % (arch, mode))

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
                        options.extra_flags)

  # Find available test suites and read test cases from them.
  variables = {
    "mode": mode,
    "arch": arch,
    "system": utils.GuessOS(),
    "isolates": options.isolates
  }
  all_tests = []
  num_tests = 0
  test_id = 0
  for s in suites:
    s.ReadStatusFile(variables)
    s.ReadTestCases(ctx)
    if len(args) > 0:
      s.FilterTestCasesByArgs(args)
    all_tests += s.tests
    s.FilterTestCasesByStatus(options.warn_unused)
    if options.cat:
      verbose.PrintTestSource(s.tests)
      continue
    variant_flags = s.VariantFlags() or VARIANT_FLAGS
    s.tests = [ t.CopyAddingFlags(v) for t in s.tests for v in variant_flags ]
    s.tests = ShardTests(s.tests, options.shard_count, options.shard_run)
    num_tests += len(s.tests)
    for t in s.tests:
      t.id = test_id
      test_id += 1

  if options.cat:
    return 0  # We're done here.

  if options.report:
    verbose.PrintReport(all_tests)

  if num_tests == 0:
    print "No tests to run."
    return 0

  # Run the tests, either locally or distributed on the network.
  try:
    start_time = time.time()
    progress_indicator = progress.PROGRESS_INDICATORS[options.progress]()

    run_networked = not options.no_network
    if not run_networked:
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
                                                 ctx, peers, workspace)
    else:
      runner = execution.Runner(suites, progress_indicator, ctx)

    exit_code = runner.Run(options.j)
    if runner.terminate:
      return exit_code
    overall_duration = time.time() - start_time
  except KeyboardInterrupt:
    return 1

  if options.time:
    verbose.PrintTestDurations(suites, overall_duration)
  return exit_code


if __name__ == "__main__":
  sys.exit(Main())

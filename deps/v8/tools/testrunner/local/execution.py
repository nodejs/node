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


import collections
import os
import re
import shutil
import sys
import time

from pool import Pool
from . import commands
from . import perfdata
from . import statusfile
from . import testsuite
from . import utils
from ..objects import output


# Base dir of the v8 checkout.
BASE_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))
TEST_DIR = os.path.join(BASE_DIR, "test")


class Instructions(object):
  def __init__(self, command, test_id, timeout, verbose, env):
    self.command = command
    self.id = test_id
    self.timeout = timeout
    self.verbose = verbose
    self.env = env


# Structure that keeps global information per worker process.
ProcessContext = collections.namedtuple(
    "process_context", ["suites", "context"])


def MakeProcessContext(context, suite_names):
  """Generate a process-local context.

  This reloads all suites per process and stores the global context.

  Args:
    context: The global context from the test runner.
    suite_names (list of str): Suite names as loaded by the parent process.
        Load the same suites in each subprocess.
  """
  suites = {}
  for root in suite_names:
    # Don't reinitialize global state as this is concurrently called from
    # different processes.
    suite = testsuite.TestSuite.LoadTestSuite(
        os.path.join(TEST_DIR, root), global_init=False)
    if suite:
      suites[suite.name] = suite
  return ProcessContext(suites, context)


def GetCommand(test, context):
  d8testflag = []
  shell = test.suite.GetShellForTestCase(test)
  if shell == "d8":
    d8testflag = ["--test"]
  if utils.IsWindows():
    shell += ".exe"
  if context.random_seed:
    d8testflag += ["--random-seed=%s" % context.random_seed]
  files, flags, env = test.suite.GetParametersForTestCase(test, context)
  cmd = (
      context.command_prefix +
      [os.path.abspath(os.path.join(context.shell_dir, shell))] +
      d8testflag +
      files +
      context.extra_flags +
      # Flags from test cases can overwrite extra cmd-line flags.
      flags
  )
  return cmd, env


def _GetInstructions(test, context):
  command, env = GetCommand(test, context)
  timeout = context.timeout
  if ("--stress-opt" in test.flags or
      "--stress-opt" in context.mode_flags or
      "--stress-opt" in context.extra_flags):
    timeout *= 4
  if "--noenable-vfp3" in context.extra_flags:
    timeout *= 2

  # TODO(majeski): make it slow outcome dependent.
  timeout *= 2
  return Instructions(command, test.id, timeout, context.verbose, env)


class Job(object):
  """Stores data to be sent over the multi-process boundary.

  All contained fields will be pickled/unpickled.
  """

  def Run(self, process_context):
    """Executes the job.

    Args:
      process_context: Process-local information that is initialized by the
                       executing worker.
    """
    raise NotImplementedError()


def SetupProblem(exception, test):
  stderr = ">>> EXCEPTION: %s\n" % exception
  match = re.match(r"^.*No such file or directory: '(.*)'$", str(exception))
  if match:
    # Extra debuging information when files are claimed missing.
    f = match.group(1)
    stderr += ">>> File %s exists? -> %s\n" % (f, os.path.exists(f))
  return test.id, output.Output(1, False, "", stderr, None), 0


class TestJob(Job):
  def __init__(self, test):
    self.test = test

  def _rename_coverage_data(self, output, context):
    """Rename coverage data.

    Rename files with PIDs to files with unique test IDs, because the number
    of tests might be higher than pid_max. E.g.:
    d8.1234.sancov -> d8.test.42.1.sancov, where 1234 was the process' PID,
    42 is the test ID and 1 is the attempt (the same test might be rerun on
    failures).
    """
    if context.sancov_dir and output.pid is not None:
      shell = self.test.suite.GetShellForTestCase(self.test)
      sancov_file = os.path.join(
          context.sancov_dir, "%s.%d.sancov" % (shell, output.pid))

      # Some tests are expected to fail and don't produce coverage data.
      if os.path.exists(sancov_file):
        parts = sancov_file.split(".")
        new_sancov_file = ".".join(
            parts[:-2] +
            ["test", str(self.test.id), str(self.test.run)] +
            parts[-1:]
        )
        assert not os.path.exists(new_sancov_file)
        os.rename(sancov_file, new_sancov_file)

  def Run(self, process_context):
    try:
      # Retrieve a new suite object on the worker-process side. The original
      # suite object isn't pickled.
      self.test.SetSuiteObject(process_context.suites)
      instr = _GetInstructions(self.test, process_context.context)
    except Exception, e:
      # TODO(majeski): Better exception reporting.
      return SetupProblem(e, self.test)

    start_time = time.time()
    output = commands.Execute(instr.command, instr.verbose, instr.timeout,
                              instr.env)
    self._rename_coverage_data(output, process_context.context)
    return (instr.id, output, time.time() - start_time)


def RunTest(job, process_context):
  return job.Run(process_context)


class Runner(object):

  def __init__(self, suites, progress_indicator, context):
    self.datapath = os.path.join("out", "testrunner_data")
    self.perf_data_manager = perfdata.GetPerfDataManager(
        context, self.datapath)
    self.perfdata = self.perf_data_manager.GetStore(context.arch, context.mode)
    self.perf_failures = False
    self.printed_allocations = False
    self.tests = [t for s in suites for t in s.tests]
    self.suite_names = [s.name for s in suites]

    # Always pre-sort by status file, slowest tests first.
    slow_key = lambda t: statusfile.IsSlow(t.suite.GetStatusFileOutcomes(t))
    self.tests.sort(key=slow_key, reverse=True)

    # Sort by stored duration of not opted out.
    if not context.no_sorting:
      for t in self.tests:
        t.duration = self.perfdata.FetchPerfData(t) or 1.0
      self.tests.sort(key=lambda t: t.duration, reverse=True)

    self._CommonInit(suites, progress_indicator, context)

  def _CommonInit(self, suites, progress_indicator, context):
    self.total = 0
    for s in suites:
      for t in s.tests:
        t.id = self.total
        self.total += 1
    self.indicator = progress_indicator
    progress_indicator.SetRunner(self)
    self.context = context
    self.succeeded = 0
    self.remaining = self.total
    self.failed = []
    self.crashed = 0
    self.reran_tests = 0

  def _RunPerfSafe(self, fun):
    try:
      fun()
    except Exception, e:
      print("PerfData exception: %s" % e)
      self.perf_failures = True

  def _MaybeRerun(self, pool, test):
    if test.run <= self.context.rerun_failures_count:
      # Possibly rerun this test if its run count is below the maximum per
      # test. <= as the flag controls reruns not including the first run.
      if test.run == 1:
        # Count the overall number of reran tests on the first rerun.
        if self.reran_tests < self.context.rerun_failures_max:
          self.reran_tests += 1
        else:
          # Don't rerun this if the overall number of rerun tests has been
          # reached.
          return
      if test.run >= 2 and test.duration > self.context.timeout / 20.0:
        # Rerun slow tests at most once.
        return

      # Rerun this test.
      test.duration = None
      test.output = None
      test.run += 1
      pool.add([TestJob(test)])
      self.remaining += 1
      self.total += 1

  def _ProcessTestNormal(self, test, result, pool):
    test.output = result[1]
    test.duration = result[2]
    has_unexpected_output = test.suite.HasUnexpectedOutput(test)
    if has_unexpected_output:
      self.failed.append(test)
      if test.output.HasCrashed():
        self.crashed += 1
    else:
      self.succeeded += 1
    self.remaining -= 1
    # For the indicator, everything that happens after the first run is treated
    # as unexpected even if it flakily passes in order to include it in the
    # output.
    self.indicator.HasRun(test, has_unexpected_output or test.run > 1)
    if has_unexpected_output:
      # Rerun test failures after the indicator has processed the results.
      self._VerbosePrint("Attempting to rerun test after failure.")
      self._MaybeRerun(pool, test)
    # Update the perf database if the test succeeded.
    return not has_unexpected_output

  def _ProcessTestPredictable(self, test, result, pool):
    def HasDifferentAllocations(output1, output2):
      def AllocationStr(stdout):
        for line in reversed((stdout or "").splitlines()):
          if line.startswith("### Allocations = "):
            self.printed_allocations = True
            return line
        return ""
      return (AllocationStr(output1.stdout) != AllocationStr(output2.stdout))

    # Always pass the test duration for the database update.
    test.duration = result[2]
    if test.run == 1 and result[1].HasTimedOut():
      # If we get a timeout in the first run, we are already in an
      # unpredictable state. Just report it as a failure and don't rerun.
      test.output = result[1]
      self.remaining -= 1
      self.failed.append(test)
      self.indicator.HasRun(test, True)
    if test.run > 1 and HasDifferentAllocations(test.output, result[1]):
      # From the second run on, check for different allocations. If a
      # difference is found, call the indicator twice to report both tests.
      # All runs of each test are counted as one for the statistic.
      self.remaining -= 1
      self.failed.append(test)
      self.indicator.HasRun(test, True)
      test.output = result[1]
      self.indicator.HasRun(test, True)
    elif test.run >= 3:
      # No difference on the third run -> report a success.
      self.remaining -= 1
      self.succeeded += 1
      test.output = result[1]
      self.indicator.HasRun(test, False)
    else:
      # No difference yet and less than three runs -> add another run and
      # remember the output for comparison.
      test.run += 1
      test.output = result[1]
      pool.add([TestJob(test)])
    # Always update the perf database.
    return True

  def Run(self, jobs):
    self.indicator.Starting()
    self._RunInternal(jobs)
    self.indicator.Done()
    if self.failed:
      return 1
    elif self.remaining:
      return 2
    return 0

  def _RunInternal(self, jobs):
    pool = Pool(jobs)
    test_map = {}
    queued_exception = [None]
    def gen_tests():
      for test in self.tests:
        assert test.id >= 0
        test_map[test.id] = test
        try:
          yield [TestJob(test)]
        except Exception, e:
          # If this failed, save the exception and re-raise it later (after
          # all other tests have had a chance to run).
          queued_exception[0] = e
          continue
    try:
      it = pool.imap_unordered(
          fn=RunTest,
          gen=gen_tests(),
          process_context_fn=MakeProcessContext,
          process_context_args=[self.context, self.suite_names],
      )
      for result in it:
        if result.heartbeat:
          self.indicator.Heartbeat()
          continue
        test = test_map[result.value[0]]
        if self.context.predictable:
          update_perf = self._ProcessTestPredictable(test, result.value, pool)
        else:
          update_perf = self._ProcessTestNormal(test, result.value, pool)
        if update_perf:
          self._RunPerfSafe(lambda: self.perfdata.UpdatePerfData(test))
    finally:
      self._VerbosePrint("Closing process pool.")
      pool.terminate()
      self._VerbosePrint("Closing database connection.")
      self._RunPerfSafe(lambda: self.perf_data_manager.close())
      if self.perf_failures:
        # Nuke perf data in case of failures. This might not work on windows as
        # some files might still be open.
        print "Deleting perf test data due to db corruption."
        shutil.rmtree(self.datapath)
    if queued_exception[0]:
      raise queued_exception[0]

    # Make sure that any allocations were printed in predictable mode (if we
    # ran any tests).
    assert (
        not self.total or
        not self.context.predictable or
        self.printed_allocations
    )

  def _VerbosePrint(self, text):
    if self.context.verbose:
      print text
      sys.stdout.flush()


class BreakNowException(Exception):
  def __init__(self, value):
    self.value = value
  def __str__(self):
    return repr(self.value)

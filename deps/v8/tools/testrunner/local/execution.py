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
import traceback

from . import command
from . import perfdata
from . import statusfile
from . import utils
from . pool import Pool
from ..objects import predictable


# Base dir of the v8 checkout.
BASE_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))
TEST_DIR = os.path.join(BASE_DIR, "test")


# Structure that keeps global information per worker process.
ProcessContext = collections.namedtuple(
    'process_context', ['sancov_dir'])


TestJobResult = collections.namedtuple(
    'TestJobResult', ['id', 'outproc_result'])

def MakeProcessContext(sancov_dir):
  return ProcessContext(sancov_dir)


# Global function for multiprocessing, because pickling a static method doesn't
# work on Windows.
def run_job(job, process_context):
  return job.run(process_context)


class Job(object):
  """Stores data to be sent over the multi-process boundary.

  All contained fields will be pickled/unpickled.
  """

  def run(self, process_context):
    raise NotImplementedError()


class TestJob(Job):
  def __init__(self, test_id, cmd, outproc, run_num):
    self.test_id = test_id
    self.cmd = cmd
    self.outproc = outproc
    self.run_num = run_num

  def _rename_coverage_data(self, out, sancov_dir):
    """Rename coverage data.

    Rename files with PIDs to files with unique test IDs, because the number
    of tests might be higher than pid_max. E.g.:
    d8.1234.sancov -> d8.test.42.1.sancov, where 1234 was the process' PID,
    42 is the test ID and 1 is the attempt (the same test might be rerun on
    failures).
    """
    if sancov_dir and out.pid is not None:
      # Doesn't work on windows so basename is sufficient to get the shell name.
      shell = os.path.basename(self.cmd.shell)
      sancov_file = os.path.join(sancov_dir, "%s.%d.sancov" % (shell, out.pid))

      # Some tests are expected to fail and don't produce coverage data.
      if os.path.exists(sancov_file):
        parts = sancov_file.split(".")
        new_sancov_file = ".".join(
            parts[:-2] +
            ["test", str(self.test_id), str(self.run_num)] +
            parts[-1:]
        )
        assert not os.path.exists(new_sancov_file)
        os.rename(sancov_file, new_sancov_file)

  def run(self, context):
    output = self.cmd.execute()
    self._rename_coverage_data(output, context.sancov_dir)
    return TestJobResult(self.test_id, self.outproc.process(output))


class Runner(object):

  def __init__(self, suites, progress_indicator, context, outproc_factory=None):
    self.datapath = os.path.join("out", "testrunner_data")
    self.perf_data_manager = perfdata.GetPerfDataManager(
        context, self.datapath)
    self.perfdata = self.perf_data_manager.GetStore(context.arch, context.mode)
    self.perf_failures = False
    self.printed_allocations = False
    self.outproc_factory = outproc_factory or (lambda test: test.output_proc)
    self.tests = [t for s in suites for t in s.tests]

    # TODO(majeski): Pass dynamically instead of keeping them in the runner.
    # Maybe some observer?
    self.outputs = {t: None for t in self.tests}

    self.suite_names = [s.name for s in suites]

    # Always pre-sort by status file, slowest tests first.
    self.tests.sort(key=lambda t: t.is_slow, reverse=True)

    # Sort by stored duration if not opted out.
    if not context.no_sorting:
      self.tests.sort(key=lambda t: self.perfdata.FetchPerfData(t) or 1.0,
                      reverse=True)

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

  def _MaybeRerun(self, pool, test, result):
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
      if (test.run >= 2 and
          result.output.duration > self.context.timeout / 20.0):
        # Rerun slow tests at most once.
        return

      # Rerun this test.
      test.run += 1
      pool.add([
          TestJob(test.id, test.cmd, self.outproc_factory(test), test.run)
      ])
      self.remaining += 1
      self.total += 1

  def _ProcessTest(self, test, result, pool):
    self.outputs[test] = result.output
    has_unexpected_output = result.has_unexpected_output
    if has_unexpected_output:
      self.failed.append(test)
      if result.output.HasCrashed():
        self.crashed += 1
    else:
      self.succeeded += 1
    self.remaining -= 1
    # For the indicator, everything that happens after the first run is treated
    # as unexpected even if it flakily passes in order to include it in the
    # output.
    self.indicator.HasRun(test, result.output,
                          has_unexpected_output or test.run > 1)
    if has_unexpected_output:
      # Rerun test failures after the indicator has processed the results.
      self._VerbosePrint("Attempting to rerun test after failure.")
      self._MaybeRerun(pool, test, result)
    # Update the perf database if the test succeeded.
    return not has_unexpected_output

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
          yield [
              TestJob(test.id, test.cmd, self.outproc_factory(test), test.run)
          ]
        except Exception, e:
          # If this failed, save the exception and re-raise it later (after
          # all other tests have had a chance to run).
          queued_exception[0] = e, traceback.format_exc()
          continue
    try:
      it = pool.imap_unordered(
          fn=run_job,
          gen=gen_tests(),
          process_context_fn=MakeProcessContext,
          process_context_args=[self.context.sancov_dir],
      )
      for result in it:
        if result.heartbeat:
          self.indicator.Heartbeat()
          continue

        job_result = result.value
        test_id = job_result.id
        outproc_result = job_result.outproc_result

        test = test_map[test_id]
        update_perf = self._ProcessTest(test, outproc_result, pool)
        if update_perf:
          self._RunPerfSafe(lambda: self.perfdata.UpdatePerfData(
              test, outproc_result.output.duration))
    except KeyboardInterrupt:
      raise
    except:
      traceback.print_exc()
      raise
    finally:
      self._VerbosePrint("Closing process pool.")
      pool.terminate()
      self._VerbosePrint("Closing database connection.")
      self._RunPerfSafe(self.perf_data_manager.close)
      if self.perf_failures:
        # Nuke perf data in case of failures. This might not work on windows as
        # some files might still be open.
        print "Deleting perf test data due to db corruption."
        shutil.rmtree(self.datapath)
    if queued_exception[0]:
      e, stacktrace = queued_exception[0]
      print stacktrace
      raise e

  def _VerbosePrint(self, text):
    if self.context.verbose:
      print text
      sys.stdout.flush()


class BreakNowException(Exception):
  def __init__(self, value):
    super(BreakNowException, self).__init__()
    self.value = value

  def __str__(self):
    return repr(self.value)

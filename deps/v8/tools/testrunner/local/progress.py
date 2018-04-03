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


from functools import wraps
import json
import os
import sys
import time

from . import junit_output
from . import statusfile
from ..testproc import progress as progress_proc


class ProgressIndicator(object):

  def __init__(self):
    self.runner = None

  def SetRunner(self, runner):
    self.runner = runner

  def Starting(self):
    pass

  def Done(self):
    pass

  def HasRun(self, test, output, has_unexpected_output):
    pass

  def Heartbeat(self):
    pass

  def PrintFailureHeader(self, test):
    if test.output_proc.negative:
      negative_marker = '[negative] '
    else:
      negative_marker = ''
    print "=== %(label)s %(negative)s===" % {
      'label': test,
      'negative': negative_marker,
    }

  def ToProgressIndicatorProc(self):
    print ('Warning: %s is not available as a processor' %
           self.__class__.__name__)
    return None


class IndicatorNotifier(object):
  """Holds a list of progress indicators and notifies them all on events."""
  def __init__(self):
    self.indicators = []

  def Register(self, indicator):
    self.indicators.append(indicator)

  def ToProgressIndicatorProcs(self):
    return [i.ToProgressIndicatorProc() for i in self.indicators]


# Forge all generic event-dispatching methods in IndicatorNotifier, which are
# part of the ProgressIndicator interface.
for func_name in ProgressIndicator.__dict__:
  func = getattr(ProgressIndicator, func_name)
  if callable(func) and not func.__name__.startswith('_'):
    def wrap_functor(f):
      @wraps(f)
      def functor(self, *args, **kwargs):
        """Generic event dispatcher."""
        for indicator in self.indicators:
          getattr(indicator, f.__name__)(*args, **kwargs)
      return functor
    setattr(IndicatorNotifier, func_name, wrap_functor(func))


class SimpleProgressIndicator(ProgressIndicator):
  """Abstract base class for {Verbose,Dots}ProgressIndicator"""

  def Starting(self):
    print 'Running %i tests' % self.runner.total

  def Done(self):
    print
    for failed in self.runner.failed:
      output = self.runner.outputs[failed]
      self.PrintFailureHeader(failed)
      if output.stderr:
        print "--- stderr ---"
        print output.stderr.strip()
      if output.stdout:
        print "--- stdout ---"
        print output.stdout.strip()
      print "Command: %s" % failed.cmd.to_string()
      if output.HasCrashed():
        print "exit code: %d" % output.exit_code
        print "--- CRASHED ---"
      if output.HasTimedOut():
        print "--- TIMEOUT ---"
    if len(self.runner.failed) == 0:
      print "==="
      print "=== All tests succeeded"
      print "==="
    else:
      print
      print "==="
      print "=== %i tests failed" % len(self.runner.failed)
      if self.runner.crashed > 0:
        print "=== %i tests CRASHED" % self.runner.crashed
      print "==="


class VerboseProgressIndicator(SimpleProgressIndicator):

  def HasRun(self, test, output, has_unexpected_output):
    if has_unexpected_output:
      if output.HasCrashed():
        outcome = 'CRASH'
      else:
        outcome = 'FAIL'
    else:
      outcome = 'pass'
    print 'Done running %s: %s' % (test, outcome)
    sys.stdout.flush()

  def Heartbeat(self):
    print 'Still working...'
    sys.stdout.flush()

  def ToProgressIndicatorProc(self):
    return progress_proc.VerboseProgressIndicator()


class DotsProgressIndicator(SimpleProgressIndicator):

  def HasRun(self, test, output, has_unexpected_output):
    total = self.runner.succeeded + len(self.runner.failed)
    if (total > 1) and (total % 50 == 1):
      sys.stdout.write('\n')
    if has_unexpected_output:
      if output.HasCrashed():
        sys.stdout.write('C')
        sys.stdout.flush()
      elif output.HasTimedOut():
        sys.stdout.write('T')
        sys.stdout.flush()
      else:
        sys.stdout.write('F')
        sys.stdout.flush()
    else:
      sys.stdout.write('.')
      sys.stdout.flush()

  def ToProgressIndicatorProc(self):
    return progress_proc.DotsProgressIndicator()


class CompactProgressIndicator(ProgressIndicator):
  """Abstract base class for {Color,Monochrome}ProgressIndicator"""

  def __init__(self, templates):
    super(CompactProgressIndicator, self).__init__()
    self.templates = templates
    self.last_status_length = 0
    self.start_time = time.time()

  def Done(self):
    self.PrintProgress('Done')
    print ""  # Line break.

  def HasRun(self, test, output, has_unexpected_output):
    self.PrintProgress(str(test))
    if has_unexpected_output:
      self.ClearLine(self.last_status_length)
      self.PrintFailureHeader(test)
      stdout = output.stdout.strip()
      if len(stdout):
        print self.templates['stdout'] % stdout
      stderr = output.stderr.strip()
      if len(stderr):
        print self.templates['stderr'] % stderr
      print "Command: %s" % test.cmd.to_string()
      if output.HasCrashed():
        print "exit code: %d" % output.exit_code
        print "--- CRASHED ---"
      if output.HasTimedOut():
        print "--- TIMEOUT ---"

  def Truncate(self, string, length):
    if length and (len(string) > (length - 3)):
      return string[:(length - 3)] + "..."
    else:
      return string

  def PrintProgress(self, name):
    self.ClearLine(self.last_status_length)
    elapsed = time.time() - self.start_time
    progress = 0 if not self.runner.total else (
        ((self.runner.total - self.runner.remaining) * 100) //
          self.runner.total)
    status = self.templates['status_line'] % {
      'passed': self.runner.succeeded,
      'progress': progress,
      'failed': len(self.runner.failed),
      'test': name,
      'mins': int(elapsed) / 60,
      'secs': int(elapsed) % 60
    }
    status = self.Truncate(status, 78)
    self.last_status_length = len(status)
    print status,
    sys.stdout.flush()


class ColorProgressIndicator(CompactProgressIndicator):

  def __init__(self):
    templates = {
      'status_line': ("[%(mins)02i:%(secs)02i|"
                      "\033[34m%%%(progress) 4d\033[0m|"
                      "\033[32m+%(passed) 4d\033[0m|"
                      "\033[31m-%(failed) 4d\033[0m]: %(test)s"),
      'stdout': "\033[1m%s\033[0m",
      'stderr': "\033[31m%s\033[0m",
    }
    super(ColorProgressIndicator, self).__init__(templates)

  def ClearLine(self, last_line_length):
    print "\033[1K\r",

  def ToProgressIndicatorProc(self):
    return progress_proc.ColorProgressIndicator()


class MonochromeProgressIndicator(CompactProgressIndicator):

  def __init__(self):
    templates = {
      'status_line': ("[%(mins)02i:%(secs)02i|%%%(progress) 4d|"
                      "+%(passed) 4d|-%(failed) 4d]: %(test)s"),
      'stdout': '%s',
      'stderr': '%s',
    }
    super(MonochromeProgressIndicator, self).__init__(templates)

  def ClearLine(self, last_line_length):
    print ("\r" + (" " * last_line_length) + "\r"),

  def ToProgressIndicatorProc(self):
    return progress_proc.MonochromeProgressIndicator()


class JUnitTestProgressIndicator(ProgressIndicator):
  def __init__(self, junitout, junittestsuite):
    super(JUnitTestProgressIndicator, self).__init__()
    self.junitout = junitout
    self.juinttestsuite = junittestsuite
    self.outputter = junit_output.JUnitTestOutput(junittestsuite)
    if junitout:
      self.outfile = open(junitout, "w")
    else:
      self.outfile = sys.stdout

  def Done(self):
    self.outputter.FinishAndWrite(self.outfile)
    if self.outfile != sys.stdout:
      self.outfile.close()

  def HasRun(self, test, output, has_unexpected_output):
    fail_text = ""
    if has_unexpected_output:
      stdout = output.stdout.strip()
      if len(stdout):
        fail_text += "stdout:\n%s\n" % stdout
      stderr = output.stderr.strip()
      if len(stderr):
        fail_text += "stderr:\n%s\n" % stderr
      fail_text += "Command: %s" % test.cmd.to_string()
      if output.HasCrashed():
        fail_text += "exit code: %d\n--- CRASHED ---" % output.exit_code
      if output.HasTimedOut():
        fail_text += "--- TIMEOUT ---"
    self.outputter.HasRunTest(
        test_name=str(test),
        test_cmd=test.cmd.to_string(relative=True),
        test_duration=output.duration,
        test_failure=fail_text)

  def ToProgressIndicatorProc(self):
    if self.outfile != sys.stdout:
      self.outfile.close()
    return progress_proc.JUnitTestProgressIndicator(self.junitout,
                                                    self.junittestsuite)


class JsonTestProgressIndicator(ProgressIndicator):

  def __init__(self, json_test_results, arch, mode, random_seed):
    super(JsonTestProgressIndicator, self).__init__()
    self.json_test_results = json_test_results
    self.arch = arch
    self.mode = mode
    self.random_seed = random_seed
    self.results = []
    self.tests = []

  def ToProgressIndicatorProc(self):
    return progress_proc.JsonTestProgressIndicator(
        self.json_test_results, self.arch, self.mode, self.random_seed)

  def Done(self):
    complete_results = []
    if os.path.exists(self.json_test_results):
      with open(self.json_test_results, "r") as f:
        # Buildbot might start out with an empty file.
        complete_results = json.loads(f.read() or "[]")

    duration_mean = None
    if self.tests:
      # Get duration mean.
      duration_mean = (
          sum(duration for (_, duration) in self.tests) /
          float(len(self.tests)))

    # Sort tests by duration.
    self.tests.sort(key=lambda (_, duration): duration, reverse=True)
    slowest_tests = [
      {
        "name": str(test),
        "flags": test.cmd.args,
        "command": test.cmd.to_string(relative=True),
        "duration": duration,
        "marked_slow": test.is_slow,
      } for (test, duration) in self.tests[:20]
    ]

    complete_results.append({
      "arch": self.arch,
      "mode": self.mode,
      "results": self.results,
      "slowest_tests": slowest_tests,
      "duration_mean": duration_mean,
      "test_total": len(self.tests),
    })

    with open(self.json_test_results, "w") as f:
      f.write(json.dumps(complete_results))

  def HasRun(self, test, output, has_unexpected_output):
    # Buffer all tests for sorting the durations in the end.
    self.tests.append((test, output.duration))
    if not has_unexpected_output:
      # Omit tests that run as expected. Passing tests of reruns after failures
      # will have unexpected_output to be reported here has well.
      return

    self.results.append({
      "name": str(test),
      "flags": test.cmd.args,
      "command": test.cmd.to_string(relative=True),
      "run": test.run,
      "stdout": output.stdout,
      "stderr": output.stderr,
      "exit_code": output.exit_code,
      "result": test.output_proc.get_outcome(output),
      "expected": test.expected_outcomes,
      "duration": output.duration,

      # TODO(machenbach): This stores only the global random seed from the
      # context and not possible overrides when using random-seed stress.
      "random_seed": self.random_seed,
      "target_name": test.get_shell(),
      "variant": test.variant,
    })


class FlakinessTestProgressIndicator(ProgressIndicator):

  def __init__(self, json_test_results):
    super(FlakinessTestProgressIndicator, self).__init__()
    self.json_test_results = json_test_results
    self.results = {}
    self.summary = {
      "PASS": 0,
      "FAIL": 0,
      "CRASH": 0,
      "TIMEOUT": 0,
    }
    self.seconds_since_epoch = time.time()

  def Done(self):
    with open(self.json_test_results, "w") as f:
      json.dump({
        "interrupted": False,
        "num_failures_by_type": self.summary,
        "path_delimiter": "/",
        "seconds_since_epoch": self.seconds_since_epoch,
        "tests": self.results,
        "version": 3,
      }, f)

  def HasRun(self, test, output, has_unexpected_output):
    key = test.get_id()
    outcome = test.output_proc.get_outcome(output)
    assert outcome in ["PASS", "FAIL", "CRASH", "TIMEOUT"]
    if test.run == 1:
      # First run of this test.
      self.results[key] = {
        "actual": outcome,
        "expected": " ".join(test.expected_outcomes),
        "times": [output.duration],
      }
      self.summary[outcome] = self.summary[outcome] + 1
    else:
      # This is a rerun and a previous result exists.
      result = self.results[key]
      result["actual"] = "%s %s" % (result["actual"], outcome)
      result["times"].append(output.duration)


PROGRESS_INDICATORS = {
  'verbose': VerboseProgressIndicator,
  'dots': DotsProgressIndicator,
  'color': ColorProgressIndicator,
  'mono': MonochromeProgressIndicator
}

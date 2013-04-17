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


import sys
import time

from . import junit_output

def EscapeCommand(command):
  parts = []
  for part in command:
    if ' ' in part:
      # Escape spaces.  We may need to escape more characters for this
      # to work properly.
      parts.append('"%s"' % part)
    else:
      parts.append(part)
  return " ".join(parts)


class ProgressIndicator(object):

  def __init__(self):
    self.runner = None

  def Starting(self):
    pass

  def Done(self):
    pass

  def AboutToRun(self, test):
    pass

  def HasRun(self, test):
    pass

  def PrintFailureHeader(self, test):
    if test.suite.IsNegativeTest(test):
      negative_marker = '[negative] '
    else:
      negative_marker = ''
    print "=== %(label)s %(negative)s===" % {
      'label': test.GetLabel(),
      'negative': negative_marker
    }


class SimpleProgressIndicator(ProgressIndicator):
  """Abstract base class for {Verbose,Dots}ProgressIndicator"""

  def Starting(self):
    print 'Running %i tests' % self.runner.total

  def Done(self):
    print
    for failed in self.runner.failed:
      self.PrintFailureHeader(failed)
      if failed.output.stderr:
        print "--- stderr ---"
        print failed.output.stderr.strip()
      if failed.output.stdout:
        print "--- stdout ---"
        print failed.output.stdout.strip()
      print "Command: %s" % EscapeCommand(self.runner.GetCommand(failed))
      if failed.output.HasCrashed():
        print "--- CRASHED ---"
      if failed.output.HasTimedOut():
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

  def AboutToRun(self, test):
    print 'Starting %s...' % test.GetLabel()
    sys.stdout.flush()

  def HasRun(self, test):
    if test.suite.HasUnexpectedOutput(test):
      if test.output.HasCrashed():
        outcome = 'CRASH'
      else:
        outcome = 'FAIL'
    else:
      outcome = 'pass'
    print 'Done running %s: %s' % (test.GetLabel(), outcome)


class DotsProgressIndicator(SimpleProgressIndicator):

  def HasRun(self, test):
    total = self.runner.succeeded + len(self.runner.failed)
    if (total > 1) and (total % 50 == 1):
      sys.stdout.write('\n')
    if test.suite.HasUnexpectedOutput(test):
      if test.output.HasCrashed():
        sys.stdout.write('C')
        sys.stdout.flush()
      elif test.output.HasTimedOut():
        sys.stdout.write('T')
        sys.stdout.flush()
      else:
        sys.stdout.write('F')
        sys.stdout.flush()
    else:
      sys.stdout.write('.')
      sys.stdout.flush()


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

  def AboutToRun(self, test):
    self.PrintProgress(test.GetLabel())

  def HasRun(self, test):
    if test.suite.HasUnexpectedOutput(test):
      self.ClearLine(self.last_status_length)
      self.PrintFailureHeader(test)
      stdout = test.output.stdout.strip()
      if len(stdout):
        print self.templates['stdout'] % stdout
      stderr = test.output.stderr.strip()
      if len(stderr):
        print self.templates['stderr'] % stderr
      print "Command: %s" % EscapeCommand(self.runner.GetCommand(test))
      if test.output.HasCrashed():
        print "exit code: %d" % test.output.exit_code
        print "--- CRASHED ---"
      if test.output.HasTimedOut():
        print "--- TIMEOUT ---"

  def Truncate(self, string, length):
    if length and (len(string) > (length - 3)):
      return string[:(length - 3)] + "..."
    else:
      return string

  def PrintProgress(self, name):
    self.ClearLine(self.last_status_length)
    elapsed = time.time() - self.start_time
    status = self.templates['status_line'] % {
      'passed': self.runner.succeeded,
      'remaining': (((self.runner.total - self.runner.remaining) * 100) //
                    self.runner.total),
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
                      "\033[34m%%%(remaining) 4d\033[0m|"
                      "\033[32m+%(passed) 4d\033[0m|"
                      "\033[31m-%(failed) 4d\033[0m]: %(test)s"),
      'stdout': "\033[1m%s\033[0m",
      'stderr': "\033[31m%s\033[0m",
    }
    super(ColorProgressIndicator, self).__init__(templates)

  def ClearLine(self, last_line_length):
    print "\033[1K\r",


class MonochromeProgressIndicator(CompactProgressIndicator):

  def __init__(self):
    templates = {
      'status_line': ("[%(mins)02i:%(secs)02i|%%%(remaining) 4d|"
                      "+%(passed) 4d|-%(failed) 4d]: %(test)s"),
      'stdout': '%s',
      'stderr': '%s',
    }
    super(MonochromeProgressIndicator, self).__init__(templates)

  def ClearLine(self, last_line_length):
    print ("\r" + (" " * last_line_length) + "\r"),


class JUnitTestProgressIndicator(ProgressIndicator):

  def __init__(self, progress_indicator, junitout, junittestsuite):
    self.progress_indicator = progress_indicator
    self.outputter = junit_output.JUnitTestOutput(junittestsuite)
    if junitout:
      self.outfile = open(junitout, "w")
    else:
      self.outfile = sys.stdout

  def Starting(self):
    self.progress_indicator.runner = self.runner
    self.progress_indicator.Starting()

  def Done(self):
    self.progress_indicator.Done()
    self.outputter.FinishAndWrite(self.outfile)
    if self.outfile != sys.stdout:
      self.outfile.close()

  def AboutToRun(self, test):
    self.progress_indicator.AboutToRun(test)

  def HasRun(self, test):
    self.progress_indicator.HasRun(test)
    fail_text = ""
    if test.suite.HasUnexpectedOutput(test):
      stdout = test.output.stdout.strip()
      if len(stdout):
        fail_text += "stdout:\n%s\n" % stdout
      stderr = test.output.stderr.strip()
      if len(stderr):
        fail_text += "stderr:\n%s\n" % stderr
      fail_text += "Command: %s" % EscapeCommand(self.runner.GetCommand(test))
      if test.output.HasCrashed():
        fail_text += "exit code: %d\n--- CRASHED ---" % test.output.exit_code
      if test.output.HasTimedOut():
        fail_text += "--- TIMEOUT ---"
    self.outputter.HasRunTest(
        [test.GetLabel()] + self.runner.context.mode_flags + test.flags,
        test.duration,
        fail_text)


PROGRESS_INDICATORS = {
  'verbose': VerboseProgressIndicator,
  'dots': DotsProgressIndicator,
  'color': ColorProgressIndicator,
  'mono': MonochromeProgressIndicator
}

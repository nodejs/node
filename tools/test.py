#!/usr/bin/env python
#
# Copyright 2008 the V8 project authors. All rights reserved.
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


import imp
import optparse
import os
from os.path import join, dirname, abspath, basename, isdir, exists
import platform
import re
import signal
import subprocess
import sys
import tempfile
import time
import threading
from Queue import Queue, Empty

sys.path.append(dirname(__file__) + "/../deps/v8/tools");
import utils

VERBOSE = False


# ---------------------------------------------
# --- P r o g r e s s   I n d i c a t o r s ---
# ---------------------------------------------


class ProgressIndicator(object):

  def __init__(self, cases):
    self.cases = cases
    self.queue = Queue(len(cases))
    for case in cases:
      self.queue.put_nowait(case)
    self.succeeded = 0
    self.remaining = len(cases)
    self.total = len(cases)
    self.failed = [ ]
    self.crashed = 0
    self.terminate = False
    self.lock = threading.Lock()

  def PrintFailureHeader(self, test):
    if test.IsNegative():
      negative_marker = '[negative] '
    else:
      negative_marker = ''
    print "=== %(label)s %(negative)s===" % {
      'label': test.GetLabel(),
      'negative': negative_marker
    }
    print "Path: %s" % "/".join(test.path)

  def Run(self, tasks):
    self.Starting()
    threads = []
    # Spawn N-1 threads and then use this thread as the last one.
    # That way -j1 avoids threading altogether which is a nice fallback
    # in case of threading problems.
    for i in xrange(tasks - 1):
      thread = threading.Thread(target=self.RunSingle, args=[])
      threads.append(thread)
      thread.start()
    try:
      self.RunSingle()
      # Wait for the remaining threads
      for thread in threads:
        # Use a timeout so that signals (ctrl-c) will be processed.
        thread.join(timeout=10000000)
    except Exception, e:
      # If there's an exception we schedule an interruption for any
      # remaining threads.
      self.terminate = True
      # ...and then reraise the exception to bail out
      raise
    self.Done()
    return not self.failed

  def RunSingle(self):
    while not self.terminate:
      try:
        test = self.queue.get_nowait()
      except Empty:
        return
      case = test.case
      self.lock.acquire()
      self.AboutToRun(case)
      self.lock.release()
      try:
        start = time.time()
        output = case.Run()
        case.duration = (time.time() - start)
      except IOError, e:
        assert self.terminate
        return
      if self.terminate:
        return
      self.lock.acquire()
      if output.UnexpectedOutput():
        self.failed.append(output)
        if output.HasCrashed():
          self.crashed += 1
      else:
        self.succeeded += 1
      self.remaining -= 1
      self.HasRun(output)
      self.lock.release()


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


class SimpleProgressIndicator(ProgressIndicator):

  def Starting(self):
    print 'Running %i tests' % len(self.cases)

  def Done(self):
    print
    for failed in self.failed:
      self.PrintFailureHeader(failed.test)
      if failed.output.stderr:
        print "--- stderr ---"
        print failed.output.stderr.strip()
      if failed.output.stdout:
        print "--- stdout ---"
        print failed.output.stdout.strip()
      print "Command: %s" % EscapeCommand(failed.command)
      if failed.HasCrashed():
        print "--- CRASHED ---"
      if failed.HasTimedOut():
        print "--- TIMEOUT ---"
    if len(self.failed) == 0:
      print "==="
      print "=== All tests succeeded"
      print "==="
    else:
      print
      print "==="
      print "=== %i tests failed" % len(self.failed)
      if self.crashed > 0:
        print "=== %i tests CRASHED" % self.crashed
      print "==="


class VerboseProgressIndicator(SimpleProgressIndicator):

  def AboutToRun(self, case):
    print 'Starting %s...' % case.GetLabel()
    sys.stdout.flush()

  def HasRun(self, output):
    if output.UnexpectedOutput():
      if output.HasCrashed():
        outcome = 'CRASH'
      else:
        outcome = 'FAIL'
    else:
      outcome = 'pass'
    print 'Done running %s: %s' % (output.test.GetLabel(), outcome)


class DotsProgressIndicator(SimpleProgressIndicator):

  def AboutToRun(self, case):
    pass

  def HasRun(self, output):
    total = self.succeeded + len(self.failed)
    if (total > 1) and (total % 50 == 1):
      sys.stdout.write('\n')
    if output.UnexpectedOutput():
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


class CompactProgressIndicator(ProgressIndicator):

  def __init__(self, cases, templates):
    super(CompactProgressIndicator, self).__init__(cases)
    self.templates = templates
    self.last_status_length = 0
    self.start_time = time.time()

  def Starting(self):
    pass

  def Done(self):
    self.PrintProgress('Done')

  def AboutToRun(self, case):
    self.PrintProgress(case.GetLabel())

  def HasRun(self, output):
    if output.UnexpectedOutput():
      self.ClearLine(self.last_status_length)
      self.PrintFailureHeader(output.test)
      stdout = output.output.stdout.strip()
      if len(stdout):
        print self.templates['stdout'] % stdout
      stderr = output.output.stderr.strip()
      if len(stderr):
        print self.templates['stderr'] % stderr
      print "Command: %s" % EscapeCommand(output.command)
      if output.HasCrashed():
        print "--- CRASHED ---"
      if output.HasTimedOut():
        print "--- TIMEOUT ---"

  def Truncate(self, str, length):
    if length and (len(str) > (length - 3)):
      return str[:(length-3)] + "..."
    else:
      return str

  def PrintProgress(self, name):
    self.ClearLine(self.last_status_length)
    elapsed = time.time() - self.start_time
    status = self.templates['status_line'] % {
      'passed': self.succeeded,
      'remaining': (((self.total - self.remaining) * 100) // self.total),
      'failed': len(self.failed),
      'test': name,
      'mins': int(elapsed) / 60,
      'secs': int(elapsed) % 60
    }
    status = self.Truncate(status, 78)
    self.last_status_length = len(status)
    print status,
    sys.stdout.flush()


class ColorProgressIndicator(CompactProgressIndicator):

  def __init__(self, cases):
    templates = {
      'status_line': "[%(mins)02i:%(secs)02i|\033[34m%%%(remaining) 4d\033[0m|\033[32m+%(passed) 4d\033[0m|\033[31m-%(failed) 4d\033[0m]: %(test)s",
      'stdout': "\033[1m%s\033[0m",
      'stderr': "\033[31m%s\033[0m",
    }
    super(ColorProgressIndicator, self).__init__(cases, templates)

  def ClearLine(self, last_line_length):
    print "\033[1K\r",


class MonochromeProgressIndicator(CompactProgressIndicator):

  def __init__(self, cases):
    templates = {
      'status_line': "[%(mins)02i:%(secs)02i|%%%(remaining) 4d|+%(passed) 4d|-%(failed) 4d]: %(test)s",
      'stdout': '%s',
      'stderr': '%s',
      'clear': lambda last_line_length: ("\r" + (" " * last_line_length) + "\r"),
      'max_length': 78
    }
    super(MonochromeProgressIndicator, self).__init__(cases, templates)

  def ClearLine(self, last_line_length):
    print ("\r" + (" " * last_line_length) + "\r"),


PROGRESS_INDICATORS = {
  'verbose': VerboseProgressIndicator,
  'dots': DotsProgressIndicator,
  'color': ColorProgressIndicator,
  'mono': MonochromeProgressIndicator
}


# -------------------------
# --- F r a m e w o r k ---
# -------------------------


class CommandOutput(object):

  def __init__(self, exit_code, timed_out, stdout, stderr):
    self.exit_code = exit_code
    self.timed_out = timed_out
    self.stdout = stdout
    self.stderr = stderr
    self.failed = None


class TestCase(object):

  def __init__(self, context, path, mode):
    self.path = path
    self.context = context
    self.duration = None
    self.mode = mode

  def IsNegative(self):
    return False

  def CompareTime(self, other):
    return cmp(other.duration, self.duration)

  def DidFail(self, output):
    if output.failed is None:
      output.failed = self.IsFailureOutput(output)
    return output.failed

  def IsFailureOutput(self, output):
    return output.exit_code != 0

  def GetSource(self):
    return "(no source available)"

  def RunCommand(self, command):
    full_command = self.context.processor(command)
    output = Execute(full_command,
                     self.context,
                     self.context.GetTimeout(self.mode))
    self.Cleanup()
    return TestOutput(self,
                      full_command,
                      output,
                      self.context.store_unexpected_output)

  def BeforeRun(self):
    pass

  def AfterRun(self, result):
    pass

  def Run(self):
    self.BeforeRun()
    try:
      result = self.RunCommand(self.GetCommand())
    finally:
      self.AfterRun(result)
    return result

  def Cleanup(self):
    return


class TestOutput(object):

  def __init__(self, test, command, output, store_unexpected_output):
    self.test = test
    self.command = command
    self.output = output
    self.store_unexpected_output = store_unexpected_output

  def UnexpectedOutput(self):
    if self.HasCrashed():
      outcome = CRASH
    elif self.HasTimedOut():
      outcome = TIMEOUT
    elif self.HasFailed():
      outcome = FAIL
    else:
      outcome = PASS
    return not outcome in self.test.outcomes

  def HasPreciousOutput(self):
    return self.UnexpectedOutput() and self.store_unexpected_output

  def HasCrashed(self):
    if utils.IsWindows():
      return 0x80000000 & self.output.exit_code and not (0x3FFFFF00 & self.output.exit_code)
    else:
      # Timed out tests will have exit_code -signal.SIGTERM.
      if self.output.timed_out:
        return False
      return self.output.exit_code < 0 and \
             self.output.exit_code != -signal.SIGABRT

  def HasTimedOut(self):
    return self.output.timed_out;

  def HasFailed(self):
    execution_failed = self.test.DidFail(self.output)
    if self.test.IsNegative():
      return not execution_failed
    else:
      return execution_failed


def KillProcessWithID(pid):
  if utils.IsWindows():
    os.popen('taskkill /T /F /PID %d' % pid)
  else:
    os.kill(pid, signal.SIGTERM)


MAX_SLEEP_TIME = 0.1
INITIAL_SLEEP_TIME = 0.0001
SLEEP_TIME_FACTOR = 1.25

SEM_INVALID_VALUE = -1
SEM_NOGPFAULTERRORBOX = 0x0002 # Microsoft Platform SDK WinBase.h

def Win32SetErrorMode(mode):
  prev_error_mode = SEM_INVALID_VALUE
  try:
    import ctypes
    prev_error_mode = ctypes.windll.kernel32.SetErrorMode(mode);
  except ImportError:
    pass
  return prev_error_mode

def RunProcess(context, timeout, args, **rest):
  if context.verbose: print "#", " ".join(args)
  popen_args = args
  prev_error_mode = SEM_INVALID_VALUE;
  if utils.IsWindows():
    if context.suppress_dialogs:
      # Try to change the error mode to avoid dialogs on fatal errors. Don't
      # touch any existing error mode flags by merging the existing error mode.
      # See http://blogs.msdn.com/oldnewthing/archive/2004/07/27/198410.aspx.
      error_mode = SEM_NOGPFAULTERRORBOX;
      prev_error_mode = Win32SetErrorMode(error_mode);
      Win32SetErrorMode(error_mode | prev_error_mode);
  process = subprocess.Popen(
    shell = utils.IsWindows(),
    args = popen_args,
    **rest
  )
  if utils.IsWindows() and context.suppress_dialogs and prev_error_mode != SEM_INVALID_VALUE:
    Win32SetErrorMode(prev_error_mode)
  # Compute the end time - if the process crosses this limit we
  # consider it timed out.
  if timeout is None: end_time = None
  else: end_time = time.time() + timeout
  timed_out = False
  # Repeatedly check the exit code from the process in a
  # loop and keep track of whether or not it times out.
  exit_code = None
  sleep_time = INITIAL_SLEEP_TIME
  while exit_code is None:
    if (not end_time is None) and (time.time() >= end_time):
      # Kill the process and wait for it to exit.
      KillProcessWithID(process.pid)
      exit_code = process.wait()
      timed_out = True
    else:
      exit_code = process.poll()
      time.sleep(sleep_time)
      sleep_time = sleep_time * SLEEP_TIME_FACTOR
      if sleep_time > MAX_SLEEP_TIME:
        sleep_time = MAX_SLEEP_TIME
  return (process, exit_code, timed_out)


def PrintError(str):
  sys.stderr.write(str)
  sys.stderr.write('\n')


def CheckedUnlink(name):
  try:
    os.unlink(name)
  except OSError, e:
    PrintError("os.unlink() " + str(e))


def Execute(args, context, timeout=None):
  (fd_out, outname) = tempfile.mkstemp()
  (fd_err, errname) = tempfile.mkstemp()
  (process, exit_code, timed_out) = RunProcess(
    context,
    timeout,
    args = args,
    stdout = fd_out,
    stderr = fd_err,
  )
  os.close(fd_out)
  os.close(fd_err)
  output = file(outname).read()
  errors = file(errname).read()
  CheckedUnlink(outname)
  CheckedUnlink(errname)
  return CommandOutput(exit_code, timed_out, output, errors)


def ExecuteNoCapture(args, context, timeout=None):
  (process, exit_code, timed_out) = RunProcess(
    context,
    timeout,
    args = args,
  )
  return CommandOutput(exit_code, False, "", "")


def CarCdr(path):
  if len(path) == 0:
    return (None, [ ])
  else:
    return (path[0], path[1:])


class TestConfiguration(object):

  def __init__(self, context, root):
    self.context = context
    self.root = root

  def Contains(self, path, file):
    if len(path) > len(file):
      return False
    for i in xrange(len(path)):
      if not path[i].match(file[i]):
        return False
    return True

  def GetTestStatus(self, sections, defs):
    pass


class TestSuite(object):

  def __init__(self, name):
    self.name = name

  def GetName(self):
    return self.name


# Use this to run several variants of the tests, e.g.:
# VARIANT_FLAGS = [[], ['--always_compact', '--noflush_code']]
VARIANT_FLAGS = [[]]


class TestRepository(TestSuite):

  def __init__(self, path):
    normalized_path = abspath(path)
    super(TestRepository, self).__init__(basename(normalized_path))
    self.path = normalized_path
    self.is_loaded = False
    self.config = None

  def GetConfiguration(self, context):
    if self.is_loaded:
      return self.config
    self.is_loaded = True
    file = None
    try:
      (file, pathname, description) = imp.find_module('testcfg', [ self.path ])
      module = imp.load_module('testcfg', file, pathname, description)
      self.config = module.GetConfiguration(context, self.path)
    finally:
      if file:
        file.close()
    return self.config

  def GetBuildRequirements(self, path, context):
    return self.GetConfiguration(context).GetBuildRequirements()

  def AddTestsToList(self, result, current_path, path, context, mode):
    for v in VARIANT_FLAGS:
      tests = self.GetConfiguration(context).ListTests(current_path, path, mode)
      for t in tests: t.variant_flags = v
      result += tests


  def GetTestStatus(self, context, sections, defs):
    self.GetConfiguration(context).GetTestStatus(sections, defs)


class LiteralTestSuite(TestSuite):

  def __init__(self, tests):
    super(LiteralTestSuite, self).__init__('root')
    self.tests = tests

  def GetBuildRequirements(self, path, context):
    (name, rest) = CarCdr(path)
    result = [ ]
    for test in self.tests:
      if not name or name.match(test.GetName()):
        result += test.GetBuildRequirements(rest, context)
    return result

  def ListTests(self, current_path, path, context, mode):
    (name, rest) = CarCdr(path)
    result = [ ]
    for test in self.tests:
      test_name = test.GetName()
      if not name or name.match(test_name):
        full_path = current_path + [test_name]
        test.AddTestsToList(result, full_path, path, context, mode)
    return result

  def GetTestStatus(self, context, sections, defs):
    for test in self.tests:
      test.GetTestStatus(context, sections, defs)


SUFFIX = {
    'debug'   : '_g',
    'release' : '' }
FLAGS = {
    'debug'   : ['--enable-slow-asserts', '--debug-code', '--verify-heap'],
    'release' : []}
TIMEOUT_SCALEFACTOR = {
    'debug'   : 4,
    'release' : 1 }


class Context(object):

  def __init__(self, workspace, buildspace, verbose, vm, timeout, processor, suppress_dialogs, store_unexpected_output):
    self.workspace = workspace
    self.buildspace = buildspace
    self.verbose = verbose
    self.vm_root = vm
    self.timeout = timeout
    self.processor = processor
    self.suppress_dialogs = suppress_dialogs
    self.store_unexpected_output = store_unexpected_output

  def GetVm(self, mode):
    if mode == 'debug':
      name = 'out/Debug/node'
    else:
      name = 'out/Release/node'

    # Currently GYP does not support output_dir for MSVS.
    # http://code.google.com/p/gyp/issues/detail?id=40
    # It will put the builds into Release/node.exe or Debug/node.exe
    if utils.IsWindows():
      out_dir = os.path.join(dirname(__file__), "..", "out")
      if not exists(out_dir):
        if mode == 'debug':
          name = os.path.abspath('Debug/node.exe')
        else:
          name = os.path.abspath('Release/node.exe')
      else:
        name = os.path.abspath(name + '.exe')

    return name

  def GetVmCommand(self, testcase, mode):
    return [self.GetVm(mode)] + self.GetVmFlags(testcase, mode)

  def GetVmFlags(self, testcase, mode):
    return testcase.variant_flags + FLAGS[mode]

  def GetTimeout(self, mode):
    return self.timeout * TIMEOUT_SCALEFACTOR[mode]

def RunTestCases(cases_to_run, progress, tasks):
  progress = PROGRESS_INDICATORS[progress](cases_to_run)
  return progress.Run(tasks)


def BuildRequirements(context, requirements, mode, scons_flags):
  command_line = (['scons', '-Y', context.workspace, 'mode=' + ",".join(mode)]
                  + requirements
                  + scons_flags)
  output = ExecuteNoCapture(command_line, context)
  return output.exit_code == 0


# -------------------------------------------
# --- T e s t   C o n f i g u r a t i o n ---
# -------------------------------------------


SKIP = 'skip'
FAIL = 'fail'
PASS = 'pass'
OKAY = 'okay'
TIMEOUT = 'timeout'
CRASH = 'crash'
SLOW = 'slow'


class Expression(object):
  pass


class Constant(Expression):

  def __init__(self, value):
    self.value = value

  def Evaluate(self, env, defs):
    return self.value


class Variable(Expression):

  def __init__(self, name):
    self.name = name

  def GetOutcomes(self, env, defs):
    if self.name in env: return ListSet([env[self.name]])
    else: return Nothing()


class Outcome(Expression):

  def __init__(self, name):
    self.name = name

  def GetOutcomes(self, env, defs):
    if self.name in defs:
      return defs[self.name].GetOutcomes(env, defs)
    else:
      return ListSet([self.name])


class Set(object):
  pass


class ListSet(Set):

  def __init__(self, elms):
    self.elms = elms

  def __str__(self):
    return "ListSet%s" % str(self.elms)

  def Intersect(self, that):
    if not isinstance(that, ListSet):
      return that.Intersect(self)
    return ListSet([ x for x in self.elms if x in that.elms ])

  def Union(self, that):
    if not isinstance(that, ListSet):
      return that.Union(self)
    return ListSet(self.elms + [ x for x in that.elms if x not in self.elms ])

  def IsEmpty(self):
    return len(self.elms) == 0


class Everything(Set):

  def Intersect(self, that):
    return that

  def Union(self, that):
    return self

  def IsEmpty(self):
    return False


class Nothing(Set):

  def Intersect(self, that):
    return self

  def Union(self, that):
    return that

  def IsEmpty(self):
    return True


class Operation(Expression):

  def __init__(self, left, op, right):
    self.left = left
    self.op = op
    self.right = right

  def Evaluate(self, env, defs):
    if self.op == '||' or self.op == ',':
      return self.left.Evaluate(env, defs) or self.right.Evaluate(env, defs)
    elif self.op == 'if':
      return False
    elif self.op == '==':
      inter = self.left.GetOutcomes(env, defs).Intersect(self.right.GetOutcomes(env, defs))
      return not inter.IsEmpty()
    else:
      assert self.op == '&&'
      return self.left.Evaluate(env, defs) and self.right.Evaluate(env, defs)

  def GetOutcomes(self, env, defs):
    if self.op == '||' or self.op == ',':
      return self.left.GetOutcomes(env, defs).Union(self.right.GetOutcomes(env, defs))
    elif self.op == 'if':
      if self.right.Evaluate(env, defs): return self.left.GetOutcomes(env, defs)
      else: return Nothing()
    else:
      assert self.op == '&&'
      return self.left.GetOutcomes(env, defs).Intersect(self.right.GetOutcomes(env, defs))


def IsAlpha(str):
  for char in str:
    if not (char.isalpha() or char.isdigit() or char == '_'):
      return False
  return True


class Tokenizer(object):
  """A simple string tokenizer that chops expressions into variables,
  parens and operators"""

  def __init__(self, expr):
    self.index = 0
    self.expr = expr
    self.length = len(expr)
    self.tokens = None

  def Current(self, length = 1):
    if not self.HasMore(length): return ""
    return self.expr[self.index:self.index+length]

  def HasMore(self, length = 1):
    return self.index < self.length + (length - 1)

  def Advance(self, count = 1):
    self.index = self.index + count

  def AddToken(self, token):
    self.tokens.append(token)

  def SkipSpaces(self):
    while self.HasMore() and self.Current().isspace():
      self.Advance()

  def Tokenize(self):
    self.tokens = [ ]
    while self.HasMore():
      self.SkipSpaces()
      if not self.HasMore():
        return None
      if self.Current() == '(':
        self.AddToken('(')
        self.Advance()
      elif self.Current() == ')':
        self.AddToken(')')
        self.Advance()
      elif self.Current() == '$':
        self.AddToken('$')
        self.Advance()
      elif self.Current() == ',':
        self.AddToken(',')
        self.Advance()
      elif IsAlpha(self.Current()):
        buf = ""
        while self.HasMore() and IsAlpha(self.Current()):
          buf += self.Current()
          self.Advance()
        self.AddToken(buf)
      elif self.Current(2) == '&&':
        self.AddToken('&&')
        self.Advance(2)
      elif self.Current(2) == '||':
        self.AddToken('||')
        self.Advance(2)
      elif self.Current(2) == '==':
        self.AddToken('==')
        self.Advance(2)
      else:
        return None
    return self.tokens


class Scanner(object):
  """A simple scanner that can serve out tokens from a given list"""

  def __init__(self, tokens):
    self.tokens = tokens
    self.length = len(tokens)
    self.index = 0

  def HasMore(self):
    return self.index < self.length

  def Current(self):
    return self.tokens[self.index]

  def Advance(self):
    self.index = self.index + 1


def ParseAtomicExpression(scan):
  if scan.Current() == "true":
    scan.Advance()
    return Constant(True)
  elif scan.Current() == "false":
    scan.Advance()
    return Constant(False)
  elif IsAlpha(scan.Current()):
    name = scan.Current()
    scan.Advance()
    return Outcome(name.lower())
  elif scan.Current() == '$':
    scan.Advance()
    if not IsAlpha(scan.Current()):
      return None
    name = scan.Current()
    scan.Advance()
    return Variable(name.lower())
  elif scan.Current() == '(':
    scan.Advance()
    result = ParseLogicalExpression(scan)
    if (not result) or (scan.Current() != ')'):
      return None
    scan.Advance()
    return result
  else:
    return None


BINARIES = ['==']
def ParseOperatorExpression(scan):
  left = ParseAtomicExpression(scan)
  if not left: return None
  while scan.HasMore() and (scan.Current() in BINARIES):
    op = scan.Current()
    scan.Advance()
    right = ParseOperatorExpression(scan)
    if not right:
      return None
    left = Operation(left, op, right)
  return left


def ParseConditionalExpression(scan):
  left = ParseOperatorExpression(scan)
  if not left: return None
  while scan.HasMore() and (scan.Current() == 'if'):
    scan.Advance()
    right = ParseOperatorExpression(scan)
    if not right:
      return None
    left=  Operation(left, 'if', right)
  return left


LOGICALS = ["&&", "||", ","]
def ParseLogicalExpression(scan):
  left = ParseConditionalExpression(scan)
  if not left: return None
  while scan.HasMore() and (scan.Current() in LOGICALS):
    op = scan.Current()
    scan.Advance()
    right = ParseConditionalExpression(scan)
    if not right:
      return None
    left = Operation(left, op, right)
  return left


def ParseCondition(expr):
  """Parses a logical expression into an Expression object"""
  tokens = Tokenizer(expr).Tokenize()
  if not tokens:
    print "Malformed expression: '%s'" % expr
    return None
  scan = Scanner(tokens)
  ast = ParseLogicalExpression(scan)
  if not ast:
    print "Malformed expression: '%s'" % expr
    return None
  if scan.HasMore():
    print "Malformed expression: '%s'" % expr
    return None
  return ast


class ClassifiedTest(object):

  def __init__(self, case, outcomes):
    self.case = case
    self.outcomes = outcomes


class Configuration(object):
  """The parsed contents of a configuration file"""

  def __init__(self, sections, defs):
    self.sections = sections
    self.defs = defs

  def ClassifyTests(self, cases, env):
    sections = [s for s in self.sections if s.condition.Evaluate(env, self.defs)]
    all_rules = reduce(list.__add__, [s.rules for s in sections], [])
    unused_rules = set(all_rules)
    result = [ ]
    all_outcomes = set([])
    for case in cases:
      matches = [ r for r in all_rules if r.Contains(case.path) ]
      outcomes = set([])
      for rule in matches:
        outcomes = outcomes.union(rule.GetOutcomes(env, self.defs))
        unused_rules.discard(rule)
      if not outcomes:
        outcomes = [PASS]
      case.outcomes = outcomes
      all_outcomes = all_outcomes.union(outcomes)
      result.append(ClassifiedTest(case, outcomes))
    return (result, list(unused_rules), all_outcomes)


class Section(object):
  """A section of the configuration file.  Sections are enabled or
  disabled prior to running the tests, based on their conditions"""

  def __init__(self, condition):
    self.condition = condition
    self.rules = [ ]

  def AddRule(self, rule):
    self.rules.append(rule)


class Rule(object):
  """A single rule that specifies the expected outcome for a single
  test."""

  def __init__(self, raw_path, path, value):
    self.raw_path = raw_path
    self.path = path
    self.value = value

  def GetOutcomes(self, env, defs):
    set = self.value.GetOutcomes(env, defs)
    assert isinstance(set, ListSet)
    return set.elms

  def Contains(self, path):
    if len(self.path) > len(path):
      return False
    for i in xrange(len(self.path)):
      if not self.path[i].match(path[i]):
        return False
    return True


HEADER_PATTERN = re.compile(r'\[([^]]+)\]')
RULE_PATTERN = re.compile(r'\s*([^: ]*)\s*:(.*)')
DEF_PATTERN = re.compile(r'^def\s*(\w+)\s*=(.*)$')
PREFIX_PATTERN = re.compile(r'^\s*prefix\s+([\w\_\.\-\/]+)$')


def ReadConfigurationInto(path, sections, defs):
  current_section = Section(Constant(True))
  sections.append(current_section)
  prefix = []
  for line in utils.ReadLinesFrom(path):
    header_match = HEADER_PATTERN.match(line)
    if header_match:
      condition_str = header_match.group(1).strip()
      condition = ParseCondition(condition_str)
      new_section = Section(condition)
      sections.append(new_section)
      current_section = new_section
      continue
    rule_match = RULE_PATTERN.match(line)
    if rule_match:
      path = prefix + SplitPath(rule_match.group(1).strip())
      value_str = rule_match.group(2).strip()
      value = ParseCondition(value_str)
      if not value:
        return False
      current_section.AddRule(Rule(rule_match.group(1), path, value))
      continue
    def_match = DEF_PATTERN.match(line)
    if def_match:
      name = def_match.group(1).lower()
      value = ParseCondition(def_match.group(2).strip())
      if not value:
        return False
      defs[name] = value
      continue
    prefix_match = PREFIX_PATTERN.match(line)
    if prefix_match:
      prefix = SplitPath(prefix_match.group(1).strip())
      continue
    print "Malformed line: '%s'." % line
    return False
  return True


# ---------------
# --- M a i n ---
# ---------------


ARCH_GUESS = utils.GuessArchitecture()


def BuildOptions():
  result = optparse.OptionParser()
  result.add_option("-m", "--mode", help="The test modes in which to run (comma-separated)",
      default='release')
  result.add_option("-v", "--verbose", help="Verbose output",
      default=False, action="store_true")
  result.add_option("-S", dest="scons_flags", help="Flag to pass through to scons",
      default=[], action="append")
  result.add_option("-p", "--progress",
      help="The style of progress indicator (verbose, dots, color, mono)",
      choices=PROGRESS_INDICATORS.keys(), default="mono")
  result.add_option("--no-build", help="Don't build requirements",
      default=True, action="store_true")
  result.add_option("--build-only", help="Only build requirements, don't run the tests",
      default=False, action="store_true")
  result.add_option("--report", help="Print a summary of the tests to be run",
      default=False, action="store_true")
  result.add_option("-s", "--suite", help="A test suite",
      default=[], action="append")
  result.add_option("-t", "--timeout", help="Timeout in seconds",
      default=60, type="int")
  result.add_option("--arch", help='The architecture to run tests for',
      default='none')
  result.add_option("--snapshot", help="Run the tests with snapshot turned on",
      default=False, action="store_true")
  result.add_option("--simulator", help="Run tests with architecture simulator",
      default='none')
  result.add_option("--special-command", default=None)
  result.add_option("--use-http1", help="Pass --use-http1 switch to node",
      default=False, action="store_true")
  result.add_option("--valgrind", help="Run tests through valgrind",
      default=False, action="store_true")
  result.add_option("--cat", help="Print the source of the tests",
      default=False, action="store_true")
  result.add_option("--warn-unused", help="Report unused rules",
      default=False, action="store_true")
  result.add_option("-j", help="The number of parallel tasks to run",
      default=1, type="int")
  result.add_option("--time", help="Print timing information after running",
      default=False, action="store_true")
  result.add_option("--suppress-dialogs", help="Suppress Windows dialogs for crashing tests",
        dest="suppress_dialogs", default=True, action="store_true")
  result.add_option("--no-suppress-dialogs", help="Display Windows dialogs for crashing tests",
        dest="suppress_dialogs", action="store_false")
  result.add_option("--shell", help="Path to V8 shell", default="shell")
  result.add_option("--store-unexpected-output", 
      help="Store the temporary JS files from tests that fails",
      dest="store_unexpected_output", default=True, action="store_true")
  result.add_option("--no-store-unexpected-output", 
      help="Deletes the temporary JS files from tests that fails",
      dest="store_unexpected_output", action="store_false")
  return result


def ProcessOptions(options):
  global VERBOSE
  VERBOSE = options.verbose
  options.mode = options.mode.split(',')
  for mode in options.mode:
    if not mode in ['debug', 'release']:
      print "Unknown mode %s" % mode
      return False
  if options.simulator != 'none':
    # Simulator argument was set. Make sure arch and simulator agree.
    if options.simulator != options.arch:
      if options.arch == 'none':
        options.arch = options.simulator
      else:
        print "Architecture %s does not match sim %s" %(options.arch, options.simulator)
        return False
    # Ensure that the simulator argument is handed down to scons.
    options.scons_flags.append("simulator=" + options.simulator)
  else:
    # If options.arch is not set by the command line and no simulator setting
    # was found, set the arch to the guess.
    if options.arch == 'none':
      options.arch = ARCH_GUESS
    options.scons_flags.append("arch=" + options.arch)
  if options.snapshot:
    options.scons_flags.append("snapshot=on")
  return True


REPORT_TEMPLATE = """\
Total: %(total)i tests
 * %(skipped)4d tests will be skipped
 * %(nocrash)4d tests are expected to be flaky but not crash
 * %(pass)4d tests are expected to pass
 * %(fail_ok)4d tests are expected to fail that we won't fix
 * %(fail)4d tests are expected to fail that we should fix\
"""

def PrintReport(cases):
  def IsFlaky(o):
    return (PASS in o) and (FAIL in o) and (not CRASH in o) and (not OKAY in o)
  def IsFailOk(o):
    return (len(o) == 2) and (FAIL in o) and (OKAY in o)
  unskipped = [c for c in cases if not SKIP in c.outcomes]
  print REPORT_TEMPLATE % {
    'total': len(cases),
    'skipped': len(cases) - len(unskipped),
    'nocrash': len([t for t in unskipped if IsFlaky(t.outcomes)]),
    'pass': len([t for t in unskipped if list(t.outcomes) == [PASS]]),
    'fail_ok': len([t for t in unskipped if IsFailOk(t.outcomes)]),
    'fail': len([t for t in unskipped if list(t.outcomes) == [FAIL]])
  }


class Pattern(object):

  def __init__(self, pattern):
    self.pattern = pattern
    self.compiled = None

  def match(self, str):
    if not self.compiled:
      pattern = "^" + self.pattern.replace('*', '.*') + "$"
      self.compiled = re.compile(pattern)
    return self.compiled.match(str)

  def __str__(self):
    return self.pattern


def SplitPath(s):
  stripped = [ c.strip() for c in s.split('/') ]
  return [ Pattern(s) for s in stripped if len(s) > 0 ]


def GetSpecialCommandProcessor(value):
  if (not value) or (value.find('@') == -1):
    def ExpandCommand(args):
      return args
    return ExpandCommand
  else:
    pos = value.find('@')
    import urllib
    prefix = urllib.unquote(value[:pos]).split()
    suffix = urllib.unquote(value[pos+1:]).split()
    def ExpandCommand(args):
      return prefix + args + suffix
    return ExpandCommand


BUILT_IN_TESTS = ['simple', 'pummel', 'message', 'internet']


def GetSuites(test_root):
  def IsSuite(path):
    return isdir(path) and exists(join(path, 'testcfg.py'))
  return [ f for f in os.listdir(test_root) if IsSuite(join(test_root, f)) ]


def FormatTime(d):
  millis = round(d * 1000) % 1000
  return time.strftime("%M:%S.", time.gmtime(d)) + ("%03i" % millis)


def Main():
  parser = BuildOptions()
  (options, args) = parser.parse_args()
  if not ProcessOptions(options):
    parser.print_help()
    return 1

  workspace = abspath(join(dirname(sys.argv[0]), '..'))
  suites = GetSuites(join(workspace, 'test'))
  repositories = [TestRepository(join(workspace, 'test', name)) for name in suites]
  repositories += [TestRepository(a) for a in options.suite]

  root = LiteralTestSuite(repositories)
  if len(args) == 0:
    paths = [SplitPath(t) for t in BUILT_IN_TESTS]
  else:
    paths = [ ]
    for arg in args:
      path = SplitPath(arg)
      paths.append(path)

  # Check for --valgrind option. If enabled, we overwrite the special
  # command flag with a command that uses the run-valgrind.py script.
  if options.valgrind:
    run_valgrind = join(workspace, "tools", "run-valgrind.py")
    options.special_command = "python -u " + run_valgrind + " @"

  shell = abspath(options.shell)
  buildspace = dirname(shell)

  processor = GetSpecialCommandProcessor(options.special_command)
  if options.use_http1:
    def wrap(processor):
      return lambda args: processor(args[:1] + ['--use-http1'] + args[1:])
    processor = wrap(processor)

  context = Context(workspace,
                    buildspace,
                    VERBOSE,
                    shell,
                    options.timeout,
                    processor,
                    options.suppress_dialogs,
                    options.store_unexpected_output)
  # First build the required targets
  if not options.no_build:
    reqs = [ ]
    for path in paths:
      reqs += root.GetBuildRequirements(path, context)
    reqs = list(set(reqs))
    if len(reqs) > 0:
      if options.j != 1:
        options.scons_flags += ['-j', str(options.j)]
      if not BuildRequirements(context, reqs, options.mode, options.scons_flags):
        return 1

  # Just return if we are only building the targets for running the tests.
  if options.build_only:
    return 0

  # Get status for tests
  sections = [ ]
  defs = { }
  root.GetTestStatus(context, sections, defs)
  config = Configuration(sections, defs)

  # List the tests
  all_cases = [ ]
  all_unused = [ ]
  unclassified_tests = [ ]
  globally_unused_rules = None
  for path in paths:
    for mode in options.mode:
      if not exists(context.GetVm(mode)):
        print "Can't find shell executable: '%s'" % context.GetVm(mode)
        continue
      env = {
        'mode': mode,
        'system': utils.GuessOS(),
        'arch': options.arch,
        'simulator': options.simulator
      }
      test_list = root.ListTests([], path, context, mode)
      unclassified_tests += test_list
      (cases, unused_rules, all_outcomes) = config.ClassifyTests(test_list, env)
      if globally_unused_rules is None:
        globally_unused_rules = set(unused_rules)
      else:
        globally_unused_rules = globally_unused_rules.intersection(unused_rules)
      all_cases += cases
      all_unused.append(unused_rules)

  if options.cat:
    visited = set()
    for test in unclassified_tests:
      key = tuple(test.path)
      if key in visited:
        continue
      visited.add(key)
      print "--- begin source: %s ---" % test.GetLabel()
      source = test.GetSource().strip()
      print source
      print "--- end source: %s ---" % test.GetLabel()
    return 0

  if options.warn_unused:
    for rule in globally_unused_rules:
      print "Rule for '%s' was not used." % '/'.join([str(s) for s in rule.path])

  if options.report:
    PrintReport(all_cases)

  result = None
  def DoSkip(case):
    return SKIP in case.outcomes or SLOW in case.outcomes
  cases_to_run = [ c for c in all_cases if not DoSkip(c) ]
  if len(cases_to_run) == 0:
    print "No tests to run."
    return 0
  else:
    try:
      start = time.time()
      if RunTestCases(cases_to_run, options.progress, options.j):
        result = 0
      else:
        result = 1
      duration = time.time() - start
    except KeyboardInterrupt:
      print "Interrupted"
      return 1

  if options.time:
    # Write the times to stderr to make it easy to separate from the
    # test output.
    print
    sys.stderr.write("--- Total time: %s ---\n" % FormatTime(duration))
    timed_tests = [ t.case for t in cases_to_run if not t.case.duration is None ]
    timed_tests.sort(lambda a, b: a.CompareTime(b))
    index = 1
    for entry in timed_tests[:20]:
      t = FormatTime(entry.duration)
      sys.stderr.write("%4i (%s) %s\n" % (index, t, entry.GetLabel()))
      index += 1

  return result


if __name__ == '__main__':
  sys.exit(Main())

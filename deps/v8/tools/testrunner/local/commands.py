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


import os
import subprocess
import sys
from threading import Timer

from ..local import utils
from ..objects import output


SEM_INVALID_VALUE = -1
SEM_NOGPFAULTERRORBOX = 0x0002  # Microsoft Platform SDK WinBase.h


def Win32SetErrorMode(mode):
  prev_error_mode = SEM_INVALID_VALUE
  try:
    import ctypes
    prev_error_mode = \
        ctypes.windll.kernel32.SetErrorMode(mode)  #@UndefinedVariable
  except ImportError:
    pass
  return prev_error_mode


def RunProcess(verbose, timeout, args, additional_env, **rest):
  if verbose: print "#", " ".join(args)
  popen_args = args
  prev_error_mode = SEM_INVALID_VALUE
  if utils.IsWindows():
    popen_args = subprocess.list2cmdline(args)
    # Try to change the error mode to avoid dialogs on fatal errors. Don't
    # touch any existing error mode flags by merging the existing error mode.
    # See http://blogs.msdn.com/oldnewthing/archive/2004/07/27/198410.aspx.
    error_mode = SEM_NOGPFAULTERRORBOX
    prev_error_mode = Win32SetErrorMode(error_mode)
    Win32SetErrorMode(error_mode | prev_error_mode)

  env = os.environ.copy()
  env.update(additional_env)
  # GTest shard information is read by the V8 tests runner. Make sure it
  # doesn't leak into the execution of gtests we're wrapping. Those might
  # otherwise apply a second level of sharding and as a result skip tests.
  env.pop('GTEST_TOTAL_SHARDS', None)
  env.pop('GTEST_SHARD_INDEX', None)

  try:
    process = subprocess.Popen(
      args=popen_args,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      env=env,
      **rest
    )
  except Exception as e:
    sys.stderr.write("Error executing: %s\n" % popen_args)
    raise e

  if (utils.IsWindows() and prev_error_mode != SEM_INVALID_VALUE):
    Win32SetErrorMode(prev_error_mode)

  def kill_process(process, timeout_result):
    timeout_result[0] = True
    try:
      if utils.IsWindows():
        if verbose:
          print "Attempting to kill process %d" % process.pid
          sys.stdout.flush()
        tk = subprocess.Popen(
            'taskkill /T /F /PID %d' % process.pid,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        stdout, stderr = tk.communicate()
        if verbose:
          print "Taskkill results for %d" % process.pid
          print stdout
          print stderr
          print "Return code: %d" % tk.returncode
          sys.stdout.flush()
      else:
        if utils.GuessOS() == "macos":
          # TODO(machenbach): Temporary output for investigating hanging test
          # driver on mac.
          print "Attempting to kill process %d - cmd %s" % (process.pid, args)
          try:
            print subprocess.check_output(
              "ps -e | egrep 'd8|cctest|unittests'", shell=True)
          except Exception:
            pass
          sys.stdout.flush()
        process.kill()
        if utils.GuessOS() == "macos":
          # TODO(machenbach): Temporary output for investigating hanging test
          # driver on mac. This will probably not print much, since kill only
          # sends the signal.
          print "Return code after signalling the kill: %s" % process.returncode
          sys.stdout.flush()

    except OSError:
      sys.stderr.write('Error: Process %s already ended.\n' % process.pid)

  # Pseudo object to communicate with timer thread.
  timeout_result = [False]

  timer = Timer(timeout, kill_process, [process, timeout_result])
  timer.start()
  stdout, stderr = process.communicate()
  timer.cancel()

  return output.Output(
      process.returncode,
      timeout_result[0],
      stdout.decode('utf-8', 'replace').encode('utf-8'),
      stderr.decode('utf-8', 'replace').encode('utf-8'),
      process.pid,
  )


# TODO(machenbach): Instead of passing args around, we should introduce an
# immutable Command class (that just represents the command with all flags and
# is pretty-printable) and a member method for running such a command.
def Execute(args, verbose=False, timeout=None, env=None):
  args = [ c for c in args if c != "" ]
  return RunProcess(verbose, timeout, args, env or {})

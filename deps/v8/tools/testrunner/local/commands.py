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
import signal
import subprocess
import sys
import tempfile
import time

from ..local import utils
from ..objects import output


def KillProcessWithID(pid):
  if utils.IsWindows():
    os.popen('taskkill /T /F /PID %d' % pid)
  else:
    os.kill(pid, signal.SIGTERM)


MAX_SLEEP_TIME = 0.1
INITIAL_SLEEP_TIME = 0.0001
SLEEP_TIME_FACTOR = 1.25

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


def RunProcess(verbose, timeout, args, **rest):
  try:
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
    process = subprocess.Popen(
      shell=utils.IsWindows(),
      args=popen_args,
      **rest
    )
    if (utils.IsWindows() and prev_error_mode != SEM_INVALID_VALUE):
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
    return (exit_code, timed_out)
  except KeyboardInterrupt:
    raise


def PrintError(string):
  sys.stderr.write(string)
  sys.stderr.write("\n")


def CheckedUnlink(name):
  # On Windows, when run with -jN in parallel processes,
  # OS often fails to unlink the temp file. Not sure why.
  # Need to retry.
  # Idea from https://bugs.webkit.org/attachment.cgi?id=75982&action=prettypatch
  retry_count = 0
  while retry_count < 30:
    try:
      os.unlink(name)
      return
    except OSError, e:
      retry_count += 1
      time.sleep(retry_count * 0.1)
  PrintError("os.unlink() " + str(e))


def Execute(args, verbose=False, timeout=None):
  try:
    args = [ c for c in args if c != "" ]
    (fd_out, outname) = tempfile.mkstemp()
    (fd_err, errname) = tempfile.mkstemp()
    (exit_code, timed_out) = RunProcess(
      verbose,
      timeout,
      args=args,
      stdout=fd_out,
      stderr=fd_err
    )
  except KeyboardInterrupt:
    raise
  except:
    raise
  finally:
    os.close(fd_out)
    os.close(fd_err)
    out = file(outname).read()
    errors = file(errname).read()
    CheckedUnlink(outname)
    CheckedUnlink(errname)
  return output.Output(exit_code, timed_out, out, errors)

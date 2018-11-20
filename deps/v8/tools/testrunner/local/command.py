# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import os
import signal
import subprocess
import sys
import threading
import time

from ..local import utils
from ..objects import output


SEM_INVALID_VALUE = -1
SEM_NOGPFAULTERRORBOX = 0x0002  # Microsoft Platform SDK WinBase.h


def setup_testing():
  """For testing only: We use threading under the hood instead of
  multiprocessing to make coverage work. Signal handling is only supported
  in the main thread, so we disable it for testing.
  """
  signal.signal = lambda *_: None


class AbortException(Exception):
  """Indicates early abort on SIGINT, SIGTERM or internal hard timeout."""
  pass


class BaseCommand(object):
  def __init__(self, shell, args=None, cmd_prefix=None, timeout=60, env=None,
               verbose=False):
    assert(timeout > 0)

    self.shell = shell
    self.args = args or []
    self.cmd_prefix = cmd_prefix or []
    self.timeout = timeout
    self.env = env or {}
    self.verbose = verbose

  def execute(self, **additional_popen_kwargs):
    if self.verbose:
      print '# %s' % self

    process = self._start_process(**additional_popen_kwargs)

    # Variable to communicate with the signal handler.
    abort_occured = [False]
    def handler(signum, frame):
      self._abort(process, abort_occured)
    signal.signal(signal.SIGTERM, handler)

    # Variable to communicate with the timer.
    timeout_occured = [False]
    timer = threading.Timer(
        self.timeout, self._abort, [process, timeout_occured])
    timer.start()

    start_time = time.time()
    stdout, stderr = process.communicate()
    duration = time.time() - start_time

    timer.cancel()

    if abort_occured[0]:
      raise AbortException()

    return output.Output(
      process.returncode,
      timeout_occured[0],
      stdout.decode('utf-8', 'replace').encode('utf-8'),
      stderr.decode('utf-8', 'replace').encode('utf-8'),
      process.pid,
      duration
    )

  def _start_process(self, **additional_popen_kwargs):
    try:
      return subprocess.Popen(
        args=self._get_popen_args(),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=self._get_env(),
        **additional_popen_kwargs
      )
    except Exception as e:
      sys.stderr.write('Error executing: %s\n' % self)
      raise e

  def _get_popen_args(self):
    return self._to_args_list()

  def _get_env(self):
    env = os.environ.copy()
    env.update(self.env)
    # GTest shard information is read by the V8 tests runner. Make sure it
    # doesn't leak into the execution of gtests we're wrapping. Those might
    # otherwise apply a second level of sharding and as a result skip tests.
    env.pop('GTEST_TOTAL_SHARDS', None)
    env.pop('GTEST_SHARD_INDEX', None)
    return env

  def _kill_process(self, process):
    raise NotImplementedError()

  def _abort(self, process, abort_called):
    abort_called[0] = True
    try:
      self._kill_process(process)
    except OSError:
      pass

  def __str__(self):
    return self.to_string()

  def to_string(self, relative=False):
    def escape(part):
      # Escape spaces. We may need to escape more characters for this to work
      # properly.
      if ' ' in part:
        return '"%s"' % part
      return part

    parts = map(escape, self._to_args_list())
    cmd = ' '.join(parts)
    if relative:
      cmd = cmd.replace(os.getcwd() + os.sep, '')
    return cmd

  def _to_args_list(self):
    return self.cmd_prefix + [self.shell] + self.args


class PosixCommand(BaseCommand):
  def _kill_process(self, process):
    process.kill()


class WindowsCommand(BaseCommand):
  def _start_process(self, **kwargs):
    # Try to change the error mode to avoid dialogs on fatal errors. Don't
    # touch any existing error mode flags by merging the existing error mode.
    # See http://blogs.msdn.com/oldnewthing/archive/2004/07/27/198410.aspx.
    def set_error_mode(mode):
      prev_error_mode = SEM_INVALID_VALUE
      try:
        import ctypes
        prev_error_mode = (
            ctypes.windll.kernel32.SetErrorMode(mode))  #@UndefinedVariable
      except ImportError:
        pass
      return prev_error_mode

    error_mode = SEM_NOGPFAULTERRORBOX
    prev_error_mode = set_error_mode(error_mode)
    set_error_mode(error_mode | prev_error_mode)

    try:
      return super(WindowsCommand, self)._start_process(**kwargs)
    finally:
      if prev_error_mode != SEM_INVALID_VALUE:
        set_error_mode(prev_error_mode)

  def _get_popen_args(self):
    return subprocess.list2cmdline(self._to_args_list())

  def _kill_process(self, process):
    if self.verbose:
      print 'Attempting to kill process %d' % process.pid
      sys.stdout.flush()
    tk = subprocess.Popen(
        'taskkill /T /F /PID %d' % process.pid,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    stdout, stderr = tk.communicate()
    if self.verbose:
      print 'Taskkill results for %d' % process.pid
      print stdout
      print stderr
      print 'Return code: %d' % tk.returncode
      sys.stdout.flush()


# Set the Command class to the OS-specific version.
if utils.IsWindows():
  Command = WindowsCommand
else:
  Command = PosixCommand

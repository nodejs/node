# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# for py2/py3 compatibility
from __future__ import print_function

import os
import re
import signal
import subprocess
import sys
import threading
import time

from ..local.android import (
    android_driver, CommandFailedException, TimeoutException)
from ..local import utils
from ..objects import output


BASE_DIR = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..' , '..', '..'))

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
               verbose=False, resources_func=None):
    """Initialize the command.

    Args:
      shell: The name of the executable (e.g. d8).
      args: List of args to pass to the executable.
      cmd_prefix: Prefix of command (e.g. a wrapper script).
      timeout: Timeout in seconds.
      env: Environment dict for execution.
      verbose: Print additional output.
      resources_func: Callable, returning all test files needed by this command.
    """
    assert(timeout > 0)

    self.shell = shell
    self.args = args or []
    self.cmd_prefix = cmd_prefix or []
    self.timeout = timeout
    self.env = env or {}
    self.verbose = verbose

  def execute(self):
    if self.verbose:
      print('# %s' % self)

    process = self._start_process()

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

  def _start_process(self):
    try:
      return subprocess.Popen(
        args=self._get_popen_args(),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=self._get_env(),
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
      print('Attempting to kill process %s' % process.pid)
      sys.stdout.flush()
      self._kill_process(process)
    except OSError as e:
      print(e)
      started_as = self.to_string(relative=True)
      print("Unruly process started as:\n  %s\n" % started_as)
      sys.stdout.flush()
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
  # TODO(machenbach): Use base process start without shell once
  # https://crbug.com/v8/8889 is resolved.
  def _start_process(self):
    def wrapped(arg):
      if set('() \'"') & set(arg):
        return "'%s'" % arg.replace("'", "'\"'\"'")
      return arg
    try:
      return subprocess.Popen(
        args=' '.join(map(wrapped, self._get_popen_args())),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=self._get_env(),
        shell=True,
      )
    except Exception as e:
      sys.stderr.write('Error executing: %s\n' % self)
      raise e

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
    tk = subprocess.Popen(
        'taskkill /T /F /PID %d' % process.pid,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    stdout, stderr = tk.communicate()
    if self.verbose:
      print('Taskkill results for %d' % process.pid)
      print(stdout)
      print(stderr)
      print('Return code: %d' % tk.returncode)
      sys.stdout.flush()


class AndroidCommand(BaseCommand):
  # This must be initialized before creating any instances of this class.
  driver = None

  def __init__(self, shell, args=None, cmd_prefix=None, timeout=60, env=None,
               verbose=False, resources_func=None):
    """Initialize the command and all files that need to be pushed to the
    Android device.
    """
    self.shell_name = os.path.basename(shell)
    self.shell_dir = os.path.dirname(shell)
    self.files_to_push = (resources_func or (lambda: []))()

    # Make all paths in arguments relative and also prepare files from arguments
    # for pushing to the device.
    rel_args = []
    find_path_re = re.compile(r'.*(%s/[^\'"]+).*' % re.escape(BASE_DIR))
    for arg in (args or []):
      match = find_path_re.match(arg)
      if match:
        self.files_to_push.append(match.group(1))
      rel_args.append(
          re.sub(r'(.*)%s/(.*)' % re.escape(BASE_DIR), r'\1\2', arg))

    super(AndroidCommand, self).__init__(
        shell, args=rel_args, cmd_prefix=cmd_prefix, timeout=timeout, env=env,
        verbose=verbose)

  def execute(self, **additional_popen_kwargs):
    """Execute the command on the device.

    This pushes all required files to the device and then runs the command.
    """
    if self.verbose:
      print('# %s' % self)

    self.driver.push_executable(self.shell_dir, 'bin', self.shell_name)

    for abs_file in self.files_to_push:
      abs_dir = os.path.dirname(abs_file)
      file_name = os.path.basename(abs_file)
      rel_dir = os.path.relpath(abs_dir, BASE_DIR)
      self.driver.push_file(abs_dir, file_name, rel_dir)

    start_time = time.time()
    return_code = 0
    timed_out = False
    try:
      stdout = self.driver.run(
          'bin', self.shell_name, self.args, '.', self.timeout, self.env)
    except CommandFailedException as e:
      return_code = e.status
      stdout = e.output
    except TimeoutException as e:
      return_code = 1
      timed_out = True
      # Sadly the Android driver doesn't provide output on timeout.
      stdout = ''

    duration = time.time() - start_time
    return output.Output(
        return_code,
        timed_out,
        stdout,
        '',  # No stderr available.
        -1,  # No pid available.
        duration,
    )


Command = None
def setup(target_os, device):
  """Set the Command class to the OS-specific version."""
  global Command
  if target_os == 'android':
    AndroidCommand.driver = android_driver(device)
    Command = AndroidCommand
  elif target_os == 'windows':
    Command = WindowsCommand
  else:
    Command = PosixCommand

def tear_down():
  """Clean up after using commands."""
  if Command == AndroidCommand:
    AndroidCommand.driver.tear_down()

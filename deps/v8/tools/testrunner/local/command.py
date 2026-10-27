# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from contextlib import contextmanager
from pathlib import Path

import atexit
import logging
import os
import signal
import subprocess
import sys
import threading
import time

from ..local.android import (Driver, CommandFailedException, TimeoutException)
from ..local.pool import AbortException
from ..objects import output
from .process_utils import EMPTY_PROCESS_LOGGER, PROCESS_LOGGER

BASE_DIR = Path(__file__).resolve().parent.parent.parent.parent

SEM_INVALID_VALUE = -1
SEM_NOGPFAULTERRORBOX = 0x0002  # Microsoft Platform SDK WinBase.h


def setup_testing():
  """For testing only: We use threading under the hood instead of
  multiprocessing to make coverage work. Signal handling is only supported
  in the main thread, so we disable it for testing.
  """
  signal.signal = lambda *_: None

@contextmanager
def handle_sigterm(process, abort_fun, enabled):
  """Call`abort_fun` on sigterm and restore previous handler to prevent
  erroneous termination of an already terminated process.

  Args:
    process: The process to terminate.
    abort_fun: Function taking two parameters: the process to terminate and
        an array with a boolean for storing if an abort occured.
    enabled: If False, this wrapper will be a no-op.
  """
  # Variable to communicate with the signal handler.
  abort_occured = [False]

  if enabled:
    # TODO(https://crbug.com/v8/13113): There is a race condition on
    # signal handler registration. In rare cases, the SIGTERM for stopping
    # a worker might be caught right after a long running process has been
    # started (or logic that starts it isn't interrupted), but before the
    # registration of the abort_fun. In this case, process.communicate will
    # block until the process is done.
    previous = signal.getsignal(signal.SIGTERM)
    def handler(signum, frame):
      abort_fun(process, abort_occured)
      if previous and callable(previous):
        # Call default signal handler. If this command is called from a worker
        # process, its signal handler will gracefully stop processing.
        previous(signum, frame)
    signal.signal(signal.SIGTERM, handler)
  try:
    yield
  finally:
    if enabled:
      signal.signal(signal.SIGTERM, previous)

  if abort_occured[0]:
    raise AbortException()


class BaseCommand(object):
  def __init__(self, shell, args=None, cmd_prefix=None, timeout=60, env=None,
               verbose=False, test_case=None, handle_sigterm=False,
               log_process_stats=False):
    """Initialize the command.

    Args:
      shell: The name of the executable (e.g. d8).
      args: List of args to pass to the executable.
      cmd_prefix: Prefix of command (e.g. a wrapper script).
      timeout: Timeout in seconds.
      env: Environment dict for execution.
      verbose: Print additional output.
      test_case: Test case reference.
      handle_sigterm: Flag indicating if SIGTERM will be used to terminate the
          underlying process. Should not be used from the main thread, e.g. when
          using a command to list tests.
      log_process_stats: Indicate if we want to probe for process statistics like
          memory consumption.
    """
    assert(timeout > 0)

    self.shell = Path(shell)
    self.args = list(map(str, args or []))
    self.cmd_prefix = cmd_prefix or []
    self.timeout = timeout
    self.env = env or {}
    self.verbose = verbose
    self.handle_sigterm = handle_sigterm
    self.use_fork_server = bool(getattr(test_case, 'use_fork_server', False))

    if log_process_stats:
      self.process_logger = self.get_process_logger()
    else:
      self.process_logger = EMPTY_PROCESS_LOGGER

  def _result_overrides(self, returncode):
    pass

  @contextmanager
  def log_errors(self):
    try:
      yield
    except:
      logging.exception(f'Error executing: {self}\n')
      raise

  def get_process_logger(self):
    return EMPTY_PROCESS_LOGGER

  def execute(self):
    if self.verbose:
      print('# %s' % self)

    process = self._start_process()

    with handle_sigterm(process, self._abort, self.handle_sigterm):
      # Variable to communicate with the timer.
      timeout_occured = [False]
      timer = threading.Timer(
          self.timeout, self._abort, [process, timeout_occured])
      timer.start()

      start_time = time.time()
      with self.log_errors():
        with self.process_logger.log_stats(process) as stats:
          stdout, stderr = process.communicate()
      end_time = time.time()

      timer.cancel()

    self._result_overrides(process)
    return output.Output(
      process.returncode,
      timeout_occured[0],
      stdout.decode('utf-8', 'replace'),
      stderr.decode('utf-8', 'replace'),
      process.pid,
      start_time,
      end_time,
      stats=stats,
    )

  def _start_process(self):
    with self.log_errors():
      return subprocess.Popen(
        args=self._get_popen_args(),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=self._get_env(),
      )

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
    started_as = self.to_string()
    process_text = 'process %d started as:\n  %s\n' % (process.pid, started_as)
    try:
      logging.warning('Attempting to kill %s', process_text)
      self._kill_process(process)
    except OSError:
      logging.exception('Unruly %s', process_text)

  def __str__(self):
    return self.to_string()

  def to_string(self):
    def escape(part):
      # Escape spaces. We may need to escape more characters for this to work
      # properly.
      if ' ' in part:
        return '"%s"' % part
      return part

    parts = map(escape, self._to_args_list())
    return ' '.join(parts)

  def _to_args_list(self):
    return list(map(str, self.cmd_prefix + [self.shell])) + self.args


class DesktopCommand(BaseCommand):
  def get_process_logger(self):
    return PROCESS_LOGGER


# VariantProc cycles through all active variants per test, so keep capacity
# above the 8 supported unittests variants (standard + ADDITIONAL_VARIANTS in
# test/unittests/testcfg.py) to avoid LRU thrashing. Each cached server is an
# idle, initialized v8_unittests process, i.e. up to 16 per worker.
_FORK_SERVERS = {}
_MAX_FORK_SERVERS = 16
_FORK_SERVER_DISABLED = False


def close_fork_servers():
  for server in list(_FORK_SERVERS.values()):
    server.close()
  _FORK_SERVERS.clear()


# Used by SingleThreadedExecutionPool (in-process execution); multiprocessing
# workers exit via os._exit(0) and rely on stdin EOF for server shutdown.
atexit.register(close_fork_servers)


class _ForkServer(object):
  """Persistent fork-server process speaking the line-framed pipe protocol:
    Request (stdin):   "<gtest_filter>\\n"
    Response (stdout): "RES <exit_code> <stdout_len> <stderr_len>\\n"
                       followed by <stdout_len> stdout bytes and
                       <stderr_len> stderr bytes.
  """

  def __init__(self, shell, base_args, env):
    self.ok = False
    self.process = subprocess.Popen(
        args=[str(shell), '--fork-server'] + list(base_args),
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        # Inherit stderr so that server-side diagnostics (flag errors, CHECK
        # failures, sanitizer reports) remain visible.
        stderr=None,
        env=env,
        start_new_session=True,
    )

  def read_exact(self, n):
    chunks = []
    remaining = n
    while remaining > 0:
      chunk = self.process.stdout.read(remaining)
      if not chunk:
        raise EOFError('Unexpected EOF from fork server')
      chunks.append(chunk)
      remaining -= len(chunk)
    return b''.join(chunks)

  def close(self):
    if not self.process:
      return
    proc = self.process
    self.process = None
    try:
      if proc.stdin:
        proc.stdin.close()
    except OSError:
      pass
    try:
      proc.wait(timeout=1.0)
    except subprocess.TimeoutExpired:
      terminate_process_group_posix(proc)
      try:
        proc.wait(timeout=1.0)
      except subprocess.TimeoutExpired:
        pass
    try:
      if proc.stdout:
        proc.stdout.close()
    except OSError:
      pass


class PosixCommand(DesktopCommand):

  def execute(self):
    if (self.use_fork_server and not self.cmd_prefix and
        not _FORK_SERVER_DISABLED):
      # Only a single --gtest_filter is supported. With several (e.g. via
      # --extra-flags), gtest's last-one-wins semantics would be hard to
      # replicate, so fall back to direct execution.
      filter_indices = [
          i for i, arg in enumerate(self.args)
          if arg.startswith('--gtest_filter=')
      ]
      if len(filter_indices) == 1:
        i = filter_indices[0]
        test_filter = self.args[i][len('--gtest_filter='):]
        if test_filter:
          base_args = self.args[:i] + self.args[i + 1:]
          return self._execute_with_fork_server(test_filter, base_args)
    return super().execute()

  def _execute_with_fork_server(self, test_filter, base_args):
    if self.verbose:
      print('# %s (fork-server)' % self)

    env = self._get_env()
    key = (str(self.shell), tuple(base_args), tuple(sorted(env.items())))
    server = _FORK_SERVERS.pop(key, None)
    if (server is None or server.process is None or
        server.process.poll() is not None):
      if server is not None:
        server.close()
      while len(_FORK_SERVERS) >= _MAX_FORK_SERVERS:
        oldest_key = next(iter(_FORK_SERVERS))
        _FORK_SERVERS.pop(oldest_key).close()
      try:
        server = _ForkServer(self.shell, base_args, env)
      except OSError as e:
        self._disable_fork_server(e)
        return super().execute()
    _FORK_SERVERS[key] = server

    proc = server.process
    pid = proc.pid
    timeout_occured = [False]
    completed = False
    error = None
    with handle_sigterm(proc, self._abort, self.handle_sigterm):
      timer = threading.Timer(self.timeout, self._abort,
                              [proc, timeout_occured])
      timer.start()
      start_time = time.time()
      try:
        proc.stdin.write((test_filter + '\n').encode('utf-8'))
        proc.stdin.flush()
        res_line = proc.stdout.readline()
        if not res_line or not res_line.startswith(b'RES '):
          raise EOFError(f'Expected RES from fork server, got {res_line!r}')
        parts = res_line.split()
        if len(parts) != 4:
          raise ValueError(f'Malformed RES header: {res_line!r}')
        returncode = int(parts[1])
        stdout = server.read_exact(int(parts[2]))
        stderr = server.read_exact(int(parts[3]))
        server.ok = True
        completed = True
      except Exception as e:
        error = e
      finally:
        end_time = time.time()
        timer.cancel()
        if not completed:
          # Also covers BaseException (e.g. KeyboardInterrupt): never leave a
          # server with a half-read response in the cache.
          _FORK_SERVERS.pop(key, None)
          server.close()

    if not completed:
      if timeout_occured[0]:
        return output.Output(
            proc.returncode or -signal.SIGKILL,
            True,
            '',
            '',
            pid,
            start_time,
            end_time,
            stats=None,
        )
      if not server.ok:
        self._disable_fork_server(error)
      else:
        logging.warning(
            'Fork server failed for %s (%r); falling back to direct execution',
            self, error)
      return super().execute()

    return output.Output(
        returncode,
        timeout_occured[0],
        stdout.decode('utf-8', 'replace'),
        stderr.decode('utf-8', 'replace'),
        pid,
        start_time,
        end_time,
        stats=None,
    )

  def _disable_fork_server(self, error):
    global _FORK_SERVER_DISABLED
    _FORK_SERVER_DISABLED = True
    close_fork_servers()
    logging.warning(
        'Fork server failed on first request for %s (%r); disabling fork '
        'server in this worker and falling back to direct execution', self,
        error)

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
          # Make the new shell create its own process group. This allows to kill
          # all spawned processes reliably (https://crbug.com/v8/8292).
          start_new_session=True,
      )
    except Exception as e:
      sys.stderr.write('Error executing: %s\n' % self)
      raise e

  def _kill_process(self, process):
    # Kill the whole process group (PID == GPID after setsid).
    terminate_process_group_posix(process)


class IOSCommand(BaseCommand):

  def __init__(self,
               shell,
               args=None,
               cmd_prefix=None,
               timeout=120,
               env=None,
               verbose=False,
               test_case=None,
               handle_sigterm=False,
               log_process_stats=False):
    """Initialize the command and set a large enough timeout required for runs
    through the iOS Simulator.
    """
    super(IOSCommand, self).__init__(
        shell,
        args=args,
        cmd_prefix=cmd_prefix,
        timeout=timeout,
        env=env,
        verbose=verbose,
        handle_sigterm=handle_sigterm,
        log_process_stats=log_process_stats)

  def _result_overrides(self, process):
    # TODO(crbug.com/1445694): if iossim returns with code 65, force a
    # successful exit instead.
    if (process.returncode == 65):
      process.returncode = 0

  def _start_process(self):
    try:
      return subprocess.Popen(
          args=self._get_popen_args(),
          stdout=subprocess.PIPE,
          stderr=subprocess.PIPE,
          env=self._get_env(),
          shell=True,
          # Make the new shell create its own process group. This allows to kill
          # all spawned processes reliably (https://crbug.com/v8/8292).
          start_new_session=True,
      )
    except Exception as e:
      sys.stderr.write('Error executing: %s\n' % self)
      raise e

  def _kill_process(self, process):
    # Kill the whole process group (PID == GPID after setsid).
    terminate_process_group_posix(process)

  def _to_args_list(self):
    return list(map(str, self.cmd_prefix + [self.shell]))


def terminate_process_group_posix(process):
  try:
    # First try a soft term to allow some feedback
    os.killpg(process.pid, signal.SIGTERM)
    # Give the process some time to cleanly terminate.
    time.sleep(0.1)
    # Forcefully kill processes.
    os.killpg(process.pid, signal.SIGKILL)
  except ProcessLookupError:
    # The process terminated in the middle.
    pass

def terminate_process_windows(process):
  try:
    import _winapi
    handle = _winapi.OpenProcess(
        _winapi.PROCESS_ALL_ACCESS, False, process.pid)
    _winapi.TerminateProcess(handle, 1)
    _winapi.CloseHandle(handle)
  except Exception:
    logging.exception('Problem terminating process %s', process.pid)


class WindowsCommand(DesktopCommand):
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
    terminate_process_windows(process)


class AndroidCommand(BaseCommand):
  # This must be initialized before creating any instances of this class.
  driver = None

  def __init__(self, shell, args=None, cmd_prefix=None, timeout=60, env=None,
               verbose=False, test_case=None, handle_sigterm=False,
               log_process_stats=False):
    """Initialize the command and all files that need to be pushed to the
    Android device.
    """
    super(AndroidCommand, self).__init__(
        shell, args=args, cmd_prefix=cmd_prefix, timeout=timeout, env=env,
        verbose=verbose, handle_sigterm=handle_sigterm,
        log_process_stats=log_process_stats)

    self.args = [str(arg) for arg in args]

    test_case_resources = test_case.get_android_resources() if test_case else []
    files_from_args = files_from_relative_args(args)
    self.files_to_push = test_case_resources + files_from_args

  def execute(self, **additional_popen_kwargs):
    """Execute the command on the device.

    This pushes all required files to the device and then runs the command.
    """
    if self.verbose:
      print('# %s' % self)

    shell_name = self.shell.name
    shell_dir = self.shell.parent

    self.driver.push_executable(shell_dir, 'bin', shell_name)
    self.push_test_resources()

    start_time = time.time()
    return_code = 0
    timed_out = False
    try:
      stdout = self.driver.run(
          'bin', shell_name, self.args, '.', self.timeout, self.env)
    except CommandFailedException as e:
      return_code = e.status
      stdout = e.output
    except TimeoutException as e:
      return_code = 1
      timed_out = True
      # Sadly the Android driver doesn't provide output on timeout.
      stdout = ''

    end_time = time.time()
    return output.Output(
        return_code,
        timed_out,
        stdout,
        '',  # No stderr available.
        -1,  # No pid available.
        start_time,
        end_time,
    )

  def push_test_resources(self):
    for abs_file in self.files_to_push:
      abs_dir = abs_file.parent
      file_name = abs_file.name
      rel_dir = abs_dir.relative_to(BASE_DIR)
      self.driver.push_file(abs_dir, file_name, rel_dir)


def files_from_relative_args(args):
  files_to_push = []
  for f in ([Path(arg) for arg in args] or []):
    if f.exists():
      files_to_push.append(f.absolute())
  return files_to_push


Command = None


# Deprecated : use context.os_context
def setup(target_os, device):
  """Set the Command class to the OS-specific version."""
  global Command
  if target_os == 'android':
    AndroidCommand.driver = Driver.instance(device)
    Command = AndroidCommand
  elif target_os == 'ios':
    Command = IOSCommand
  elif target_os == 'windows':
    Command = WindowsCommand
  else:
    Command = PosixCommand


# Deprecated : use context.os_context
def tear_down():
  """Clean up after using commands."""
  if Command == AndroidCommand:
    AndroidCommand.driver.tear_down()

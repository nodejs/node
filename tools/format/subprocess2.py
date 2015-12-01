# coding=utf8
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Collection of subprocess wrapper functions.

In theory you shouldn't need anything else in subprocess, or this module failed.
"""

import cStringIO
import errno
import logging
import os
import Queue
import subprocess
import sys
import time
import threading


# Constants forwarded from subprocess.
PIPE = subprocess.PIPE
STDOUT = subprocess.STDOUT
# Sends stdout or stderr to os.devnull.
VOID = object()
# Error code when a process was killed because it timed out.
TIMED_OUT = -2001

# Globals.
# Set to True if you somehow need to disable this hack.
SUBPROCESS_CLEANUP_HACKED = False


class CalledProcessError(subprocess.CalledProcessError):
  """Augment the standard exception with more data."""
  def __init__(self, returncode, cmd, cwd, stdout, stderr):
    super(CalledProcessError, self).__init__(returncode, cmd, output=stdout)
    self.stdout = self.output  # for backward compatibility.
    self.stderr = stderr
    self.cwd = cwd

  def __str__(self):
    out = 'Command %r returned non-zero exit status %s' % (
        ' '.join(self.cmd), self.returncode)
    if self.cwd:
      out += ' in ' + self.cwd
    return '\n'.join(filter(None, (out, self.stdout, self.stderr)))


class CygwinRebaseError(CalledProcessError):
  """Occurs when cygwin's fork() emulation fails due to rebased dll."""


## Utility functions


def kill_pid(pid):
  """Kills a process by its process id."""
  try:
    # Unable to import 'module'
    # pylint: disable=E1101,F0401
    import signal
    return os.kill(pid, signal.SIGKILL)
  except ImportError:
    pass


def kill_win(process):
  """Kills a process with its windows handle.

  Has no effect on other platforms.
  """
  try:
    # Unable to import 'module'
    # pylint: disable=F0401
    import win32process
    # Access to a protected member _handle of a client class
    # pylint: disable=W0212
    return win32process.TerminateProcess(process._handle, -1)
  except ImportError:
    pass


def add_kill():
  """Adds kill() method to subprocess.Popen for python <2.6"""
  if hasattr(subprocess.Popen, 'kill'):
    return

  if sys.platform == 'win32':
    subprocess.Popen.kill = kill_win
  else:
    subprocess.Popen.kill = lambda process: kill_pid(process.pid)


def hack_subprocess():
  """subprocess functions may throw exceptions when used in multiple threads.

  See http://bugs.python.org/issue1731717 for more information.
  """
  global SUBPROCESS_CLEANUP_HACKED
  if not SUBPROCESS_CLEANUP_HACKED and threading.activeCount() != 1:
    # Only hack if there is ever multiple threads.
    # There is no point to leak with only one thread.
    subprocess._cleanup = lambda: None
    SUBPROCESS_CLEANUP_HACKED = True


def get_english_env(env):
  """Forces LANG and/or LANGUAGE to be English.

  Forces encoding to utf-8 for subprocesses.

  Returns None if it is unnecessary.
  """
  if sys.platform == 'win32':
    return None
  env = env or os.environ

  # Test if it is necessary at all.
  is_english = lambda name: env.get(name, 'en').startswith('en')

  if is_english('LANG') and is_english('LANGUAGE'):
    return None

  # Requires modifications.
  env = env.copy()
  def fix_lang(name):
    if not is_english(name):
      env[name] = 'en_US.UTF-8'
  fix_lang('LANG')
  fix_lang('LANGUAGE')
  return env


class NagTimer(object):
  """
  Triggers a callback when a time interval passes without an event being fired.

  For example, the event could be receiving terminal output from a subprocess;
  and the callback could print a warning to stderr that the subprocess appeared
  to be hung.
  """
  def __init__(self, interval, cb):
    self.interval = interval
    self.cb = cb
    self.timer = threading.Timer(self.interval, self.fn)
    self.last_output = self.previous_last_output = 0

  def start(self):
    self.last_output = self.previous_last_output = time.time()
    self.timer.start()

  def event(self):
    self.last_output = time.time()

  def fn(self):
    now = time.time()
    if self.last_output == self.previous_last_output:
      self.cb(now - self.previous_last_output)
    # Use 0.1 fudge factor, just in case
    #   (self.last_output - now) is very close to zero.
    sleep_time = (self.last_output - now - 0.1) % self.interval
    self.previous_last_output = self.last_output
    self.timer = threading.Timer(sleep_time + 0.1, self.fn)
    self.timer.start()

  def cancel(self):
    self.timer.cancel()


class Popen(subprocess.Popen):
  """Wraps subprocess.Popen() with various workarounds.

  - Forces English output since it's easier to parse the stdout if it is always
    in English.
  - Sets shell=True on windows by default. You can override this by forcing
    shell parameter to a value.
  - Adds support for VOID to not buffer when not needed.
  - Adds self.start property.

  Note: Popen() can throw OSError when cwd or args[0] doesn't exist. Translate
  exceptions generated by cygwin when it fails trying to emulate fork().
  """
  # subprocess.Popen.__init__() is not threadsafe; there is a race between
  # creating the exec-error pipe for the child and setting it to CLOEXEC during
  # which another thread can fork and cause the pipe to be inherited by its
  # descendents, which will cause the current Popen to hang until all those
  # descendents exit. Protect this with a lock so that only one fork/exec can
  # happen at a time.
  popen_lock = threading.Lock()

  def __init__(self, args, **kwargs):
    # Make sure we hack subprocess if necessary.
    hack_subprocess()
    add_kill()

    env = get_english_env(kwargs.get('env'))
    if env:
      kwargs['env'] = env
    if kwargs.get('shell') is None:
      # *Sigh*:  Windows needs shell=True, or else it won't search %PATH% for
      # the executable, but shell=True makes subprocess on Linux fail when it's
      # called with a list because it only tries to execute the first item in
      # the list.
      kwargs['shell'] = bool(sys.platform=='win32')

    if isinstance(args, basestring):
      tmp_str = args
    elif isinstance(args, (list, tuple)):
      tmp_str = ' '.join(args)
    else:
      raise CalledProcessError(None, args, kwargs.get('cwd'), None, None)
    if kwargs.get('cwd', None):
      tmp_str += ';  cwd=%s' % kwargs['cwd']
    logging.debug(tmp_str)

    self.stdout_cb = None
    self.stderr_cb = None
    self.stdin_is_void = False
    self.stdout_is_void = False
    self.stderr_is_void = False
    self.cmd_str = tmp_str

    if kwargs.get('stdin') is VOID:
      kwargs['stdin'] = open(os.devnull, 'r')
      self.stdin_is_void = True

    for stream in ('stdout', 'stderr'):
      if kwargs.get(stream) in (VOID, os.devnull):
        kwargs[stream] = open(os.devnull, 'w')
        setattr(self, stream + '_is_void', True)
      if callable(kwargs.get(stream)):
        setattr(self, stream + '_cb', kwargs[stream])
        kwargs[stream] = PIPE

    self.start = time.time()
    self.timeout = None
    self.nag_timer = None
    self.nag_max = None
    self.shell = kwargs.get('shell', None)
    # Silence pylint on MacOSX
    self.returncode = None

    try:
      with self.popen_lock:
        super(Popen, self).__init__(args, **kwargs)
    except OSError, e:
      if e.errno == errno.EAGAIN and sys.platform == 'cygwin':
        # Convert fork() emulation failure into a CygwinRebaseError().
        raise CygwinRebaseError(
            e.errno,
            args,
            kwargs.get('cwd'),
            None,
            'Visit '
            'http://code.google.com/p/chromium/wiki/CygwinDllRemappingFailure '
            'to learn how to fix this error; you need to rebase your cygwin '
            'dlls')
      # Popen() can throw OSError when cwd or args[0] doesn't exist.
      raise OSError('Execution failed with error: %s.\n'
                    'Check that %s or %s exist and have execution permission.'
                    % (str(e), kwargs.get('cwd'), args[0]))

  def _tee_threads(self, input):  # pylint: disable=W0622
    """Does I/O for a process's pipes using threads.

    It's the simplest and slowest implementation. Expect very slow behavior.

    If there is a callback and it doesn't keep up with the calls, the timeout
    effectiveness will be delayed accordingly.
    """
    # Queue of either of <threadname> when done or (<threadname>, data).  In
    # theory we would like to limit to ~64kb items to not cause large memory
    # usage when the callback blocks. It is not done because it slows down
    # processing on OSX10.6 by a factor of 2x, making it even slower than
    # Windows!  Revisit this decision if it becomes a problem, e.g. crash
    # because of memory exhaustion.
    queue = Queue.Queue()
    done = threading.Event()
    nag = None

    def write_stdin():
      try:
        stdin_io = cStringIO.StringIO(input)
        while True:
          data = stdin_io.read(1024)
          if data:
            self.stdin.write(data)
          else:
            self.stdin.close()
            break
      finally:
        queue.put('stdin')

    def _queue_pipe_read(pipe, name):
      """Queues characters read from a pipe into a queue."""
      try:
        while True:
          data = pipe.read(1)
          if not data:
            break
          if nag:
            nag.event()
          queue.put((name, data))
      finally:
        queue.put(name)

    def timeout_fn():
      try:
        done.wait(self.timeout)
      finally:
        queue.put('timeout')

    def wait_fn():
      try:
        self.wait()
      finally:
        queue.put('wait')

    # Starts up to 5 threads:
    # Wait for the process to quit
    # Read stdout
    # Read stderr
    # Write stdin
    # Timeout
    threads = {
        'wait': threading.Thread(target=wait_fn),
    }
    if self.timeout is not None:
      threads['timeout'] = threading.Thread(target=timeout_fn)
    if self.stdout_cb:
      threads['stdout'] = threading.Thread(
          target=_queue_pipe_read, args=(self.stdout, 'stdout'))
    if self.stderr_cb:
      threads['stderr'] = threading.Thread(
        target=_queue_pipe_read, args=(self.stderr, 'stderr'))
    if input:
      threads['stdin'] = threading.Thread(target=write_stdin)
    elif self.stdin:
      # Pipe but no input, make sure it's closed.
      self.stdin.close()
    for t in threads.itervalues():
      t.start()

    if self.nag_timer:
      def _nag_cb(elapsed):
        logging.warn('  No output for %.0f seconds from command:' % elapsed)
        logging.warn('    %s' % self.cmd_str)
        if (self.nag_max and
            int('%.0f' % (elapsed / self.nag_timer)) >= self.nag_max):
          queue.put('timeout')
          done.set()  # Must do this so that timeout thread stops waiting.
      nag = NagTimer(self.nag_timer, _nag_cb)
      nag.start()

    timed_out = False
    try:
      # This thread needs to be optimized for speed.
      while threads:
        item = queue.get()
        if item[0] == 'stdout':
          self.stdout_cb(item[1])
        elif item[0] == 'stderr':
          self.stderr_cb(item[1])
        else:
          # A thread terminated.
          if item in threads:
            threads[item].join()
            del threads[item]
          if item == 'wait':
            # Terminate the timeout thread if necessary.
            done.set()
          elif item == 'timeout' and not timed_out and self.poll() is None:
            logging.debug('Timed out after %.0fs: killing' % (
                time.time() - self.start))
            self.kill()
            timed_out = True
    finally:
      # Stop the threads.
      done.set()
      if nag:
        nag.cancel()
      if 'wait' in threads:
        # Accelerate things, otherwise it would hang until the child process is
        # done.
        logging.debug('Killing child because of an exception')
        self.kill()
      # Join threads.
      for thread in threads.itervalues():
        thread.join()
      if timed_out:
        self.returncode = TIMED_OUT

  # pylint: disable=W0221,W0622
  def communicate(self, input=None, timeout=None, nag_timer=None,
                  nag_max=None):
    """Adds timeout and callbacks support.

    Returns (stdout, stderr) like subprocess.Popen().communicate().

    - The process will be killed after |timeout| seconds and returncode set to
      TIMED_OUT.
    - If the subprocess runs for |nag_timer| seconds without producing terminal
      output, print a warning to stderr.
    """
    self.timeout = timeout
    self.nag_timer = nag_timer
    self.nag_max = nag_max
    if (not self.timeout and not self.nag_timer and
        not self.stdout_cb and not self.stderr_cb):
      return super(Popen, self).communicate(input)

    if self.timeout and self.shell:
      raise TypeError(
          'Using timeout and shell simultaneously will cause a process leak '
          'since the shell will be killed instead of the child process.')

    stdout = None
    stderr = None
    # Convert to a lambda to workaround python's deadlock.
    # http://docs.python.org/library/subprocess.html#subprocess.Popen.wait
    # When the pipe fills up, it would deadlock this process.
    if self.stdout and not self.stdout_cb and not self.stdout_is_void:
      stdout = []
      self.stdout_cb = stdout.append
    if self.stderr and not self.stderr_cb and not self.stderr_is_void:
      stderr = []
      self.stderr_cb = stderr.append
    self._tee_threads(input)
    if stdout is not None:
      stdout = ''.join(stdout)
    if stderr is not None:
      stderr = ''.join(stderr)
    return (stdout, stderr)


def communicate(args, timeout=None, nag_timer=None, nag_max=None, **kwargs):
  """Wraps subprocess.Popen().communicate() and add timeout support.

  Returns ((stdout, stderr), returncode).

  - The process will be killed after |timeout| seconds and returncode set to
    TIMED_OUT.
  - If the subprocess runs for |nag_timer| seconds without producing terminal
    output, print a warning to stderr.
  - Automatically passes stdin content as input so do not specify stdin=PIPE.
  """
  stdin = kwargs.pop('stdin', None)
  if stdin is not None:
    if isinstance(stdin, basestring):
      # When stdin is passed as an argument, use it as the actual input data and
      # set the Popen() parameter accordingly.
      kwargs['stdin'] = PIPE
    else:
      kwargs['stdin'] = stdin
      stdin = None

  proc = Popen(args, **kwargs)
  if stdin:
    return proc.communicate(stdin, timeout, nag_timer), proc.returncode
  else:
    return proc.communicate(None, timeout, nag_timer), proc.returncode


def call(args, **kwargs):
  """Emulates subprocess.call().

  Automatically convert stdout=PIPE or stderr=PIPE to VOID.
  In no case they can be returned since no code path raises
  subprocess2.CalledProcessError.
  """
  if kwargs.get('stdout') == PIPE:
    kwargs['stdout'] = VOID
  if kwargs.get('stderr') == PIPE:
    kwargs['stderr'] = VOID
  return communicate(args, **kwargs)[1]


def check_call_out(args, **kwargs):
  """Improved version of subprocess.check_call().

  Returns (stdout, stderr), unlike subprocess.check_call().
  """
  out, returncode = communicate(args, **kwargs)
  if returncode:
    raise CalledProcessError(
        returncode, args, kwargs.get('cwd'), out[0], out[1])
  return out


def check_call(args, **kwargs):
  """Emulate subprocess.check_call()."""
  check_call_out(args, **kwargs)
  return 0


def capture(args, **kwargs):
  """Captures stdout of a process call and returns it.

  Returns stdout.

  - Discards returncode.
  - Blocks stdin by default if not specified since no output will be visible.
  """
  kwargs.setdefault('stdin', VOID)

  # Like check_output, deny the caller from using stdout arg.
  return communicate(args, stdout=PIPE, **kwargs)[0][0]


def check_output(args, **kwargs):
  """Emulates subprocess.check_output().

  Captures stdout of a process call and returns stdout only.

  - Throws if return code is not 0.
  - Works even prior to python 2.7.
  - Blocks stdin by default if not specified since no output will be visible.
  - As per doc, "The stdout argument is not allowed as it is used internally."
  """
  kwargs.setdefault('stdin', VOID)
  if 'stdout' in kwargs:
    raise ValueError('stdout argument not allowed, it would be overridden.')
  return check_call_out(args, stdout=PIPE, **kwargs)[0]

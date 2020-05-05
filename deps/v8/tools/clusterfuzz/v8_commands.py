# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Fork from commands.py and output.py in v8 test driver.

import os
import signal
import subprocess
import sys
from threading import Event, Timer

import v8_fuzz_config

PYTHON3 = sys.version_info >= (3, 0)

# List of default flags passed to each d8 run.
DEFAULT_FLAGS = [
  '--correctness-fuzzer-suppressions',
  '--expose-gc',
  '--allow-natives-for-differential-fuzzing',
  '--invoke-weak-callbacks',
  '--omit-quit',
  '--es-staging',
  '--wasm-staging',
  '--no-wasm-async-compilation',
  '--suppress-asm-messages',
]

BASE_PATH = os.path.dirname(os.path.abspath(__file__))

# List of files passed to each d8 run before the testcase.
DEFAULT_MOCK = os.path.join(BASE_PATH, 'v8_mock.js')

# Suppressions on JavaScript level for known issues.
JS_SUPPRESSIONS = os.path.join(BASE_PATH, 'v8_suppressions.js')

# Config-specific mock files.
ARCH_MOCKS = os.path.join(BASE_PATH, 'v8_mock_archs.js')
WEBASSEMBLY_MOCKS = os.path.join(BASE_PATH, 'v8_mock_webassembly.js')

# Timeout in seconds for one d8 run.
TIMEOUT = 3


def _startup_files(options):
  """Default files and optional config-specific mock files."""
  files = [DEFAULT_MOCK]
  if not options.skip_suppressions:
    files.append(JS_SUPPRESSIONS)
  if options.first.arch != options.second.arch:
    files.append(ARCH_MOCKS)
  # Mock out WebAssembly when comparing with jitless mode.
  if '--jitless' in options.first.flags + options.second.flags:
    files.append(WEBASSEMBLY_MOCKS)
  return files


class Command(object):
  """Represents a configuration for running V8 multiple times with certain
  flags and files.
  """
  def __init__(self, options, label, executable, config_flags):
    self.label = label
    self.executable = executable
    self.config_flags = config_flags
    self.common_flags =  DEFAULT_FLAGS[:]
    self.common_flags.extend(['--random-seed', str(options.random_seed)])

    self.files = _startup_files(options)

  def run(self, testcase, verbose=False):
    """Run the executable with a specific testcase."""
    args = [self.executable] + self.flags + self.files + [testcase]
    if verbose:
      print('# Command line for %s comparison:' % self.label)
      print(' '.join(args))
    if self.executable.endswith('.py'):
      # Wrap with python in tests.
      args = [sys.executable] + args
    return Execute(
        args,
        cwd=os.path.dirname(os.path.abspath(testcase)),
        timeout=TIMEOUT,
    )

  @property
  def flags(self):
    return self.common_flags + self.config_flags


class Output(object):
  def __init__(self, exit_code, timed_out, stdout, pid):
    self.exit_code = exit_code
    self.timed_out = timed_out
    self.stdout = stdout
    self.pid = pid

  def HasCrashed(self):
    # Timed out tests will have exit_code -signal.SIGTERM.
    if self.timed_out:
      return False
    return (self.exit_code < 0 and
            self.exit_code != -signal.SIGABRT)

  def HasTimedOut(self):
    return self.timed_out


def Execute(args, cwd, timeout=None):
  popen_args = [c for c in args if c != ""]
  kwargs = {}
  if PYTHON3:
    kwargs['encoding'] = 'utf-8'
  try:
    process = subprocess.Popen(
      args=popen_args,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      cwd=cwd,
      **kwargs
    )
  except Exception as e:
    sys.stderr.write("Error executing: %s\n" % popen_args)
    raise e

  timeout_event = Event()

  def kill_process():
    timeout_event.set()
    try:
      process.kill()
    except OSError:
      sys.stderr.write('Error: Process %s already ended.\n' % process.pid)

  timer = Timer(timeout, kill_process)
  timer.start()
  stdout, _ = process.communicate()
  timer.cancel()

  return Output(
      process.returncode,
      timeout_event.is_set(),
      stdout,
      process.pid,
  )

# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Fork from commands.py and output.py in v8 test driver.

import signal
import subprocess
import sys
from threading import Event, Timer


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
  try:
    process = subprocess.Popen(
      args=popen_args,
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,
      cwd=cwd
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
      stdout.decode('utf-8', 'replace').encode('utf-8'),
      process.pid,
  )

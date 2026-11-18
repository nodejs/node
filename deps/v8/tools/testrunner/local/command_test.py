#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import signal
import stat
import sys
import tempfile
import threading
import time
import unittest

# Needed because the test runner contains relative imports.
TOOLS_PATH = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(TOOLS_PATH)

from testrunner.local import command
from testrunner.local.pool import AbortException

FAKE_SERVER_SCRIPT = """#!{python}
import sys
import time

if len(sys.argv) < 2 or sys.argv[1] != '--fork-server':
  print('DIRECT:' + ' '.join(sys.argv[1:]), end='')
  sys.stderr.write('DIRECT_ERR')
  sys.exit(0)

if '--fail-startup' in sys.argv:
  sys.exit(1)

for line in sys.stdin:
  filt = line.strip()
  if filt == 'hang':
    time.sleep(30)
  elif filt == 'short_res':
    sys.stdout.buffer.write(b'RES 0\\n')
    sys.stdout.buffer.flush()
  elif filt == 'garbage':
    sys.stdout.buffer.write(b'NOT_A_RES\\n')
    sys.stdout.buffer.flush()
  elif filt == 'segv':
    sys.stdout.buffer.write(b'RES -11 0 4\\nSEGV')
    sys.stdout.buffer.flush()
  elif filt == 'abrt':
    sys.stdout.buffer.write(b'RES -6 0 4\\nABRT')
    sys.stdout.buffer.flush()
  else:
    out = ('OUT:' + filt).encode('utf-8')
    err = ('ERR:' + filt).encode('utf-8')
    header = ('RES 0 %d %d\\n' % (len(out), len(err))).encode('utf-8')
    sys.stdout.buffer.write(header + out + err)
    sys.stdout.buffer.flush()
"""


class _DummyTestCase(object):

  def __init__(self, use_fork_server=True):
    self.use_fork_server = use_fork_server


@unittest.skipIf(sys.platform == 'win32', 'POSIX fork server only')
class ForkServerCommandTest(unittest.TestCase):

  def setUp(self):
    command.close_fork_servers()
    command._FORK_SERVER_DISABLED = False
    self._tmp = tempfile.NamedTemporaryFile(
        mode='w', suffix='.py', delete=False)
    self._tmp.write(FAKE_SERVER_SCRIPT.format(python=sys.executable))
    self._tmp.close()
    os.chmod(self._tmp.name, stat.S_IRWXU)
    self.shell = self._tmp.name

  def tearDown(self):
    command.close_fork_servers()
    command._FORK_SERVER_DISABLED = False
    if os.path.exists(self._tmp.name):
      os.unlink(self._tmp.name)

  def _make_cmd(self,
                test_filter='Suite.Test',
                extra_args=None,
                timeout=5,
                use_fork_server=True,
                cmd_prefix=None,
                handle_sigterm=False):
    args = list(extra_args or []) + [f'--gtest_filter={test_filter}']
    return command.PosixCommand(
        shell=self.shell,
        args=args,
        cmd_prefix=cmd_prefix,
        timeout=timeout,
        test_case=_DummyTestCase(use_fork_server=use_fork_server),
        handle_sigterm=handle_sigterm,
    )

  def testSuccessAndReuse(self):
    out1 = self._make_cmd('Suite.One').execute()
    self.assertEqual(0, out1.exit_code)
    self.assertFalse(out1.timed_out)
    self.assertEqual('OUT:Suite.One', out1.stdout)
    self.assertEqual('ERR:Suite.One', out1.stderr)
    self.assertEqual(1, len(command._FORK_SERVERS))

    out2 = self._make_cmd('Suite.Two').execute()
    self.assertEqual(0, out2.exit_code)
    self.assertEqual('OUT:Suite.Two', out2.stdout)
    self.assertEqual('ERR:Suite.Two', out2.stderr)
    self.assertEqual(out1.pid, out2.pid)
    self.assertEqual(1, len(command._FORK_SERVERS))

  def testSignalExitCodes(self):
    segv = self._make_cmd('segv').execute()
    self.assertEqual(-11, segv.exit_code)
    self.assertTrue(segv.HasCrashed())

    abrt = self._make_cmd('abrt').execute()
    self.assertEqual(-6, abrt.exit_code)
    self.assertFalse(abrt.HasCrashed())

  def testMalformedHeaderFallsBackWithoutDisabling(self):
    # First request succeeds so server.ok becomes True.
    out1 = self._make_cmd('Suite.Ok').execute()
    self.assertEqual('OUT:Suite.Ok', out1.stdout)

    # Truncated RES header ("RES 0\n") triggers fallback to direct mode,
    # evicts the broken server, and leaves fork server enabled for next test.
    out2 = self._make_cmd('short_res').execute()
    self.assertEqual(0, out2.exit_code)
    self.assertIn('DIRECT:', out2.stdout)
    self.assertFalse(command._FORK_SERVER_DISABLED)
    self.assertEqual(0, len(command._FORK_SERVERS))

    out3 = self._make_cmd('Suite.Recovered').execute()
    self.assertEqual('OUT:Suite.Recovered', out3.stdout)
    self.assertNotEqual(out1.pid, out3.pid)

  def testNonResHeaderFallsBackWithoutDisabling(self):
    out1 = self._make_cmd('Suite.Ok').execute()
    self.assertEqual('OUT:Suite.Ok', out1.stdout)

    # A line that is not a RES header at all takes the same fallback path as
    # a truncated one: direct execution, eviction, fork server stays enabled.
    out2 = self._make_cmd('garbage').execute()
    self.assertEqual(0, out2.exit_code)
    self.assertIn('DIRECT:', out2.stdout)
    self.assertFalse(command._FORK_SERVER_DISABLED)
    self.assertEqual(0, len(command._FORK_SERVERS))

  def testStartupFailureStickyDisable(self):
    out1 = self._make_cmd('Suite.One', extra_args=['--fail-startup']).execute()
    self.assertIn('DIRECT:', out1.stdout)
    self.assertTrue(command._FORK_SERVER_DISABLED)
    self.assertEqual(0, len(command._FORK_SERVERS))

    # Subsequent commands bypass fork server immediately.
    out2 = self._make_cmd('Suite.Two').execute()
    self.assertIn('DIRECT:', out2.stdout)

  def testTimeoutEvictsAndRespawns(self):
    out1 = self._make_cmd('hang', timeout=0.2).execute()
    self.assertTrue(out1.timed_out)
    self.assertEqual(0, len(command._FORK_SERVERS))

    out2 = self._make_cmd('Suite.AfterTimeout').execute()
    self.assertFalse(out2.timed_out)
    self.assertEqual('OUT:Suite.AfterTimeout', out2.stdout)

  def testLruCacheEviction(self):
    procs = []
    for i in range(command._MAX_FORK_SERVERS + 2):
      out = self._make_cmd('Suite.Test', extra_args=[f'--flag={i}']).execute()
      self.assertEqual('OUT:Suite.Test', out.stdout)
      # Capture the Popen object now; _ForkServer.close() drops its reference.
      server = command._FORK_SERVERS[next(reversed(command._FORK_SERVERS))]
      self.assertEqual(out.pid, server.process.pid)
      procs.append(server.process)
      self.assertLessEqual(
          len(command._FORK_SERVERS), command._MAX_FORK_SERVERS)

    # First two servers should have been closed and reaped, the rest is alive.
    for proc in procs[:2]:
      self.assertIsNotNone(proc.returncode)
    for proc in procs[2:]:
      self.assertIsNone(proc.poll())

  def testDirectModeWhenDisabledOrPrefixed(self):
    out1 = self._make_cmd('Suite.One', use_fork_server=False).execute()
    self.assertIn('DIRECT:', out1.stdout)
    self.assertEqual(0, len(command._FORK_SERVERS))

    out2 = self._make_cmd('Suite.Two', cmd_prefix=[sys.executable]).execute()
    self.assertIn('DIRECT:', out2.stdout)
    self.assertEqual(0, len(command._FORK_SERVERS))

  def testDirectModeWithMultipleFilters(self):
    # E.g. an additional --gtest_filter passed via --extra-flags. gtest's
    # last-one-wins semantics are left to direct execution.
    out = self._make_cmd(
        'Suite.Two', extra_args=['--gtest_filter=Suite.One']).execute()
    self.assertIn('DIRECT:', out.stdout)
    self.assertIn('--gtest_filter=Suite.One --gtest_filter=Suite.Two',
                  out.stdout)
    self.assertEqual(0, len(command._FORK_SERVERS))
    self.assertFalse(command._FORK_SERVER_DISABLED)

  def testSpawnErrorStickyDisable(self):
    # A non-existent shell makes Popen raise OSError when spawning the server.
    # This must not propagate but disable the fork server and fall back to
    # direct execution (which reports the shell's error as a regular result).
    self.shell = os.path.join(os.path.dirname(self.shell), 'does_not_exist')
    out1 = self._make_cmd('Suite.One').execute()
    self.assertNotEqual(0, out1.exit_code)
    self.assertTrue(command._FORK_SERVER_DISABLED)
    self.assertEqual(0, len(command._FORK_SERVERS))

    out2 = self._make_cmd('Suite.Two').execute()
    self.assertNotEqual(0, out2.exit_code)
    self.assertEqual(0, len(command._FORK_SERVERS))

  def testSigtermRaisesAbortException(self):
    # The real signal.signal lives in the signal (or _signal) module; a no-op
    # lambda installed by command.setup_testing() would leave SIGTERM at
    # SIG_DFL and kill the test process.
    if getattr(signal.signal, '__module__', None) not in ('signal', '_signal'):
      self.skipTest('signal.signal is monkey-patched (command.setup_testing)')

    # Safety net: should SIGTERM arrive before handle_sigterm installed its
    # handler, it must not hit SIG_DFL and kill the test process.
    previous = signal.signal(signal.SIGTERM, lambda *_: None)
    self.addCleanup(signal.signal, signal.SIGTERM, previous)

    cmd = self._make_cmd('hang', timeout=10, handle_sigterm=True)
    main_thread_id = threading.main_thread().ident

    def send_sigterm():
      # Wait until the server is registered, i.e. the request is in flight.
      while not command._FORK_SERVERS:
        time.sleep(0.01)
      time.sleep(0.05)
      # pthread_kill is unaffected by pool.setup_testing() patching os.kill.
      signal.pthread_kill(main_thread_id, signal.SIGTERM)

    t = threading.Thread(target=send_sigterm)
    t.start()
    try:
      with self.assertRaises(AbortException):
        cmd.execute()
    finally:
      t.join()
    self.assertEqual(0, len(command._FORK_SERVERS))


if __name__ == '__main__':
  unittest.main()

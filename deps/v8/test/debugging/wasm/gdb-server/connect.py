# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Flags: -expose-wasm --wasm-gdb-remote --wasm-pause-waiting-for-debugger test/debugging/wasm/gdb-server/test_files/test_basic.js

from ctypes import *
import os
import subprocess
import unittest
import sys

import gdb_rsp
import test_files.test_basic as test_basic

# These are set up by Main().
COMMAND = None

class Tests(unittest.TestCase):

  def test_disconnect(self):
    process = gdb_rsp.PopenDebugStub(COMMAND)
    try:
      # Connect.
      connection = gdb_rsp.GdbRspConnection()
      connection.Close()
      # Reconnect 3 times.
      for _ in range(3):
        connection = gdb_rsp.GdbRspConnection()
        connection.Close()
    finally:
      gdb_rsp.KillProcess(process)

  def test_kill(self):
    process = gdb_rsp.PopenDebugStub(COMMAND)
    try:
      connection = gdb_rsp.GdbRspConnection()
      # Request killing the target.
      reply = connection.RspRequest('k')
      self.assertEqual(reply, 'OK')
      signal = c_byte(process.wait()).value
      self.assertEqual(signal, gdb_rsp.RETURNCODE_KILL)
    finally:
      gdb_rsp.KillProcess(process)

  def test_detach(self):
    process = gdb_rsp.PopenDebugStub(COMMAND)
    try:
      connection = gdb_rsp.GdbRspConnection()
      # Request detaching from the target.
      # This resumes execution, so we get the normal exit() status.
      reply = connection.RspRequest('D')
      self.assertEqual(reply, 'OK')
    finally:
      gdb_rsp.KillProcess(process)


def Main():
  index = sys.argv.index('--')
  args = sys.argv[index + 1:]
  # The remaining arguments go to unittest.main().
  global COMMAND
  COMMAND = args
  unittest.main(argv=sys.argv[:index])

if __name__ == '__main__':
  Main()

# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Flags: -expose-wasm --wasm-gdb-remote --wasm-pause-waiting-for-debugger test/debugging/wasm/gdb-server/test_files/test_trap.js

import sys
import unittest

import gdb_rsp
import test_files.test_trap as test_trap

# These are set up by Main().
COMMAND = None

class Tests(unittest.TestCase):
  def test_trap(self):
    with gdb_rsp.LaunchDebugStub(COMMAND) as connection:
      module_load_addr = gdb_rsp.GetLoadedModuleAddress(connection)
      reply = connection.RspRequest('c')
      gdb_rsp.AssertReplySignal(reply, gdb_rsp.SIGSEGV)
      tid = gdb_rsp.ParseThreadStopReply(reply)['thread_id']
      self.assertEqual(tid, 1)
      regs = gdb_rsp.DecodeRegs(connection.RspRequest('g'))
      self.assertEqual(regs['pc'], module_load_addr + test_trap.TRAP_ADDRESS)


def Main():
  index = sys.argv.index('--')
  args = sys.argv[index + 1:]
  # The remaining arguments go to unittest.main().
  global COMMAND
  COMMAND = args
  unittest.main(argv=sys.argv[:index])

if __name__ == '__main__':
  Main()

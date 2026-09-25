# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Flags: -expose-wasm --wasm-gdb-remote --wasm-pause-waiting-for-debugger test/debugging/wasm/gdb-server/test_files/test_basic.js

import sys
import unittest

import gdb_rsp
import test_files.test_basic as test_basic

# These are set up by Main().
COMMAND = None

class Tests(unittest.TestCase):
  def test_single_step(self):
    with gdb_rsp.LaunchDebugStub(COMMAND) as connection:
      module_load_addr = gdb_rsp.GetLoadedModuleAddress(connection)
      bp_addr = module_load_addr + test_basic.BREAK_ADDRESS_0
      reply = connection.RspRequest('Z0,%x,1' % bp_addr)
      self.assertEqual("OK", reply)
      reply = connection.RspRequest('c')
      gdb_rsp.AssertReplySignal(reply, gdb_rsp.SIGTRAP)

      # We expect 's' to stop at the next instruction.
      reply = connection.RspRequest('s')
      gdb_rsp.AssertReplySignal(reply, gdb_rsp.SIGTRAP)
      tid = gdb_rsp.ParseThreadStopReply(reply)['thread_id']
      self.assertEqual(tid, 1)
      regs = gdb_rsp.DecodeRegs(connection.RspRequest('g'))
      self.assertEqual(regs['pc'], module_load_addr + test_basic.BREAK_ADDRESS_1)

      # Again.
      reply = connection.RspRequest('s')
      gdb_rsp.AssertReplySignal(reply, gdb_rsp.SIGTRAP)
      tid = gdb_rsp.ParseThreadStopReply(reply)['thread_id']
      self.assertEqual(tid, 1)
      regs = gdb_rsp.DecodeRegs(connection.RspRequest('g'))
      self.assertEqual(regs['pc'], module_load_addr + test_basic.BREAK_ADDRESS_2)

      # Check that we can continue after single-stepping.
      reply = connection.RspRequest('c')
      gdb_rsp.AssertReplySignal(reply, gdb_rsp.SIGTRAP)


def Main():
  index = sys.argv.index('--')
  args = sys.argv[index + 1:]
  # The remaining arguments go to unittest.main().
  global COMMAND
  COMMAND = args
  unittest.main(argv=sys.argv[:index])

if __name__ == '__main__':
  Main()

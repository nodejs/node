# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Flags: -expose-wasm --wasm-gdb-remote --wasm-pause-waiting-for-debugger test/debugging/wasm/gdb-server/test_files/test_basic.js

import sys
import unittest

import gdb_rsp
import test_files.test_basic as test_basic

class Tests(unittest.TestCase):
  def test_initial_breakpoint(self):
    # Testing that the debuggee suspends when the debugger attaches.
    with gdb_rsp.LaunchDebugStub(COMMAND) as connection:
      reply = connection.RspRequest('?')
      gdb_rsp.AssertReplySignal(reply, gdb_rsp.SIGTRAP)

  def test_setting_removing_breakpoint(self):
    with gdb_rsp.LaunchDebugStub(COMMAND) as connection:
      module_load_addr = gdb_rsp.GetLoadedModuleAddress(connection)
      func_addr = module_load_addr + test_basic.BREAK_ADDRESS_1

      # Set a breakpoint.
      reply = connection.RspRequest('Z0,%x,1' % func_addr)
      self.assertEqual(reply, 'OK')

      # When we run the program, we should hit the breakpoint.
      reply = connection.RspRequest('c')
      gdb_rsp.AssertReplySignal(reply, gdb_rsp.SIGTRAP)
      gdb_rsp.CheckInstructionPtr(connection, func_addr)

      # Check that we can remove the breakpoint.
      reply = connection.RspRequest('z0,%x,0' % func_addr)
      self.assertEqual(reply, 'OK')
      # Requesting removing a breakpoint on an address that does not
      # have one should return an error.
      reply = connection.RspRequest('z0,%x,0' % func_addr)
      self.assertEqual(reply, 'E03')

  def test_setting_breakpoint_on_invalid_address(self):
    with gdb_rsp.LaunchDebugStub(COMMAND) as connection:
      # Requesting a breakpoint on an invalid address should give an error.
      reply = connection.RspRequest('Z0,%x,1' % (1 << 32))
      self.assertEqual(reply, 'E03')


def Main():
  index = sys.argv.index('--')
  args = sys.argv[index + 1:]
  # The remaining arguments go to unittest.main().
  global COMMAND
  COMMAND = args
  unittest.main(argv=sys.argv[:index])

if __name__ == '__main__':
  Main()

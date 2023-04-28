# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Flags: -expose-wasm --wasm-gdb-remote --wasm-pause-waiting-for-debugger test/debugging/wasm/gdb-server/test_files/test_basic.js

import re
import struct
import sys
import unittest

import gdb_rsp
import test_files.test_basic as test_basic

# These are set up by Main().
COMMAND = None

class Tests(unittest.TestCase):

  def test_loaded_modules(self):
    with gdb_rsp.LaunchDebugStub(COMMAND) as connection:
      modules = gdb_rsp.GetLoadedModules(connection)
      connection.Close()
      assert(len(modules) > 0)

  def test_checking_thread_state(self):
    with gdb_rsp.LaunchDebugStub(COMMAND) as connection:
      # Query wasm thread id
      reply = connection.RspRequest('qfThreadInfo')
      match = re.match('m([0-9])$', reply)
      if match is None:
        raise AssertionError('Bad active thread list reply: %r' % reply)
      thread_id = int(match.group(1), 10)
      # There should not be other threads.
      reply = connection.RspRequest('qsThreadInfo')
      self.assertEqual("l", reply)
      # Test that valid thread should be alive.
      reply = connection.RspRequest('T%d' % (thread_id))
      self.assertEqual("OK", reply)
      # Test invalid thread id.
      reply = connection.RspRequest('T42')
      self.assertEqual("E02", reply)

  def test_wasm_local(self):
    with gdb_rsp.LaunchDebugStub(COMMAND) as connection:
      module_load_addr = gdb_rsp.GetLoadedModuleAddress(connection)
      breakpoint_addr = module_load_addr + test_basic.BREAK_ADDRESS_2

      reply = connection.RspRequest('Z0,%x,1' % breakpoint_addr)
      self.assertEqual("OK", reply)
      reply = connection.RspRequest('c')
      gdb_rsp.AssertReplySignal(reply, gdb_rsp.SIGTRAP)

      reply = connection.RspRequest('qWasmLocal:0;0')
      value = struct.unpack('I', gdb_rsp.DecodeHex(reply))[0]
      self.assertEqual(test_basic.ARG_0, value)

      reply = connection.RspRequest('qWasmLocal:0;1')
      value = struct.unpack('I', gdb_rsp.DecodeHex(reply))[0]
      self.assertEqual(test_basic.ARG_1, value)

      # invalid local
      reply = connection.RspRequest('qWasmLocal:0;9')
      self.assertEqual("E03", reply)

  def test_wasm_stack_value(self):
    with gdb_rsp.LaunchDebugStub(COMMAND) as connection:
      module_load_addr = gdb_rsp.GetLoadedModuleAddress(connection)
      breakpoint_addr = module_load_addr + test_basic.BREAK_ADDRESS_2

      reply = connection.RspRequest('Z0,%x,1' % breakpoint_addr)
      self.assertEqual("OK", reply)
      reply = connection.RspRequest('c')
      gdb_rsp.AssertReplySignal(reply, gdb_rsp.SIGTRAP)

      reply = connection.RspRequest('qWasmStackValue:0;0')
      value = struct.unpack('I', gdb_rsp.DecodeHex(reply))[0]
      self.assertEqual(test_basic.ARG_0, value)

      reply = connection.RspRequest('qWasmStackValue:0;1')
      value = struct.unpack('I', gdb_rsp.DecodeHex(reply))[0]
      self.assertEqual(test_basic.ARG_1, value)

      # invalid index
      reply = connection.RspRequest('qWasmStackValue:0;2')
      self.assertEqual("E03", reply)

  def test_modifying_code_is_disallowed(self):
    with gdb_rsp.LaunchDebugStub(COMMAND) as connection:
      # Pick an arbitrary address in the code segment.
      module_load_addr = gdb_rsp.GetLoadedModuleAddress(connection)
      breakpoint_addr = module_load_addr + test_basic.BREAK_ADDRESS_1
      # Writing to the code area should be disallowed.
      data = '\x00'
      write_command = 'M%x,%x:%s' % (breakpoint_addr, len(data), gdb_rsp.EncodeHex(data))
      reply = connection.RspRequest(write_command)
      self.assertEquals(reply, 'E03')


def Main():
  index = sys.argv.index('--')
  args = sys.argv[index + 1:]
  # The remaining arguments go to unittest.main().
  global COMMAND
  COMMAND = args
  unittest.main(argv=sys.argv[:index])

if __name__ == '__main__':
  Main()

# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Flags: -expose-wasm --wasm-gdb-remote --wasm-pause-waiting-for-debugger test/debugging/wasm/gdb-server/test_files/test_memory.js

import struct
import sys
import unittest

import gdb_rsp
import test_files.test_memory as test_memory

# These are set up by Main().
COMMAND = None

class Tests(unittest.TestCase):
  # Test that reading from an unreadable address gives a sensible error.
  def CheckReadMemoryAtInvalidAddr(self, connection):
    mem_addr = 0xffffffff
    result = connection.RspRequest('m%x,%x' % (mem_addr, 1))
    self.assertEquals(result, 'E02')

  def RunToWasm(self, connection, breakpoint_addr):
    # Set a breakpoint.
    reply = connection.RspRequest('Z0,%x,1' % breakpoint_addr)
    self.assertEqual(reply, 'OK')

    # When we run the program, we should hit the breakpoint.
    reply = connection.RspRequest('c')
    gdb_rsp.AssertReplySignal(reply, gdb_rsp.SIGTRAP)

    # Remove the breakpoint.
    reply = connection.RspRequest('z0,%x,1' % breakpoint_addr)
    self.assertEqual(reply, 'OK')

  def test_reading_and_writing_memory(self):
    with gdb_rsp.LaunchDebugStub(COMMAND) as connection:
      module_load_addr = gdb_rsp.GetLoadedModuleAddress(connection)
      breakpoint_addr = module_load_addr + test_memory.FUNC0_START_ADDR
      self.RunToWasm(connection, breakpoint_addr)

      self.CheckReadMemoryAtInvalidAddr(connection)

      # Check reading code memory space.
      expected_data = b'\0asm'
      result = gdb_rsp.ReadCodeMemory(connection, module_load_addr, len(expected_data))
      self.assertEqual(result, expected_data)

      # Check reading instance memory at a valid range.
      module_id = module_load_addr >> 32
      reply = connection.RspRequest('qWasmMem:%d;%x;%x' % (module_id, 32, 4))
      value = struct.unpack('I', gdb_rsp.DecodeHex(reply))[0]
      self.assertEquals(int(value), 0)

      # Check reading instance memory at an invalid range.
      reply = connection.RspRequest('qWasmMem:%d;%x;%x' % (module_id, 0xf0000000, 4))
      self.assertEqual(reply, 'E03')

  def test_reading_and_writing_data_section(self):
    with gdb_rsp.LaunchDebugStub(COMMAND) as connection:
      module_load_addr = gdb_rsp.GetLoadedModuleAddress(connection)
      breakpoint_addr = module_load_addr + test_memory.FUNC0_START_ADDR
      self.RunToWasm(connection, breakpoint_addr)

      # Check reading instance memory at a valid range.
      module_id = module_load_addr >> 32
      reply = connection.RspRequest('qWasmData:%d;%x;%x' % (module_id, test_memory.DATA_OFFSET, test_memory.DATA_SIZE))
      value = struct.unpack('I', gdb_rsp.DecodeHex(reply))[0]
      self.assertEquals(int(value), test_memory.DATA_CONTENT)

      # Check reading instance memory at an invalid range.
      reply = connection.RspRequest('qWasmData:%d;%x;%x' % (module_id, 0xf0000000, 4))
      self.assertEqual(reply, 'E03')

  def test_wasm_global(self):
    with gdb_rsp.LaunchDebugStub(COMMAND) as connection:
      module_load_addr = gdb_rsp.GetLoadedModuleAddress(connection)
      breakpoint_addr = module_load_addr + test_memory.FUNC0_START_ADDR
      self.RunToWasm(connection, breakpoint_addr)

      # Check reading valid global.
      reply = connection.RspRequest('qWasmGlobal:0;0')
      value = struct.unpack('I', gdb_rsp.DecodeHex(reply))[0]
      self.assertEqual(0, value)

      # Check reading invalid global.
      reply = connection.RspRequest('qWasmGlobal:0;9')
      self.assertEqual("E03", reply)

  def test_wasm_call_stack(self):
    with gdb_rsp.LaunchDebugStub(COMMAND) as connection:
      module_load_addr = gdb_rsp.GetLoadedModuleAddress(connection)
      breakpoint_addr = module_load_addr + test_memory.FUNC0_START_ADDR
      self.RunToWasm(connection, breakpoint_addr)

      reply = connection.RspRequest('qWasmCallStack')
      stack = gdb_rsp.DecodeUInt64Array(reply)
      assert(len(stack) > 2) # At least two Wasm frames, plus one or more JS frames.
      self.assertEqual(stack[0], module_load_addr + test_memory.FUNC0_START_ADDR)
      self.assertEqual(stack[1], module_load_addr + test_memory.FUNC1_RETURN_ADDR)


def Main():
  index = sys.argv.index('--')
  args = sys.argv[index + 1:]
  # The remaining arguments go to unittest.main().
  global COMMAND
  COMMAND = args
  unittest.main(argv=sys.argv[:index])

if __name__ == '__main__':
  Main()

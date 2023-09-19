# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Flags: -expose-wasm --wasm-gdb-remote --wasm-pause-waiting-for-debugger test/debugging/wasm/gdb-server/test_files/test_float.js

import os
import re
import struct
import subprocess
import sys
import unittest

import gdb_rsp
import test_files.test_float as test_float

# These are set up by Main().
COMMAND = None

class Tests(unittest.TestCase):

  def RunToWasm(self, connection):
    module_load_addr = gdb_rsp.GetLoadedModuleAddress(connection)
    breakpoint_addr = module_load_addr + test_float.FUNC_START_ADDR

    # Set a breakpoint.
    reply = connection.RspRequest('Z0,%x,1' % breakpoint_addr)
    self.assertEqual(reply, 'OK')

    # When we run the program, we should hit the breakpoint.
    reply = connection.RspRequest('c')
    gdb_rsp.AssertReplySignal(reply, gdb_rsp.SIGTRAP)

    # Remove the breakpoint.
    reply = connection.RspRequest('z0,%x,1' % breakpoint_addr)
    self.assertEqual(reply, 'OK')

  def test_loaded_modules(self):
    with gdb_rsp.LaunchDebugStub(COMMAND) as connection:
      modules = gdb_rsp.GetLoadedModules(connection)
      connection.Close()
      assert(len(modules) > 0)

  def test_wasm_local_float(self):
    with gdb_rsp.LaunchDebugStub(COMMAND) as connection:
      self.RunToWasm(connection)
      module_load_addr = gdb_rsp.GetLoadedModuleAddress(connection)

      reply = connection.RspRequest('qWasmLocal:0;0')
      value = struct.unpack('f', gdb_rsp.DecodeHex(reply))[0]
      self.assertEqual(test_float.ARG_0, value)

      reply = connection.RspRequest('qWasmLocal:0;1')
      value = struct.unpack('f', gdb_rsp.DecodeHex(reply))[0]
      self.assertEqual(test_float.ARG_1, value)

      # invalid local
      reply = connection.RspRequest('qWasmLocal:0;9')
      self.assertEqual("E03", reply)


def Main():
  index = sys.argv.index('--')
  args = sys.argv[index + 1:]
  # The remaining arguments go to unittest.main().
  global COMMAND
  COMMAND = args
  unittest.main(argv=sys.argv[:index])

if __name__ == '__main__':
  Main()

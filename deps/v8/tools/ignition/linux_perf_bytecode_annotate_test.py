# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import StringIO
import unittest
import linux_perf_bytecode_annotate as bytecode_annotate


PERF_SCRIPT_OUTPUT = """
# This line is a comment
# This should be ignored too
#
#    cdefab01  aRandomSymbol::Name(to, be, ignored)

   00000000 firstSymbol
   00000123 secondSymbol

   01234567 foo
   abcdef76 BytecodeHandler:bar+0x12
   76543210 baz
   abcdef76 BytecodeHandler:bar+0x16
   76543210 baz

   01234567 foo
   abcdef76 BytecodeHandler:foo+0x1
   76543210 baz
   abcdef76 BytecodeHandler:bar+0x2
   76543210 bar

   abcdef76 BytecodeHandler:bar+0x19

   abcdef76 BytecodeHandler:bar+0x12

   abcdef76 BytecodeHandler:bar+0x12
"""


D8_CODEGEN_OUTPUT = """
kind = BYTECODE_HANDLER
name = foo
compiler = turbofan
Instructions (size = 3)
0x3101394a3c0     0  55             push rbp
0x3101394a3c1     1  ffe3           jmp rbx

kind = BYTECODE_HANDLER
name = bar
compiler = turbofan
Instructions (size = 5)
0x3101394b3c0     0  55             push rbp
0x3101394b3c1     1  4883c428       REX.W addq rsp,0x28
# Unexpected comment
0x3101394b3c5     5  ffe3           jmp rbx

kind = BYTECODE_HANDLER
name = baz
compiler = turbofan
Instructions (size = 5)
0x3101394c3c0     0  55             push rbp
0x3101394c3c1     1  4883c428       REX.W addq rsp,0x28
0x3101394c3c5     5  ffe3           jmp rbx
"""


class LinuxPerfBytecodeAnnotateTest(unittest.TestCase):

  def test_bytecode_offset_generator(self):
    perf_stream = StringIO.StringIO(PERF_SCRIPT_OUTPUT)
    offsets = list(
        bytecode_annotate.bytecode_offset_generator(perf_stream, "bar"))
    self.assertListEqual(offsets, [18, 25, 18, 18])

  def test_bytecode_disassembly_generator(self):
    codegen_stream = StringIO.StringIO(D8_CODEGEN_OUTPUT)
    disassembly = list(
        bytecode_annotate.bytecode_disassembly_generator(codegen_stream, "bar"))
    self.assertListEqual(disassembly, [
        "0x3101394b3c0     0  55             push rbp",
        "0x3101394b3c1     1  4883c428       REX.W addq rsp,0x28",
        "0x3101394b3c5     5  ffe3           jmp rbx"])


if __name__ == "__main__":
  unittest.main()

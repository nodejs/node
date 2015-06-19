// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_MIPS_INSTRUCTION_CODES_MIPS_H_
#define V8_COMPILER_MIPS_INSTRUCTION_CODES_MIPS_H_

namespace v8 {
namespace internal {
namespace compiler {

// MIPS64-specific opcodes that specify which assembly sequence to emit.
// Most opcodes specify a single instruction.
#define TARGET_ARCH_OPCODE_LIST(V)  \
  V(Mips64Add)                      \
  V(Mips64Dadd)                     \
  V(Mips64Sub)                      \
  V(Mips64Dsub)                     \
  V(Mips64Mul)                      \
  V(Mips64MulHigh)                  \
  V(Mips64MulHighU)                 \
  V(Mips64Dmul)                     \
  V(Mips64Div)                      \
  V(Mips64Ddiv)                     \
  V(Mips64DivU)                     \
  V(Mips64DdivU)                    \
  V(Mips64Mod)                      \
  V(Mips64Dmod)                     \
  V(Mips64ModU)                     \
  V(Mips64DmodU)                    \
  V(Mips64And)                      \
  V(Mips64Or)                       \
  V(Mips64Xor)                      \
  V(Mips64Clz)                      \
  V(Mips64Shl)                      \
  V(Mips64Shr)                      \
  V(Mips64Sar)                      \
  V(Mips64Ext)                      \
  V(Mips64Dext)                     \
  V(Mips64Dshl)                     \
  V(Mips64Dshr)                     \
  V(Mips64Dsar)                     \
  V(Mips64Ror)                      \
  V(Mips64Dror)                     \
  V(Mips64Mov)                      \
  V(Mips64Tst)                      \
  V(Mips64Cmp)                      \
  V(Mips64CmpS)                     \
  V(Mips64AddS)                     \
  V(Mips64SubS)                     \
  V(Mips64MulS)                     \
  V(Mips64DivS)                     \
  V(Mips64ModS)                     \
  V(Mips64AbsS)                     \
  V(Mips64SqrtS)                    \
  V(Mips64CmpD)                     \
  V(Mips64AddD)                     \
  V(Mips64SubD)                     \
  V(Mips64MulD)                     \
  V(Mips64DivD)                     \
  V(Mips64ModD)                     \
  V(Mips64AbsD)                     \
  V(Mips64SqrtD)                    \
  V(Mips64Float64RoundDown)         \
  V(Mips64Float64RoundTruncate)     \
  V(Mips64Float64RoundUp)           \
  V(Mips64CvtSD)                    \
  V(Mips64CvtDS)                    \
  V(Mips64TruncWD)                  \
  V(Mips64TruncUwD)                 \
  V(Mips64CvtDW)                    \
  V(Mips64CvtDUw)                   \
  V(Mips64Lb)                       \
  V(Mips64Lbu)                      \
  V(Mips64Sb)                       \
  V(Mips64Lh)                       \
  V(Mips64Lhu)                      \
  V(Mips64Sh)                       \
  V(Mips64Ld)                       \
  V(Mips64Lw)                       \
  V(Mips64Sw)                       \
  V(Mips64Sd)                       \
  V(Mips64Lwc1)                     \
  V(Mips64Swc1)                     \
  V(Mips64Ldc1)                     \
  V(Mips64Sdc1)                     \
  V(Mips64Float64ExtractLowWord32)  \
  V(Mips64Float64ExtractHighWord32) \
  V(Mips64Float64InsertLowWord32)   \
  V(Mips64Float64InsertHighWord32)  \
  V(Mips64Push)                     \
  V(Mips64StoreToStackSlot)         \
  V(Mips64StackClaim)               \
  V(Mips64StoreWriteBarrier)


// Addressing modes represent the "shape" of inputs to an instruction.
// Many instructions support multiple addressing modes. Addressing modes
// are encoded into the InstructionCode of the instruction and tell the
// code generator after register allocation which assembler method to call.
//
// We use the following local notation for addressing modes:
//
// R = register
// O = register or stack slot
// D = double register
// I = immediate (handle, external, int32)
// MRI = [register + immediate]
// MRR = [register + register]
// TODO(plind): Add the new r6 address modes.
#define TARGET_ADDRESSING_MODE_LIST(V) \
  V(MRI) /* [%r0 + K] */               \
  V(MRR) /* [%r0 + %r1] */


}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_MIPS_INSTRUCTION_CODES_MIPS_H_

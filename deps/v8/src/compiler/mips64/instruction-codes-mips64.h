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
  V(Mips64DaddOvf)                  \
  V(Mips64Sub)                      \
  V(Mips64Dsub)                     \
  V(Mips64DsubOvf)                  \
  V(Mips64Mul)                      \
  V(Mips64MulHigh)                  \
  V(Mips64DMulHigh)                 \
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
  V(Mips64Nor)                      \
  V(Mips64Xor)                      \
  V(Mips64Clz)                      \
  V(Mips64Shl)                      \
  V(Mips64Shr)                      \
  V(Mips64Sar)                      \
  V(Mips64Ext)                      \
  V(Mips64Ins)                      \
  V(Mips64Dext)                     \
  V(Mips64Dins)                     \
  V(Mips64Dclz)                     \
  V(Mips64Ctz)                      \
  V(Mips64Dctz)                     \
  V(Mips64Popcnt)                   \
  V(Mips64Dpopcnt)                  \
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
  V(Mips64MaxS)                     \
  V(Mips64MinS)                     \
  V(Mips64CmpD)                     \
  V(Mips64AddD)                     \
  V(Mips64SubD)                     \
  V(Mips64MulD)                     \
  V(Mips64DivD)                     \
  V(Mips64ModD)                     \
  V(Mips64AbsD)                     \
  V(Mips64SqrtD)                    \
  V(Mips64MaxD)                     \
  V(Mips64MinD)                     \
  V(Mips64Float64RoundDown)         \
  V(Mips64Float64RoundTruncate)     \
  V(Mips64Float64RoundUp)           \
  V(Mips64Float64RoundTiesEven)     \
  V(Mips64Float32RoundDown)         \
  V(Mips64Float32RoundTruncate)     \
  V(Mips64Float32RoundUp)           \
  V(Mips64Float32RoundTiesEven)     \
  V(Mips64CvtSD)                    \
  V(Mips64CvtDS)                    \
  V(Mips64TruncWD)                  \
  V(Mips64RoundWD)                  \
  V(Mips64FloorWD)                  \
  V(Mips64CeilWD)                   \
  V(Mips64TruncWS)                  \
  V(Mips64RoundWS)                  \
  V(Mips64FloorWS)                  \
  V(Mips64CeilWS)                   \
  V(Mips64TruncLS)                  \
  V(Mips64TruncLD)                  \
  V(Mips64TruncUwD)                 \
  V(Mips64TruncUwS)                 \
  V(Mips64TruncUlS)                 \
  V(Mips64TruncUlD)                 \
  V(Mips64CvtDW)                    \
  V(Mips64CvtSL)                    \
  V(Mips64CvtSW)                    \
  V(Mips64CvtSUw)                   \
  V(Mips64CvtSUl)                   \
  V(Mips64CvtDL)                    \
  V(Mips64CvtDUw)                   \
  V(Mips64CvtDUl)                   \
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
  V(Mips64BitcastDL)                \
  V(Mips64BitcastLD)                \
  V(Mips64Float64ExtractLowWord32)  \
  V(Mips64Float64ExtractHighWord32) \
  V(Mips64Float64InsertLowWord32)   \
  V(Mips64Float64InsertHighWord32)  \
  V(Mips64Float64Max)               \
  V(Mips64Float64Min)               \
  V(Mips64Float32Max)               \
  V(Mips64Float32Min)               \
  V(Mips64Push)                     \
  V(Mips64StoreToStackSlot)         \
  V(Mips64StackClaim)

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

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
  V(Mips64MulOvf)                   \
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
  V(Mips64And32)                    \
  V(Mips64Or)                       \
  V(Mips64Or32)                     \
  V(Mips64Nor)                      \
  V(Mips64Nor32)                    \
  V(Mips64Xor)                      \
  V(Mips64Xor32)                    \
  V(Mips64Clz)                      \
  V(Mips64Lsa)                      \
  V(Mips64Dlsa)                     \
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
  V(Mips64NegS)                     \
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
  V(Mips64NegD)                     \
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
  V(Mips64Ulh)                      \
  V(Mips64Lhu)                      \
  V(Mips64Ulhu)                     \
  V(Mips64Sh)                       \
  V(Mips64Ush)                      \
  V(Mips64Ld)                       \
  V(Mips64Uld)                      \
  V(Mips64Lw)                       \
  V(Mips64Ulw)                      \
  V(Mips64Lwu)                      \
  V(Mips64Ulwu)                     \
  V(Mips64Sw)                       \
  V(Mips64Usw)                      \
  V(Mips64Sd)                       \
  V(Mips64Usd)                      \
  V(Mips64Lwc1)                     \
  V(Mips64Ulwc1)                    \
  V(Mips64Swc1)                     \
  V(Mips64Uswc1)                    \
  V(Mips64Ldc1)                     \
  V(Mips64Uldc1)                    \
  V(Mips64Sdc1)                     \
  V(Mips64Usdc1)                    \
  V(Mips64BitcastDL)                \
  V(Mips64BitcastLD)                \
  V(Mips64Float64ExtractLowWord32)  \
  V(Mips64Float64ExtractHighWord32) \
  V(Mips64Float64InsertLowWord32)   \
  V(Mips64Float64InsertHighWord32)  \
  V(Mips64Float32Max)               \
  V(Mips64Float64Max)               \
  V(Mips64Float32Min)               \
  V(Mips64Float64Min)               \
  V(Mips64Float64SilenceNaN)        \
  V(Mips64Push)                     \
  V(Mips64Peek)                     \
  V(Mips64StoreToStackSlot)         \
  V(Mips64ByteSwap64)               \
  V(Mips64ByteSwap32)               \
  V(Mips64StackClaim)               \
  V(Mips64Seb)                      \
  V(Mips64Seh)                      \
  V(Mips64AssertEqual)              \
  V(Mips64S128Zero)                 \
  V(Mips64I32x4Splat)               \
  V(Mips64I32x4ExtractLane)         \
  V(Mips64I32x4ReplaceLane)         \
  V(Mips64I32x4Add)                 \
  V(Mips64I32x4AddHoriz)            \
  V(Mips64I32x4Sub)                 \
  V(Mips64F32x4Splat)               \
  V(Mips64F32x4ExtractLane)         \
  V(Mips64F32x4ReplaceLane)         \
  V(Mips64F32x4SConvertI32x4)       \
  V(Mips64F32x4UConvertI32x4)       \
  V(Mips64I32x4Mul)                 \
  V(Mips64I32x4MaxS)                \
  V(Mips64I32x4MinS)                \
  V(Mips64I32x4Eq)                  \
  V(Mips64I32x4Ne)                  \
  V(Mips64I32x4Shl)                 \
  V(Mips64I32x4ShrS)                \
  V(Mips64I32x4ShrU)                \
  V(Mips64I32x4MaxU)                \
  V(Mips64I32x4MinU)                \
  V(Mips64F32x4Abs)                 \
  V(Mips64F32x4Neg)                 \
  V(Mips64F32x4RecipApprox)         \
  V(Mips64F32x4RecipSqrtApprox)     \
  V(Mips64F32x4Add)                 \
  V(Mips64F32x4AddHoriz)            \
  V(Mips64F32x4Sub)                 \
  V(Mips64F32x4Mul)                 \
  V(Mips64F32x4Max)                 \
  V(Mips64F32x4Min)                 \
  V(Mips64F32x4Eq)                  \
  V(Mips64F32x4Ne)                  \
  V(Mips64F32x4Lt)                  \
  V(Mips64F32x4Le)                  \
  V(Mips64I32x4SConvertF32x4)       \
  V(Mips64I32x4UConvertF32x4)       \
  V(Mips64I32x4Neg)                 \
  V(Mips64I32x4GtS)                 \
  V(Mips64I32x4GeS)                 \
  V(Mips64I32x4GtU)                 \
  V(Mips64I32x4GeU)                 \
  V(Mips64I16x8Splat)               \
  V(Mips64I16x8ExtractLane)         \
  V(Mips64I16x8ReplaceLane)         \
  V(Mips64I16x8Neg)                 \
  V(Mips64I16x8Shl)                 \
  V(Mips64I16x8ShrS)                \
  V(Mips64I16x8ShrU)                \
  V(Mips64I16x8Add)                 \
  V(Mips64I16x8AddSaturateS)        \
  V(Mips64I16x8AddHoriz)            \
  V(Mips64I16x8Sub)                 \
  V(Mips64I16x8SubSaturateS)        \
  V(Mips64I16x8Mul)                 \
  V(Mips64I16x8MaxS)                \
  V(Mips64I16x8MinS)                \
  V(Mips64I16x8Eq)                  \
  V(Mips64I16x8Ne)                  \
  V(Mips64I16x8GtS)                 \
  V(Mips64I16x8GeS)                 \
  V(Mips64I16x8AddSaturateU)        \
  V(Mips64I16x8SubSaturateU)        \
  V(Mips64I16x8MaxU)                \
  V(Mips64I16x8MinU)                \
  V(Mips64I16x8GtU)                 \
  V(Mips64I16x8GeU)                 \
  V(Mips64I8x16Splat)               \
  V(Mips64I8x16ExtractLane)         \
  V(Mips64I8x16ReplaceLane)         \
  V(Mips64I8x16Neg)                 \
  V(Mips64I8x16Shl)                 \
  V(Mips64I8x16ShrS)                \
  V(Mips64I8x16Add)                 \
  V(Mips64I8x16AddSaturateS)        \
  V(Mips64I8x16Sub)                 \
  V(Mips64I8x16SubSaturateS)        \
  V(Mips64I8x16Mul)                 \
  V(Mips64I8x16MaxS)                \
  V(Mips64I8x16MinS)                \
  V(Mips64I8x16Eq)                  \
  V(Mips64I8x16Ne)                  \
  V(Mips64I8x16GtS)                 \
  V(Mips64I8x16GeS)                 \
  V(Mips64I8x16ShrU)                \
  V(Mips64I8x16AddSaturateU)        \
  V(Mips64I8x16SubSaturateU)        \
  V(Mips64I8x16MaxU)                \
  V(Mips64I8x16MinU)                \
  V(Mips64I8x16GtU)                 \
  V(Mips64I8x16GeU)                 \
  V(Mips64S128And)                  \
  V(Mips64S128Or)                   \
  V(Mips64S128Xor)                  \
  V(Mips64S128Not)                  \
  V(Mips64S128Select)               \
  V(Mips64S1x4AnyTrue)              \
  V(Mips64S1x4AllTrue)              \
  V(Mips64S1x8AnyTrue)              \
  V(Mips64S1x8AllTrue)              \
  V(Mips64S1x16AnyTrue)             \
  V(Mips64S1x16AllTrue)             \
  V(Mips64S32x4InterleaveRight)     \
  V(Mips64S32x4InterleaveLeft)      \
  V(Mips64S32x4PackEven)            \
  V(Mips64S32x4PackOdd)             \
  V(Mips64S32x4InterleaveEven)      \
  V(Mips64S32x4InterleaveOdd)       \
  V(Mips64S32x4Shuffle)             \
  V(Mips64S16x8InterleaveRight)     \
  V(Mips64S16x8InterleaveLeft)      \
  V(Mips64S16x8PackEven)            \
  V(Mips64S16x8PackOdd)             \
  V(Mips64S16x8InterleaveEven)      \
  V(Mips64S16x8InterleaveOdd)       \
  V(Mips64S16x4Reverse)             \
  V(Mips64S16x2Reverse)             \
  V(Mips64S8x16InterleaveRight)     \
  V(Mips64S8x16InterleaveLeft)      \
  V(Mips64S8x16PackEven)            \
  V(Mips64S8x16PackOdd)             \
  V(Mips64S8x16InterleaveEven)      \
  V(Mips64S8x16InterleaveOdd)       \
  V(Mips64S8x16Shuffle)             \
  V(Mips64S8x16Concat)              \
  V(Mips64S8x8Reverse)              \
  V(Mips64S8x4Reverse)              \
  V(Mips64S8x2Reverse)              \
  V(Mips64MsaLd)                    \
  V(Mips64MsaSt)                    \
  V(Mips64I32x4SConvertI16x8Low)    \
  V(Mips64I32x4SConvertI16x8High)   \
  V(Mips64I32x4UConvertI16x8Low)    \
  V(Mips64I32x4UConvertI16x8High)   \
  V(Mips64I16x8SConvertI8x16Low)    \
  V(Mips64I16x8SConvertI8x16High)   \
  V(Mips64I16x8SConvertI32x4)       \
  V(Mips64I16x8UConvertI32x4)       \
  V(Mips64I16x8UConvertI8x16Low)    \
  V(Mips64I16x8UConvertI8x16High)   \
  V(Mips64I8x16SConvertI16x8)       \
  V(Mips64I8x16UConvertI16x8)

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

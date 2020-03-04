// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_ARM_INSTRUCTION_CODES_ARM_H_
#define V8_COMPILER_BACKEND_ARM_INSTRUCTION_CODES_ARM_H_

namespace v8 {
namespace internal {
namespace compiler {

// ARM-specific opcodes that specify which assembly sequence to emit.
// Most opcodes specify a single instruction.
#define TARGET_ARCH_OPCODE_LIST(V) \
  V(ArmAdd)                        \
  V(ArmAnd)                        \
  V(ArmBic)                        \
  V(ArmClz)                        \
  V(ArmCmp)                        \
  V(ArmCmn)                        \
  V(ArmTst)                        \
  V(ArmTeq)                        \
  V(ArmOrr)                        \
  V(ArmEor)                        \
  V(ArmSub)                        \
  V(ArmRsb)                        \
  V(ArmMul)                        \
  V(ArmMla)                        \
  V(ArmMls)                        \
  V(ArmSmull)                      \
  V(ArmSmmul)                      \
  V(ArmSmmla)                      \
  V(ArmUmull)                      \
  V(ArmSdiv)                       \
  V(ArmUdiv)                       \
  V(ArmMov)                        \
  V(ArmMvn)                        \
  V(ArmBfc)                        \
  V(ArmUbfx)                       \
  V(ArmSbfx)                       \
  V(ArmSxtb)                       \
  V(ArmSxth)                       \
  V(ArmSxtab)                      \
  V(ArmSxtah)                      \
  V(ArmUxtb)                       \
  V(ArmUxth)                       \
  V(ArmUxtab)                      \
  V(ArmRbit)                       \
  V(ArmRev)                        \
  V(ArmUxtah)                      \
  V(ArmAddPair)                    \
  V(ArmSubPair)                    \
  V(ArmMulPair)                    \
  V(ArmLslPair)                    \
  V(ArmLsrPair)                    \
  V(ArmAsrPair)                    \
  V(ArmVcmpF32)                    \
  V(ArmVaddF32)                    \
  V(ArmVsubF32)                    \
  V(ArmVmulF32)                    \
  V(ArmVmlaF32)                    \
  V(ArmVmlsF32)                    \
  V(ArmVdivF32)                    \
  V(ArmVabsF32)                    \
  V(ArmVnegF32)                    \
  V(ArmVsqrtF32)                   \
  V(ArmVcmpF64)                    \
  V(ArmVaddF64)                    \
  V(ArmVsubF64)                    \
  V(ArmVmulF64)                    \
  V(ArmVmlaF64)                    \
  V(ArmVmlsF64)                    \
  V(ArmVdivF64)                    \
  V(ArmVmodF64)                    \
  V(ArmVabsF64)                    \
  V(ArmVnegF64)                    \
  V(ArmVsqrtF64)                   \
  V(ArmVrintmF32)                  \
  V(ArmVrintmF64)                  \
  V(ArmVrintpF32)                  \
  V(ArmVrintpF64)                  \
  V(ArmVrintzF32)                  \
  V(ArmVrintzF64)                  \
  V(ArmVrintaF64)                  \
  V(ArmVrintnF32)                  \
  V(ArmVrintnF64)                  \
  V(ArmVcvtF32F64)                 \
  V(ArmVcvtF64F32)                 \
  V(ArmVcvtF32S32)                 \
  V(ArmVcvtF32U32)                 \
  V(ArmVcvtF64S32)                 \
  V(ArmVcvtF64U32)                 \
  V(ArmVcvtS32F32)                 \
  V(ArmVcvtU32F32)                 \
  V(ArmVcvtS32F64)                 \
  V(ArmVcvtU32F64)                 \
  V(ArmVmovU32F32)                 \
  V(ArmVmovF32U32)                 \
  V(ArmVmovLowU32F64)              \
  V(ArmVmovLowF64U32)              \
  V(ArmVmovHighU32F64)             \
  V(ArmVmovHighF64U32)             \
  V(ArmVmovF64U32U32)              \
  V(ArmVmovU32U32F64)              \
  V(ArmVldrF32)                    \
  V(ArmVstrF32)                    \
  V(ArmVldrF64)                    \
  V(ArmVld1F64)                    \
  V(ArmVstrF64)                    \
  V(ArmVst1F64)                    \
  V(ArmVld1S128)                   \
  V(ArmVst1S128)                   \
  V(ArmFloat32Max)                 \
  V(ArmFloat64Max)                 \
  V(ArmFloat32Min)                 \
  V(ArmFloat64Min)                 \
  V(ArmFloat64SilenceNaN)          \
  V(ArmLdrb)                       \
  V(ArmLdrsb)                      \
  V(ArmStrb)                       \
  V(ArmLdrh)                       \
  V(ArmLdrsh)                      \
  V(ArmStrh)                       \
  V(ArmLdr)                        \
  V(ArmStr)                        \
  V(ArmPush)                       \
  V(ArmPoke)                       \
  V(ArmPeek)                       \
  V(ArmDmbIsh)                     \
  V(ArmDsbIsb)                     \
  V(ArmF64x2Splat)                 \
  V(ArmF64x2ExtractLane)           \
  V(ArmF64x2ReplaceLane)           \
  V(ArmF64x2Abs)                   \
  V(ArmF64x2Neg)                   \
  V(ArmF64x2Sqrt)                  \
  V(ArmF64x2Add)                   \
  V(ArmF64x2Sub)                   \
  V(ArmF64x2Mul)                   \
  V(ArmF64x2Div)                   \
  V(ArmF64x2Min)                   \
  V(ArmF64x2Max)                   \
  V(ArmF64x2Eq)                    \
  V(ArmF64x2Ne)                    \
  V(ArmF64x2Lt)                    \
  V(ArmF64x2Le)                    \
  V(ArmF32x4Splat)                 \
  V(ArmF32x4ExtractLane)           \
  V(ArmF32x4ReplaceLane)           \
  V(ArmF32x4SConvertI32x4)         \
  V(ArmF32x4UConvertI32x4)         \
  V(ArmF32x4Abs)                   \
  V(ArmF32x4Neg)                   \
  V(ArmF32x4Sqrt)                  \
  V(ArmF32x4RecipApprox)           \
  V(ArmF32x4RecipSqrtApprox)       \
  V(ArmF32x4Add)                   \
  V(ArmF32x4AddHoriz)              \
  V(ArmF32x4Sub)                   \
  V(ArmF32x4Mul)                   \
  V(ArmF32x4Div)                   \
  V(ArmF32x4Min)                   \
  V(ArmF32x4Max)                   \
  V(ArmF32x4Eq)                    \
  V(ArmF32x4Ne)                    \
  V(ArmF32x4Lt)                    \
  V(ArmF32x4Le)                    \
  V(ArmI64x2SplatI32Pair)          \
  V(ArmI64x2ReplaceLaneI32Pair)    \
  V(ArmI64x2Neg)                   \
  V(ArmI64x2Shl)                   \
  V(ArmI64x2ShrS)                  \
  V(ArmI64x2Add)                   \
  V(ArmI64x2Sub)                   \
  V(ArmI64x2ShrU)                  \
  V(ArmI32x4Splat)                 \
  V(ArmI32x4ExtractLane)           \
  V(ArmI32x4ReplaceLane)           \
  V(ArmI32x4SConvertF32x4)         \
  V(ArmI32x4SConvertI16x8Low)      \
  V(ArmI32x4SConvertI16x8High)     \
  V(ArmI32x4Neg)                   \
  V(ArmI32x4Shl)                   \
  V(ArmI32x4ShrS)                  \
  V(ArmI32x4Add)                   \
  V(ArmI32x4AddHoriz)              \
  V(ArmI32x4Sub)                   \
  V(ArmI32x4Mul)                   \
  V(ArmI32x4MinS)                  \
  V(ArmI32x4MaxS)                  \
  V(ArmI32x4Eq)                    \
  V(ArmI32x4Ne)                    \
  V(ArmI32x4GtS)                   \
  V(ArmI32x4GeS)                   \
  V(ArmI32x4UConvertF32x4)         \
  V(ArmI32x4UConvertI16x8Low)      \
  V(ArmI32x4UConvertI16x8High)     \
  V(ArmI32x4ShrU)                  \
  V(ArmI32x4MinU)                  \
  V(ArmI32x4MaxU)                  \
  V(ArmI32x4GtU)                   \
  V(ArmI32x4GeU)                   \
  V(ArmI16x8Splat)                 \
  V(ArmI16x8ExtractLaneS)          \
  V(ArmI16x8ReplaceLane)           \
  V(ArmI16x8SConvertI8x16Low)      \
  V(ArmI16x8SConvertI8x16High)     \
  V(ArmI16x8Neg)                   \
  V(ArmI16x8Shl)                   \
  V(ArmI16x8ShrS)                  \
  V(ArmI16x8SConvertI32x4)         \
  V(ArmI16x8Add)                   \
  V(ArmI16x8AddSaturateS)          \
  V(ArmI16x8AddHoriz)              \
  V(ArmI16x8Sub)                   \
  V(ArmI16x8SubSaturateS)          \
  V(ArmI16x8Mul)                   \
  V(ArmI16x8MinS)                  \
  V(ArmI16x8MaxS)                  \
  V(ArmI16x8Eq)                    \
  V(ArmI16x8Ne)                    \
  V(ArmI16x8GtS)                   \
  V(ArmI16x8GeS)                   \
  V(ArmI16x8ExtractLaneU)          \
  V(ArmI16x8UConvertI8x16Low)      \
  V(ArmI16x8UConvertI8x16High)     \
  V(ArmI16x8ShrU)                  \
  V(ArmI16x8UConvertI32x4)         \
  V(ArmI16x8AddSaturateU)          \
  V(ArmI16x8SubSaturateU)          \
  V(ArmI16x8MinU)                  \
  V(ArmI16x8MaxU)                  \
  V(ArmI16x8GtU)                   \
  V(ArmI16x8GeU)                   \
  V(ArmI8x16Splat)                 \
  V(ArmI8x16ExtractLaneS)          \
  V(ArmI8x16ReplaceLane)           \
  V(ArmI8x16Neg)                   \
  V(ArmI8x16Shl)                   \
  V(ArmI8x16ShrS)                  \
  V(ArmI8x16SConvertI16x8)         \
  V(ArmI8x16Add)                   \
  V(ArmI8x16AddSaturateS)          \
  V(ArmI8x16Sub)                   \
  V(ArmI8x16SubSaturateS)          \
  V(ArmI8x16Mul)                   \
  V(ArmI8x16MinS)                  \
  V(ArmI8x16MaxS)                  \
  V(ArmI8x16Eq)                    \
  V(ArmI8x16Ne)                    \
  V(ArmI8x16GtS)                   \
  V(ArmI8x16GeS)                   \
  V(ArmI8x16ExtractLaneU)          \
  V(ArmI8x16ShrU)                  \
  V(ArmI8x16UConvertI16x8)         \
  V(ArmI8x16AddSaturateU)          \
  V(ArmI8x16SubSaturateU)          \
  V(ArmI8x16MinU)                  \
  V(ArmI8x16MaxU)                  \
  V(ArmI8x16GtU)                   \
  V(ArmI8x16GeU)                   \
  V(ArmS128Zero)                   \
  V(ArmS128Dup)                    \
  V(ArmS128And)                    \
  V(ArmS128Or)                     \
  V(ArmS128Xor)                    \
  V(ArmS128Not)                    \
  V(ArmS128Select)                 \
  V(ArmS32x4ZipLeft)               \
  V(ArmS32x4ZipRight)              \
  V(ArmS32x4UnzipLeft)             \
  V(ArmS32x4UnzipRight)            \
  V(ArmS32x4TransposeLeft)         \
  V(ArmS32x4TransposeRight)        \
  V(ArmS32x4Shuffle)               \
  V(ArmS16x8ZipLeft)               \
  V(ArmS16x8ZipRight)              \
  V(ArmS16x8UnzipLeft)             \
  V(ArmS16x8UnzipRight)            \
  V(ArmS16x8TransposeLeft)         \
  V(ArmS16x8TransposeRight)        \
  V(ArmS8x16ZipLeft)               \
  V(ArmS8x16ZipRight)              \
  V(ArmS8x16UnzipLeft)             \
  V(ArmS8x16UnzipRight)            \
  V(ArmS8x16TransposeLeft)         \
  V(ArmS8x16TransposeRight)        \
  V(ArmS8x16Concat)                \
  V(ArmS8x16Swizzle)               \
  V(ArmS8x16Shuffle)               \
  V(ArmS32x2Reverse)               \
  V(ArmS16x4Reverse)               \
  V(ArmS16x2Reverse)               \
  V(ArmS8x8Reverse)                \
  V(ArmS8x4Reverse)                \
  V(ArmS8x2Reverse)                \
  V(ArmS1x4AnyTrue)                \
  V(ArmS1x4AllTrue)                \
  V(ArmS1x8AnyTrue)                \
  V(ArmS1x8AllTrue)                \
  V(ArmS1x16AnyTrue)               \
  V(ArmS1x16AllTrue)               \
  V(ArmS8x16LoadSplat)             \
  V(ArmS16x8LoadSplat)             \
  V(ArmS32x4LoadSplat)             \
  V(ArmS64x2LoadSplat)             \
  V(ArmI16x8Load8x8S)              \
  V(ArmI16x8Load8x8U)              \
  V(ArmI32x4Load16x4S)             \
  V(ArmI32x4Load16x4U)             \
  V(ArmI64x2Load32x2S)             \
  V(ArmI64x2Load32x2U)             \
  V(ArmWord32AtomicPairLoad)       \
  V(ArmWord32AtomicPairStore)      \
  V(ArmWord32AtomicPairAdd)        \
  V(ArmWord32AtomicPairSub)        \
  V(ArmWord32AtomicPairAnd)        \
  V(ArmWord32AtomicPairOr)         \
  V(ArmWord32AtomicPairXor)        \
  V(ArmWord32AtomicPairExchange)   \
  V(ArmWord32AtomicPairCompareExchange)

// Addressing modes represent the "shape" of inputs to an instruction.
// Many instructions support multiple addressing modes. Addressing modes
// are encoded into the InstructionCode of the instruction and tell the
// code generator after register allocation which assembler method to call.
#define TARGET_ADDRESSING_MODE_LIST(V)  \
  V(Offset_RI)        /* [%r0 + K] */   \
  V(Offset_RR)        /* [%r0 + %r1] */ \
  V(Operand2_I)       /* K */           \
  V(Operand2_R)       /* %r0 */         \
  V(Operand2_R_ASR_I) /* %r0 ASR K */   \
  V(Operand2_R_LSL_I) /* %r0 LSL K */   \
  V(Operand2_R_LSR_I) /* %r0 LSR K */   \
  V(Operand2_R_ROR_I) /* %r0 ROR K */   \
  V(Operand2_R_ASR_R) /* %r0 ASR %r1 */ \
  V(Operand2_R_LSL_R) /* %r0 LSL %r1 */ \
  V(Operand2_R_LSR_R) /* %r0 LSR %r1 */ \
  V(Operand2_R_ROR_R) /* %r0 ROR %r1 */

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BACKEND_ARM_INSTRUCTION_CODES_ARM_H_

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_ARM_INSTRUCTION_CODES_ARM_H_
#define V8_COMPILER_ARM_INSTRUCTION_CODES_ARM_H_

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
  V(ArmVstrF64)                    \
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
  V(ArmFloat32x4Splat)             \
  V(ArmFloat32x4ExtractLane)       \
  V(ArmFloat32x4ReplaceLane)       \
  V(ArmFloat32x4FromInt32x4)       \
  V(ArmFloat32x4FromUint32x4)      \
  V(ArmFloat32x4Abs)               \
  V(ArmFloat32x4Neg)               \
  V(ArmFloat32x4Add)               \
  V(ArmFloat32x4Sub)               \
  V(ArmFloat32x4Equal)             \
  V(ArmFloat32x4NotEqual)          \
  V(ArmInt32x4Splat)               \
  V(ArmInt32x4ExtractLane)         \
  V(ArmInt32x4ReplaceLane)         \
  V(ArmInt32x4FromFloat32x4)       \
  V(ArmUint32x4FromFloat32x4)      \
  V(ArmInt32x4Neg)                 \
  V(ArmInt32x4ShiftLeftByScalar)   \
  V(ArmInt32x4ShiftRightByScalar)  \
  V(ArmInt32x4Add)                 \
  V(ArmInt32x4Sub)                 \
  V(ArmInt32x4Mul)                 \
  V(ArmInt32x4Min)                 \
  V(ArmInt32x4Max)                 \
  V(ArmInt32x4Equal)               \
  V(ArmInt32x4NotEqual)            \
  V(ArmInt32x4GreaterThan)         \
  V(ArmInt32x4GreaterThanOrEqual)  \
  V(ArmUint32x4ShiftRightByScalar) \
  V(ArmUint32x4Min)                \
  V(ArmUint32x4Max)                \
  V(ArmUint32x4GreaterThan)        \
  V(ArmUint32x4GreaterThanOrEqual) \
  V(ArmInt16x8Splat)               \
  V(ArmInt16x8ExtractLane)         \
  V(ArmInt16x8ReplaceLane)         \
  V(ArmInt16x8Neg)                 \
  V(ArmInt16x8ShiftLeftByScalar)   \
  V(ArmInt16x8ShiftRightByScalar)  \
  V(ArmInt16x8Add)                 \
  V(ArmInt16x8AddSaturate)         \
  V(ArmInt16x8Sub)                 \
  V(ArmInt16x8SubSaturate)         \
  V(ArmInt16x8Mul)                 \
  V(ArmInt16x8Min)                 \
  V(ArmInt16x8Max)                 \
  V(ArmInt16x8Equal)               \
  V(ArmInt16x8NotEqual)            \
  V(ArmInt16x8GreaterThan)         \
  V(ArmInt16x8GreaterThanOrEqual)  \
  V(ArmUint16x8ShiftRightByScalar) \
  V(ArmUint16x8AddSaturate)        \
  V(ArmUint16x8SubSaturate)        \
  V(ArmUint16x8Min)                \
  V(ArmUint16x8Max)                \
  V(ArmUint16x8GreaterThan)        \
  V(ArmUint16x8GreaterThanOrEqual) \
  V(ArmInt8x16Splat)               \
  V(ArmInt8x16ExtractLane)         \
  V(ArmInt8x16ReplaceLane)         \
  V(ArmInt8x16Neg)                 \
  V(ArmInt8x16ShiftLeftByScalar)   \
  V(ArmInt8x16ShiftRightByScalar)  \
  V(ArmInt8x16Add)                 \
  V(ArmInt8x16AddSaturate)         \
  V(ArmInt8x16Sub)                 \
  V(ArmInt8x16SubSaturate)         \
  V(ArmInt8x16Mul)                 \
  V(ArmInt8x16Min)                 \
  V(ArmInt8x16Max)                 \
  V(ArmInt8x16Equal)               \
  V(ArmInt8x16NotEqual)            \
  V(ArmInt8x16GreaterThan)         \
  V(ArmInt8x16GreaterThanOrEqual)  \
  V(ArmUint8x16ShiftRightByScalar) \
  V(ArmUint8x16AddSaturate)        \
  V(ArmUint8x16SubSaturate)        \
  V(ArmUint8x16Min)                \
  V(ArmUint8x16Max)                \
  V(ArmUint8x16GreaterThan)        \
  V(ArmUint8x16GreaterThanOrEqual) \
  V(ArmSimd128And)                 \
  V(ArmSimd128Or)                  \
  V(ArmSimd128Xor)                 \
  V(ArmSimd128Not)                 \
  V(ArmSimd32x4Select)             \
  V(ArmSimd16x8Select)             \
  V(ArmSimd8x16Select)

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

#endif  // V8_COMPILER_ARM_INSTRUCTION_CODES_ARM_H_

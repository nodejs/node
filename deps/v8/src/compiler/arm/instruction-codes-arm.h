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
  V(ArmPoke)

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

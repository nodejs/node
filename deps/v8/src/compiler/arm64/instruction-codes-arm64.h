// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_ARM64_INSTRUCTION_CODES_ARM64_H_
#define V8_COMPILER_ARM64_INSTRUCTION_CODES_ARM64_H_

namespace v8 {
namespace internal {
namespace compiler {

// ARM64-specific opcodes that specify which assembly sequence to emit.
// Most opcodes specify a single instruction.
#define TARGET_ARCH_OPCODE_LIST(V) \
  V(Arm64Add)                      \
  V(Arm64Add32)                    \
  V(Arm64And)                      \
  V(Arm64And32)                    \
  V(Arm64Bic)                      \
  V(Arm64Bic32)                    \
  V(Arm64Clz32)                    \
  V(Arm64Cmp)                      \
  V(Arm64Cmp32)                    \
  V(Arm64Cmn)                      \
  V(Arm64Cmn32)                    \
  V(Arm64Tst)                      \
  V(Arm64Tst32)                    \
  V(Arm64Or)                       \
  V(Arm64Or32)                     \
  V(Arm64Orn)                      \
  V(Arm64Orn32)                    \
  V(Arm64Eor)                      \
  V(Arm64Eor32)                    \
  V(Arm64Eon)                      \
  V(Arm64Eon32)                    \
  V(Arm64Sub)                      \
  V(Arm64Sub32)                    \
  V(Arm64Mul)                      \
  V(Arm64Mul32)                    \
  V(Arm64Smull)                    \
  V(Arm64Umull)                    \
  V(Arm64Madd)                     \
  V(Arm64Madd32)                   \
  V(Arm64Msub)                     \
  V(Arm64Msub32)                   \
  V(Arm64Mneg)                     \
  V(Arm64Mneg32)                   \
  V(Arm64Idiv)                     \
  V(Arm64Idiv32)                   \
  V(Arm64Udiv)                     \
  V(Arm64Udiv32)                   \
  V(Arm64Imod)                     \
  V(Arm64Imod32)                   \
  V(Arm64Umod)                     \
  V(Arm64Umod32)                   \
  V(Arm64Not)                      \
  V(Arm64Not32)                    \
  V(Arm64Neg)                      \
  V(Arm64Neg32)                    \
  V(Arm64Lsl)                      \
  V(Arm64Lsl32)                    \
  V(Arm64Lsr)                      \
  V(Arm64Lsr32)                    \
  V(Arm64Asr)                      \
  V(Arm64Asr32)                    \
  V(Arm64Ror)                      \
  V(Arm64Ror32)                    \
  V(Arm64Mov32)                    \
  V(Arm64Sxtb32)                   \
  V(Arm64Sxth32)                   \
  V(Arm64Sxtw)                     \
  V(Arm64Sbfx32)                   \
  V(Arm64Ubfx)                     \
  V(Arm64Ubfx32)                   \
  V(Arm64Bfi)                      \
  V(Arm64TestAndBranch32)          \
  V(Arm64TestAndBranch)            \
  V(Arm64CompareAndBranch32)       \
  V(Arm64Claim)                    \
  V(Arm64Poke)                     \
  V(Arm64PokePair)                 \
  V(Arm64Float32Cmp)               \
  V(Arm64Float32Add)               \
  V(Arm64Float32Sub)               \
  V(Arm64Float32Mul)               \
  V(Arm64Float32Div)               \
  V(Arm64Float32Max)               \
  V(Arm64Float32Min)               \
  V(Arm64Float32Abs)               \
  V(Arm64Float32Sqrt)              \
  V(Arm64Float64Cmp)               \
  V(Arm64Float64Add)               \
  V(Arm64Float64Sub)               \
  V(Arm64Float64Mul)               \
  V(Arm64Float64Div)               \
  V(Arm64Float64Mod)               \
  V(Arm64Float64Max)               \
  V(Arm64Float64Min)               \
  V(Arm64Float64Abs)               \
  V(Arm64Float64Neg)               \
  V(Arm64Float64Sqrt)              \
  V(Arm64Float64RoundDown)         \
  V(Arm64Float64RoundTiesAway)     \
  V(Arm64Float64RoundTruncate)     \
  V(Arm64Float64RoundUp)           \
  V(Arm64Float32ToFloat64)         \
  V(Arm64Float64ToFloat32)         \
  V(Arm64Float64ToInt32)           \
  V(Arm64Float64ToUint32)          \
  V(Arm64Int32ToFloat64)           \
  V(Arm64Uint32ToFloat64)          \
  V(Arm64Float64ExtractLowWord32)  \
  V(Arm64Float64ExtractHighWord32) \
  V(Arm64Float64InsertLowWord32)   \
  V(Arm64Float64InsertHighWord32)  \
  V(Arm64Float64MoveU64)           \
  V(Arm64LdrS)                     \
  V(Arm64StrS)                     \
  V(Arm64LdrD)                     \
  V(Arm64StrD)                     \
  V(Arm64Ldrb)                     \
  V(Arm64Ldrsb)                    \
  V(Arm64Strb)                     \
  V(Arm64Ldrh)                     \
  V(Arm64Ldrsh)                    \
  V(Arm64Strh)                     \
  V(Arm64LdrW)                     \
  V(Arm64StrW)                     \
  V(Arm64Ldr)                      \
  V(Arm64Str)                      \
  V(Arm64StoreWriteBarrier)


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
#define TARGET_ADDRESSING_MODE_LIST(V)                          \
  V(MRI)              /* [%r0 + K] */                           \
  V(MRR)              /* [%r0 + %r1] */                         \
  V(Operand2_R_LSL_I) /* %r0 LSL K */                           \
  V(Operand2_R_LSR_I) /* %r0 LSR K */                           \
  V(Operand2_R_ASR_I) /* %r0 ASR K */                           \
  V(Operand2_R_ROR_I) /* %r0 ROR K */                           \
  V(Operand2_R_UXTB)  /* %r0 UXTB (unsigned extend byte) */     \
  V(Operand2_R_UXTH)  /* %r0 UXTH (unsigned extend halfword) */ \
  V(Operand2_R_SXTB)  /* %r0 SXTB (signed extend byte) */       \
  V(Operand2_R_SXTH)  /* %r0 SXTH (signed extend halfword) */

}  // namespace internal
}  // namespace compiler
}  // namespace v8

#endif  // V8_COMPILER_ARM64_INSTRUCTION_CODES_ARM64_H_

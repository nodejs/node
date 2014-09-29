// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_IA32_INSTRUCTION_CODES_IA32_H_
#define V8_COMPILER_IA32_INSTRUCTION_CODES_IA32_H_

namespace v8 {
namespace internal {
namespace compiler {

// IA32-specific opcodes that specify which assembly sequence to emit.
// Most opcodes specify a single instruction.
#define TARGET_ARCH_OPCODE_LIST(V) \
  V(IA32Add)                       \
  V(IA32And)                       \
  V(IA32Cmp)                       \
  V(IA32Test)                      \
  V(IA32Or)                        \
  V(IA32Xor)                       \
  V(IA32Sub)                       \
  V(IA32Imul)                      \
  V(IA32Idiv)                      \
  V(IA32Udiv)                      \
  V(IA32Not)                       \
  V(IA32Neg)                       \
  V(IA32Shl)                       \
  V(IA32Shr)                       \
  V(IA32Sar)                       \
  V(IA32Push)                      \
  V(IA32CallCodeObject)            \
  V(IA32CallAddress)               \
  V(PopStack)                      \
  V(IA32CallJSFunction)            \
  V(SSEFloat64Cmp)                 \
  V(SSEFloat64Add)                 \
  V(SSEFloat64Sub)                 \
  V(SSEFloat64Mul)                 \
  V(SSEFloat64Div)                 \
  V(SSEFloat64Mod)                 \
  V(SSEFloat64ToInt32)             \
  V(SSEFloat64ToUint32)            \
  V(SSEInt32ToFloat64)             \
  V(SSEUint32ToFloat64)            \
  V(SSELoad)                       \
  V(SSEStore)                      \
  V(IA32LoadWord8)                 \
  V(IA32StoreWord8)                \
  V(IA32StoreWord8I)               \
  V(IA32LoadWord16)                \
  V(IA32StoreWord16)               \
  V(IA32StoreWord16I)              \
  V(IA32LoadWord32)                \
  V(IA32StoreWord32)               \
  V(IA32StoreWord32I)              \
  V(IA32StoreWriteBarrier)


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
// MR = [register]
// MI = [immediate]
// MRN = [register + register * N in {1, 2, 4, 8}]
// MRI = [register + immediate]
// MRNI = [register + register * N in {1, 2, 4, 8} + immediate]
#define TARGET_ADDRESSING_MODE_LIST(V) \
  V(MI)   /* [K] */                    \
  V(MR)   /* [%r0] */                  \
  V(MRI)  /* [%r0 + K] */              \
  V(MR1I) /* [%r0 + %r1 * 1 + K] */    \
  V(MR2I) /* [%r0 + %r1 * 2 + K] */    \
  V(MR4I) /* [%r0 + %r1 * 4 + K] */    \
  V(MR8I) /* [%r0 + %r1 * 8 + K] */

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_IA32_INSTRUCTION_CODES_IA32_H_

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
  V(Arm64Cmp)                      \
  V(Arm64Cmp32)                    \
  V(Arm64Tst)                      \
  V(Arm64Tst32)                    \
  V(Arm64Or)                       \
  V(Arm64Or32)                     \
  V(Arm64Xor)                      \
  V(Arm64Xor32)                    \
  V(Arm64Sub)                      \
  V(Arm64Sub32)                    \
  V(Arm64Mul)                      \
  V(Arm64Mul32)                    \
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
  V(Arm64Shl)                      \
  V(Arm64Shl32)                    \
  V(Arm64Shr)                      \
  V(Arm64Shr32)                    \
  V(Arm64Sar)                      \
  V(Arm64Sar32)                    \
  V(Arm64CallCodeObject)           \
  V(Arm64CallJSFunction)           \
  V(Arm64CallAddress)              \
  V(Arm64Claim)                    \
  V(Arm64Poke)                     \
  V(Arm64PokePairZero)             \
  V(Arm64PokePair)                 \
  V(Arm64Drop)                     \
  V(Arm64Float64Cmp)               \
  V(Arm64Float64Add)               \
  V(Arm64Float64Sub)               \
  V(Arm64Float64Mul)               \
  V(Arm64Float64Div)               \
  V(Arm64Float64Mod)               \
  V(Arm64Int32ToInt64)             \
  V(Arm64Int64ToInt32)             \
  V(Arm64Float64ToInt32)           \
  V(Arm64Float64ToUint32)          \
  V(Arm64Int32ToFloat64)           \
  V(Arm64Uint32ToFloat64)          \
  V(Arm64Float64Load)              \
  V(Arm64Float64Store)             \
  V(Arm64LoadWord8)                \
  V(Arm64StoreWord8)               \
  V(Arm64LoadWord16)               \
  V(Arm64StoreWord16)              \
  V(Arm64LoadWord32)               \
  V(Arm64StoreWord32)              \
  V(Arm64LoadWord64)               \
  V(Arm64StoreWord64)              \
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
#define TARGET_ADDRESSING_MODE_LIST(V) \
  V(MRI) /* [%r0 + K] */               \
  V(MRR) /* [%r0 + %r1] */

}  // namespace internal
}  // namespace compiler
}  // namespace v8

#endif  // V8_COMPILER_ARM64_INSTRUCTION_CODES_ARM64_H_

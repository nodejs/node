// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_PPC_INSTRUCTION_CODES_PPC_H_
#define V8_COMPILER_PPC_INSTRUCTION_CODES_PPC_H_

namespace v8 {
namespace internal {
namespace compiler {

// PPC-specific opcodes that specify which assembly sequence to emit.
// Most opcodes specify a single instruction.
#define TARGET_ARCH_OPCODE_LIST(V) \
  V(PPC_And32)                     \
  V(PPC_And64)                     \
  V(PPC_AndComplement32)           \
  V(PPC_AndComplement64)           \
  V(PPC_Or32)                      \
  V(PPC_Or64)                      \
  V(PPC_OrComplement32)            \
  V(PPC_OrComplement64)            \
  V(PPC_Xor32)                     \
  V(PPC_Xor64)                     \
  V(PPC_ShiftLeft32)               \
  V(PPC_ShiftLeft64)               \
  V(PPC_ShiftRight32)              \
  V(PPC_ShiftRight64)              \
  V(PPC_ShiftRightAlg32)           \
  V(PPC_ShiftRightAlg64)           \
  V(PPC_RotRight32)                \
  V(PPC_RotRight64)                \
  V(PPC_Not32)                     \
  V(PPC_Not64)                     \
  V(PPC_RotLeftAndMask32)          \
  V(PPC_RotLeftAndClear64)         \
  V(PPC_RotLeftAndClearLeft64)     \
  V(PPC_RotLeftAndClearRight64)    \
  V(PPC_Add32)                     \
  V(PPC_AddWithOverflow32)         \
  V(PPC_Add64)                     \
  V(PPC_AddFloat64)                \
  V(PPC_Sub32)                     \
  V(PPC_SubWithOverflow32)         \
  V(PPC_Sub64)                     \
  V(PPC_SubFloat64)                \
  V(PPC_Mul32)                     \
  V(PPC_Mul64)                     \
  V(PPC_MulHigh32)                 \
  V(PPC_MulHighU32)                \
  V(PPC_MulFloat64)                \
  V(PPC_Div32)                     \
  V(PPC_Div64)                     \
  V(PPC_DivU32)                    \
  V(PPC_DivU64)                    \
  V(PPC_DivFloat64)                \
  V(PPC_Mod32)                     \
  V(PPC_Mod64)                     \
  V(PPC_ModU32)                    \
  V(PPC_ModU64)                    \
  V(PPC_ModFloat64)                \
  V(PPC_Neg32)                     \
  V(PPC_Neg64)                     \
  V(PPC_NegFloat64)                \
  V(PPC_SqrtFloat64)               \
  V(PPC_FloorFloat64)              \
  V(PPC_CeilFloat64)               \
  V(PPC_TruncateFloat64)           \
  V(PPC_RoundFloat64)              \
  V(PPC_Cmp32)                     \
  V(PPC_Cmp64)                     \
  V(PPC_CmpFloat64)                \
  V(PPC_Tst32)                     \
  V(PPC_Tst64)                     \
  V(PPC_Push)                      \
  V(PPC_ExtendSignWord8)           \
  V(PPC_ExtendSignWord16)          \
  V(PPC_ExtendSignWord32)          \
  V(PPC_Uint32ToUint64)            \
  V(PPC_Int64ToInt32)              \
  V(PPC_Int32ToFloat64)            \
  V(PPC_Uint32ToFloat64)           \
  V(PPC_Float32ToFloat64)          \
  V(PPC_Float64ToInt32)            \
  V(PPC_Float64ToUint32)           \
  V(PPC_Float64ToFloat32)          \
  V(PPC_LoadWordS8)                \
  V(PPC_LoadWordU8)                \
  V(PPC_LoadWordS16)               \
  V(PPC_LoadWordU16)               \
  V(PPC_LoadWordS32)               \
  V(PPC_LoadWord64)                \
  V(PPC_LoadFloat32)               \
  V(PPC_LoadFloat64)               \
  V(PPC_StoreWord8)                \
  V(PPC_StoreWord16)               \
  V(PPC_StoreWord32)               \
  V(PPC_StoreWord64)               \
  V(PPC_StoreFloat32)              \
  V(PPC_StoreFloat64)              \
  V(PPC_StoreWriteBarrier)


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

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_PPC_INSTRUCTION_CODES_PPC_H_

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
  V(PPC_And)                       \
  V(PPC_AndComplement)             \
  V(PPC_Or)                        \
  V(PPC_OrComplement)              \
  V(PPC_Xor)                       \
  V(PPC_ShiftLeft32)               \
  V(PPC_ShiftLeft64)               \
  V(PPC_ShiftLeftPair)             \
  V(PPC_ShiftRight32)              \
  V(PPC_ShiftRight64)              \
  V(PPC_ShiftRightPair)            \
  V(PPC_ShiftRightAlg32)           \
  V(PPC_ShiftRightAlg64)           \
  V(PPC_ShiftRightAlgPair)         \
  V(PPC_RotRight32)                \
  V(PPC_RotRight64)                \
  V(PPC_Not)                       \
  V(PPC_RotLeftAndMask32)          \
  V(PPC_RotLeftAndClear64)         \
  V(PPC_RotLeftAndClearLeft64)     \
  V(PPC_RotLeftAndClearRight64)    \
  V(PPC_Add32)                     \
  V(PPC_Add64)                     \
  V(PPC_AddWithOverflow32)         \
  V(PPC_AddPair)                   \
  V(PPC_AddDouble)                 \
  V(PPC_Sub)                       \
  V(PPC_SubWithOverflow32)         \
  V(PPC_SubPair)                   \
  V(PPC_SubDouble)                 \
  V(PPC_Mul32)                     \
  V(PPC_Mul32WithHigh32)           \
  V(PPC_Mul64)                     \
  V(PPC_MulHigh32)                 \
  V(PPC_MulHighU32)                \
  V(PPC_MulPair)                   \
  V(PPC_MulDouble)                 \
  V(PPC_Div32)                     \
  V(PPC_Div64)                     \
  V(PPC_DivU32)                    \
  V(PPC_DivU64)                    \
  V(PPC_DivDouble)                 \
  V(PPC_Mod32)                     \
  V(PPC_Mod64)                     \
  V(PPC_ModU32)                    \
  V(PPC_ModU64)                    \
  V(PPC_ModDouble)                 \
  V(PPC_Neg)                       \
  V(PPC_NegDouble)                 \
  V(PPC_SqrtDouble)                \
  V(PPC_FloorDouble)               \
  V(PPC_CeilDouble)                \
  V(PPC_TruncateDouble)            \
  V(PPC_RoundDouble)               \
  V(PPC_MaxDouble)                 \
  V(PPC_MinDouble)                 \
  V(PPC_AbsDouble)                 \
  V(PPC_Cntlz32)                   \
  V(PPC_Cntlz64)                   \
  V(PPC_Popcnt32)                  \
  V(PPC_Popcnt64)                  \
  V(PPC_Cmp32)                     \
  V(PPC_Cmp64)                     \
  V(PPC_CmpDouble)                 \
  V(PPC_Tst32)                     \
  V(PPC_Tst64)                     \
  V(PPC_Push)                      \
  V(PPC_PushFrame)                 \
  V(PPC_StoreToStackSlot)          \
  V(PPC_ExtendSignWord8)           \
  V(PPC_ExtendSignWord16)          \
  V(PPC_ExtendSignWord32)          \
  V(PPC_Uint32ToUint64)            \
  V(PPC_Int64ToInt32)              \
  V(PPC_Int64ToFloat32)            \
  V(PPC_Int64ToDouble)             \
  V(PPC_Uint64ToFloat32)           \
  V(PPC_Uint64ToDouble)            \
  V(PPC_Int32ToFloat32)            \
  V(PPC_Int32ToDouble)             \
  V(PPC_Uint32ToFloat32)           \
  V(PPC_Uint32ToDouble)            \
  V(PPC_Float32ToDouble)           \
  V(PPC_Float64SilenceNaN)         \
  V(PPC_DoubleToInt32)             \
  V(PPC_DoubleToUint32)            \
  V(PPC_DoubleToInt64)             \
  V(PPC_DoubleToUint64)            \
  V(PPC_DoubleToFloat32)           \
  V(PPC_DoubleExtractLowWord32)    \
  V(PPC_DoubleExtractHighWord32)   \
  V(PPC_DoubleInsertLowWord32)     \
  V(PPC_DoubleInsertHighWord32)    \
  V(PPC_DoubleConstruct)           \
  V(PPC_BitcastInt32ToFloat32)     \
  V(PPC_BitcastFloat32ToInt32)     \
  V(PPC_BitcastInt64ToDouble)      \
  V(PPC_BitcastDoubleToInt64)      \
  V(PPC_LoadWordS8)                \
  V(PPC_LoadWordU8)                \
  V(PPC_LoadWordS16)               \
  V(PPC_LoadWordU16)               \
  V(PPC_LoadWordS32)               \
  V(PPC_LoadWordU32)               \
  V(PPC_LoadWord64)                \
  V(PPC_LoadFloat32)               \
  V(PPC_LoadDouble)                \
  V(PPC_StoreWord8)                \
  V(PPC_StoreWord16)               \
  V(PPC_StoreWord32)               \
  V(PPC_StoreWord64)               \
  V(PPC_StoreFloat32)              \
  V(PPC_StoreDouble)

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

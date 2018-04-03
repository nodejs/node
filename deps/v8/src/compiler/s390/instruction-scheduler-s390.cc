// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/instruction-scheduler.h"

namespace v8 {
namespace internal {
namespace compiler {

bool InstructionScheduler::SchedulerSupported() { return true; }

int InstructionScheduler::GetTargetInstructionFlags(
    const Instruction* instr) const {
  switch (instr->arch_opcode()) {
    case kS390_Abs32:
    case kS390_Abs64:
    case kS390_And32:
    case kS390_And64:
    case kS390_Or32:
    case kS390_Or64:
    case kS390_Xor32:
    case kS390_Xor64:
    case kS390_ShiftLeft32:
    case kS390_ShiftLeft64:
    case kS390_ShiftLeftPair:
    case kS390_ShiftRight32:
    case kS390_ShiftRight64:
    case kS390_ShiftRightPair:
    case kS390_ShiftRightArith32:
    case kS390_ShiftRightArith64:
    case kS390_ShiftRightArithPair:
    case kS390_RotRight32:
    case kS390_RotRight64:
    case kS390_Not32:
    case kS390_Not64:
    case kS390_RotLeftAndClear64:
    case kS390_RotLeftAndClearLeft64:
    case kS390_RotLeftAndClearRight64:
    case kS390_Lay:
    case kS390_Add32:
    case kS390_Add64:
    case kS390_AddPair:
    case kS390_AddFloat:
    case kS390_AddDouble:
    case kS390_Sub32:
    case kS390_Sub64:
    case kS390_SubPair:
    case kS390_MulPair:
    case kS390_SubFloat:
    case kS390_SubDouble:
    case kS390_Mul32:
    case kS390_Mul32WithOverflow:
    case kS390_Mul64:
    case kS390_MulHigh32:
    case kS390_MulHighU32:
    case kS390_MulFloat:
    case kS390_MulDouble:
    case kS390_Div32:
    case kS390_Div64:
    case kS390_DivU32:
    case kS390_DivU64:
    case kS390_DivFloat:
    case kS390_DivDouble:
    case kS390_Mod32:
    case kS390_Mod64:
    case kS390_ModU32:
    case kS390_ModU64:
    case kS390_ModDouble:
    case kS390_Neg32:
    case kS390_Neg64:
    case kS390_NegDouble:
    case kS390_NegFloat:
    case kS390_SqrtFloat:
    case kS390_FloorFloat:
    case kS390_CeilFloat:
    case kS390_TruncateFloat:
    case kS390_AbsFloat:
    case kS390_SqrtDouble:
    case kS390_FloorDouble:
    case kS390_CeilDouble:
    case kS390_TruncateDouble:
    case kS390_RoundDouble:
    case kS390_MaxFloat:
    case kS390_MaxDouble:
    case kS390_MinFloat:
    case kS390_MinDouble:
    case kS390_AbsDouble:
    case kS390_Cntlz32:
    case kS390_Cntlz64:
    case kS390_Popcnt32:
    case kS390_Popcnt64:
    case kS390_Cmp32:
    case kS390_Cmp64:
    case kS390_CmpFloat:
    case kS390_CmpDouble:
    case kS390_Tst32:
    case kS390_Tst64:
    case kS390_ExtendSignWord8:
    case kS390_ExtendSignWord16:
    case kS390_ExtendSignWord32:
    case kS390_Uint32ToUint64:
    case kS390_Int64ToInt32:
    case kS390_Int64ToFloat32:
    case kS390_Int64ToDouble:
    case kS390_Uint64ToFloat32:
    case kS390_Uint64ToDouble:
    case kS390_Int32ToFloat32:
    case kS390_Int32ToDouble:
    case kS390_Uint32ToFloat32:
    case kS390_Uint32ToDouble:
    case kS390_Float32ToInt32:
    case kS390_Float32ToUint32:
    case kS390_Float32ToUint64:
    case kS390_Float32ToDouble:
    case kS390_Float64SilenceNaN:
    case kS390_DoubleToInt32:
    case kS390_DoubleToUint32:
    case kS390_Float32ToInt64:
    case kS390_DoubleToInt64:
    case kS390_DoubleToUint64:
    case kS390_DoubleToFloat32:
    case kS390_DoubleExtractLowWord32:
    case kS390_DoubleExtractHighWord32:
    case kS390_DoubleInsertLowWord32:
    case kS390_DoubleInsertHighWord32:
    case kS390_DoubleConstruct:
    case kS390_BitcastInt32ToFloat32:
    case kS390_BitcastFloat32ToInt32:
    case kS390_BitcastInt64ToDouble:
    case kS390_BitcastDoubleToInt64:
    case kS390_LoadReverse16RR:
    case kS390_LoadReverse32RR:
    case kS390_LoadReverse64RR:
    case kS390_LoadAndTestWord32:
    case kS390_LoadAndTestWord64:
    case kS390_LoadAndTestFloat32:
    case kS390_LoadAndTestFloat64:
      return kNoOpcodeFlags;

    case kS390_LoadWordS8:
    case kS390_LoadWordU8:
    case kS390_LoadWordS16:
    case kS390_LoadWordU16:
    case kS390_LoadWordS32:
    case kS390_LoadWordU32:
    case kS390_LoadWord64:
    case kS390_LoadFloat32:
    case kS390_LoadDouble:
    case kS390_LoadReverse16:
    case kS390_LoadReverse32:
    case kS390_LoadReverse64:
      return kIsLoadOperation;

    case kS390_StoreWord8:
    case kS390_StoreWord16:
    case kS390_StoreWord32:
    case kS390_StoreWord64:
    case kS390_StoreReverse16:
    case kS390_StoreReverse32:
    case kS390_StoreReverse64:
    case kS390_StoreFloat32:
    case kS390_StoreDouble:
    case kS390_Push:
    case kS390_PushFrame:
    case kS390_StoreToStackSlot:
    case kS390_StackClaim:
      return kHasSideEffect;

#define CASE(Name) case k##Name:
      COMMON_ARCH_OPCODE_LIST(CASE)
#undef CASE
      // Already covered in architecture independent code.
      UNREACHABLE();
  }

  UNREACHABLE();
}

int InstructionScheduler::GetInstructionLatency(const Instruction* instr) {
  // TODO(all): Add instruction cost modeling.
  return 1;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

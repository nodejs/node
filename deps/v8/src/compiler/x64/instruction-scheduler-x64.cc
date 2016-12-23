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
    case kX64Add:
    case kX64Add32:
    case kX64And:
    case kX64And32:
    case kX64Cmp:
    case kX64Cmp32:
    case kX64Cmp16:
    case kX64Cmp8:
    case kX64Test:
    case kX64Test32:
    case kX64Test16:
    case kX64Test8:
    case kX64Or:
    case kX64Or32:
    case kX64Xor:
    case kX64Xor32:
    case kX64Sub:
    case kX64Sub32:
    case kX64Imul:
    case kX64Imul32:
    case kX64ImulHigh32:
    case kX64UmulHigh32:
    case kX64Not:
    case kX64Not32:
    case kX64Neg:
    case kX64Neg32:
    case kX64Shl:
    case kX64Shl32:
    case kX64Shr:
    case kX64Shr32:
    case kX64Sar:
    case kX64Sar32:
    case kX64Ror:
    case kX64Ror32:
    case kX64Lzcnt:
    case kX64Lzcnt32:
    case kX64Tzcnt:
    case kX64Tzcnt32:
    case kX64Popcnt:
    case kX64Popcnt32:
    case kSSEFloat32Cmp:
    case kSSEFloat32Add:
    case kSSEFloat32Sub:
    case kSSEFloat32Mul:
    case kSSEFloat32Div:
    case kSSEFloat32Abs:
    case kSSEFloat32Neg:
    case kSSEFloat32Sqrt:
    case kSSEFloat32Round:
    case kSSEFloat32ToFloat64:
    case kSSEFloat64Cmp:
    case kSSEFloat64Add:
    case kSSEFloat64Sub:
    case kSSEFloat64Mul:
    case kSSEFloat64Div:
    case kSSEFloat64Mod:
    case kSSEFloat64Abs:
    case kSSEFloat64Neg:
    case kSSEFloat64Sqrt:
    case kSSEFloat64Round:
    case kSSEFloat32Max:
    case kSSEFloat64Max:
    case kSSEFloat32Min:
    case kSSEFloat64Min:
    case kSSEFloat64ToFloat32:
    case kSSEFloat32ToInt32:
    case kSSEFloat32ToUint32:
    case kSSEFloat64ToInt32:
    case kSSEFloat64ToUint32:
    case kSSEFloat64ToInt64:
    case kSSEFloat32ToInt64:
    case kSSEFloat64ToUint64:
    case kSSEFloat32ToUint64:
    case kSSEInt32ToFloat64:
    case kSSEInt32ToFloat32:
    case kSSEInt64ToFloat32:
    case kSSEInt64ToFloat64:
    case kSSEUint64ToFloat32:
    case kSSEUint64ToFloat64:
    case kSSEUint32ToFloat64:
    case kSSEUint32ToFloat32:
    case kSSEFloat64ExtractLowWord32:
    case kSSEFloat64ExtractHighWord32:
    case kSSEFloat64InsertLowWord32:
    case kSSEFloat64InsertHighWord32:
    case kSSEFloat64LoadLowWord32:
    case kSSEFloat64SilenceNaN:
    case kAVXFloat32Cmp:
    case kAVXFloat32Add:
    case kAVXFloat32Sub:
    case kAVXFloat32Mul:
    case kAVXFloat32Div:
    case kAVXFloat64Cmp:
    case kAVXFloat64Add:
    case kAVXFloat64Sub:
    case kAVXFloat64Mul:
    case kAVXFloat64Div:
    case kAVXFloat64Abs:
    case kAVXFloat64Neg:
    case kAVXFloat32Abs:
    case kAVXFloat32Neg:
    case kX64BitcastFI:
    case kX64BitcastDL:
    case kX64BitcastIF:
    case kX64BitcastLD:
    case kX64Lea32:
    case kX64Lea:
    case kX64Dec32:
    case kX64Inc32:
    case kX64Int32x4Create:
    case kX64Int32x4ExtractLane:
      return (instr->addressing_mode() == kMode_None)
          ? kNoOpcodeFlags
          : kIsLoadOperation | kHasSideEffect;

    case kX64Idiv:
    case kX64Idiv32:
    case kX64Udiv:
    case kX64Udiv32:
      return (instr->addressing_mode() == kMode_None)
                 ? kMayNeedDeoptCheck
                 : kMayNeedDeoptCheck | kIsLoadOperation | kHasSideEffect;

    case kX64Movsxbl:
    case kX64Movzxbl:
    case kX64Movsxbq:
    case kX64Movzxbq:
    case kX64Movsxwl:
    case kX64Movzxwl:
    case kX64Movsxwq:
    case kX64Movzxwq:
    case kX64Movsxlq:
      DCHECK(instr->InputCount() >= 1);
      return instr->InputAt(0)->IsRegister() ? kNoOpcodeFlags
                                             : kIsLoadOperation;

    case kX64Movb:
    case kX64Movw:
      return kHasSideEffect;

    case kX64Movl:
    case kX64TrapMovl:
      if (instr->HasOutput()) {
        DCHECK(instr->InputCount() >= 1);
        return instr->InputAt(0)->IsRegister() ? kNoOpcodeFlags
                                               : kIsLoadOperation;
      } else {
        return kHasSideEffect;
      }

    case kX64Movq:
    case kX64Movsd:
    case kX64Movss:
      return instr->HasOutput() ? kIsLoadOperation : kHasSideEffect;

    case kX64StackCheck:
      return kIsLoadOperation;

    case kX64Push:
    case kX64Poke:
      return kHasSideEffect;

    case kX64Xchgb:
    case kX64Xchgw:
    case kX64Xchgl:
      return kIsLoadOperation | kHasSideEffect;

#define CASE(Name) case k##Name:
    COMMON_ARCH_OPCODE_LIST(CASE)
#undef CASE
      // Already covered in architecture independent code.
      UNREACHABLE();
  }

  UNREACHABLE();
  return kNoOpcodeFlags;
}


int InstructionScheduler::GetInstructionLatency(const Instruction* instr) {
  // TODO(all): Add instruction cost modeling.
  return 1;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

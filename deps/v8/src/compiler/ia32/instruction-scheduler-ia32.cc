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
    case kIA32Add:
    case kIA32And:
    case kIA32Cmp:
    case kIA32Test:
    case kIA32Or:
    case kIA32Xor:
    case kIA32Sub:
    case kIA32Imul:
    case kIA32ImulHigh:
    case kIA32UmulHigh:
    case kIA32Idiv:
    case kIA32Udiv:
    case kIA32Not:
    case kIA32Neg:
    case kIA32Shl:
    case kIA32Shr:
    case kIA32Sar:
    case kIA32Ror:
    case kIA32Lzcnt:
    case kIA32Tzcnt:
    case kIA32Popcnt:
    case kIA32Lea:
    case kSSEFloat32Cmp:
    case kSSEFloat32Add:
    case kSSEFloat32Sub:
    case kSSEFloat32Mul:
    case kSSEFloat32Div:
    case kSSEFloat32Max:
    case kSSEFloat32Min:
    case kSSEFloat32Abs:
    case kSSEFloat32Neg:
    case kSSEFloat32Sqrt:
    case kSSEFloat32Round:
    case kSSEFloat64Cmp:
    case kSSEFloat64Add:
    case kSSEFloat64Sub:
    case kSSEFloat64Mul:
    case kSSEFloat64Div:
    case kSSEFloat64Mod:
    case kSSEFloat64Max:
    case kSSEFloat64Min:
    case kSSEFloat64Abs:
    case kSSEFloat64Neg:
    case kSSEFloat64Sqrt:
    case kSSEFloat64Round:
    case kSSEFloat32ToFloat64:
    case kSSEFloat64ToFloat32:
    case kSSEFloat64ToInt32:
    case kSSEFloat64ToUint32:
    case kSSEInt32ToFloat64:
    case kSSEUint32ToFloat64:
    case kSSEFloat64ExtractLowWord32:
    case kSSEFloat64ExtractHighWord32:
    case kSSEFloat64InsertLowWord32:
    case kSSEFloat64InsertHighWord32:
    case kSSEFloat64LoadLowWord32:
    case kAVXFloat32Add:
    case kAVXFloat32Sub:
    case kAVXFloat32Mul:
    case kAVXFloat32Div:
    case kAVXFloat32Max:
    case kAVXFloat32Min:
    case kAVXFloat64Add:
    case kAVXFloat64Sub:
    case kAVXFloat64Mul:
    case kAVXFloat64Div:
    case kAVXFloat64Max:
    case kAVXFloat64Min:
    case kAVXFloat64Abs:
    case kAVXFloat64Neg:
    case kAVXFloat32Abs:
    case kAVXFloat32Neg:
    case kIA32BitcastFI:
    case kIA32BitcastIF:
      return (instr->addressing_mode() == kMode_None)
          ? kNoOpcodeFlags
          : kIsLoadOperation | kHasSideEffect;

    case kIA32Movsxbl:
    case kIA32Movzxbl:
    case kIA32Movb:
    case kIA32Movsxwl:
    case kIA32Movzxwl:
    case kIA32Movw:
    case kIA32Movl:
    case kIA32Movss:
    case kIA32Movsd:
      // Moves are used for memory load/store operations.
      return instr->HasOutput() ? kIsLoadOperation : kHasSideEffect;

    case kIA32StackCheck:
      return kIsLoadOperation;

    case kIA32Push:
    case kIA32PushFloat32:
    case kIA32PushFloat64:
    case kIA32Poke:
      return kHasSideEffect;

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

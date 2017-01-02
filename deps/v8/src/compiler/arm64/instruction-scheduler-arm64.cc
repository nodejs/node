// Copyright 2014 the V8 project authors. All rights reserved.
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
    case kArm64Add:
    case kArm64Add32:
    case kArm64And:
    case kArm64And32:
    case kArm64Bic:
    case kArm64Bic32:
    case kArm64Clz:
    case kArm64Clz32:
    case kArm64Cmp:
    case kArm64Cmp32:
    case kArm64Cmn:
    case kArm64Cmn32:
    case kArm64Tst:
    case kArm64Tst32:
    case kArm64Or:
    case kArm64Or32:
    case kArm64Orn:
    case kArm64Orn32:
    case kArm64Eor:
    case kArm64Eor32:
    case kArm64Eon:
    case kArm64Eon32:
    case kArm64Sub:
    case kArm64Sub32:
    case kArm64Mul:
    case kArm64Mul32:
    case kArm64Smull:
    case kArm64Umull:
    case kArm64Madd:
    case kArm64Madd32:
    case kArm64Msub:
    case kArm64Msub32:
    case kArm64Mneg:
    case kArm64Mneg32:
    case kArm64Idiv:
    case kArm64Idiv32:
    case kArm64Udiv:
    case kArm64Udiv32:
    case kArm64Imod:
    case kArm64Imod32:
    case kArm64Umod:
    case kArm64Umod32:
    case kArm64Not:
    case kArm64Not32:
    case kArm64Lsl:
    case kArm64Lsl32:
    case kArm64Lsr:
    case kArm64Lsr32:
    case kArm64Asr:
    case kArm64Asr32:
    case kArm64Ror:
    case kArm64Ror32:
    case kArm64Mov32:
    case kArm64Sxtb32:
    case kArm64Sxth32:
    case kArm64Sxtw:
    case kArm64Sbfx32:
    case kArm64Ubfx:
    case kArm64Ubfx32:
    case kArm64Ubfiz32:
    case kArm64Bfi:
    case kArm64Rbit:
    case kArm64Rbit32:
    case kArm64Float32Cmp:
    case kArm64Float32Add:
    case kArm64Float32Sub:
    case kArm64Float32Mul:
    case kArm64Float32Div:
    case kArm64Float32Abs:
    case kArm64Float32Neg:
    case kArm64Float32Sqrt:
    case kArm64Float32RoundDown:
    case kArm64Float32Max:
    case kArm64Float32Min:
    case kArm64Float64Cmp:
    case kArm64Float64Add:
    case kArm64Float64Sub:
    case kArm64Float64Mul:
    case kArm64Float64Div:
    case kArm64Float64Mod:
    case kArm64Float64Max:
    case kArm64Float64Min:
    case kArm64Float64Abs:
    case kArm64Float64Neg:
    case kArm64Float64Sqrt:
    case kArm64Float64RoundDown:
    case kArm64Float64RoundTiesAway:
    case kArm64Float64RoundTruncate:
    case kArm64Float64RoundTiesEven:
    case kArm64Float64RoundUp:
    case kArm64Float32RoundTiesEven:
    case kArm64Float32RoundTruncate:
    case kArm64Float32RoundUp:
    case kArm64Float32ToFloat64:
    case kArm64Float64ToFloat32:
    case kArm64Float32ToInt32:
    case kArm64Float64ToInt32:
    case kArm64Float32ToUint32:
    case kArm64Float64ToUint32:
    case kArm64Float32ToInt64:
    case kArm64Float64ToInt64:
    case kArm64Float32ToUint64:
    case kArm64Float64ToUint64:
    case kArm64Int32ToFloat32:
    case kArm64Int32ToFloat64:
    case kArm64Int64ToFloat32:
    case kArm64Int64ToFloat64:
    case kArm64Uint32ToFloat32:
    case kArm64Uint32ToFloat64:
    case kArm64Uint64ToFloat32:
    case kArm64Uint64ToFloat64:
    case kArm64Float64ExtractLowWord32:
    case kArm64Float64ExtractHighWord32:
    case kArm64Float64InsertLowWord32:
    case kArm64Float64InsertHighWord32:
    case kArm64Float64MoveU64:
    case kArm64U64MoveFloat64:
    case kArm64Float64SilenceNaN:
      return kNoOpcodeFlags;

    case kArm64TestAndBranch32:
    case kArm64TestAndBranch:
    case kArm64CompareAndBranch32:
    case kArm64CompareAndBranch:
      return kIsBlockTerminator;

    case kArm64LdrS:
    case kArm64LdrD:
    case kArm64Ldrb:
    case kArm64Ldrsb:
    case kArm64Ldrh:
    case kArm64Ldrsh:
    case kArm64Ldrsw:
    case kArm64LdrW:
    case kArm64Ldr:
      return kIsLoadOperation;

    case kArm64ClaimCSP:
    case kArm64ClaimJSSP:
    case kArm64PokeCSP:
    case kArm64PokeJSSP:
    case kArm64PokePair:
    case kArm64StrS:
    case kArm64StrD:
    case kArm64Strb:
    case kArm64Strh:
    case kArm64StrW:
    case kArm64Str:
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
  // Basic latency modeling for arm64 instructions. They have been determined
  // in an empirical way.
  switch (instr->arch_opcode()) {
    case kArm64Add:
    case kArm64Add32:
    case kArm64And:
    case kArm64And32:
    case kArm64Bic:
    case kArm64Bic32:
    case kArm64Cmn:
    case kArm64Cmn32:
    case kArm64Cmp:
    case kArm64Cmp32:
    case kArm64Eon:
    case kArm64Eon32:
    case kArm64Eor:
    case kArm64Eor32:
    case kArm64Not:
    case kArm64Not32:
    case kArm64Or:
    case kArm64Or32:
    case kArm64Orn:
    case kArm64Orn32:
    case kArm64Sub:
    case kArm64Sub32:
    case kArm64Tst:
    case kArm64Tst32:
      if (instr->addressing_mode() != kMode_None) {
        return 3;
      } else {
        return 1;
      }

    case kArm64Clz:
    case kArm64Clz32:
    case kArm64Sbfx32:
    case kArm64Sxtb32:
    case kArm64Sxth32:
    case kArm64Sxtw:
    case kArm64Ubfiz32:
    case kArm64Ubfx:
    case kArm64Ubfx32:
      return 1;

    case kArm64Lsl:
    case kArm64Lsl32:
    case kArm64Lsr:
    case kArm64Lsr32:
    case kArm64Asr:
    case kArm64Asr32:
    case kArm64Ror:
    case kArm64Ror32:
      return 1;

    case kArm64Ldr:
    case kArm64LdrD:
    case kArm64LdrS:
    case kArm64LdrW:
    case kArm64Ldrb:
    case kArm64Ldrh:
    case kArm64Ldrsb:
    case kArm64Ldrsh:
    case kArm64Ldrsw:
      return 11;

    case kCheckedLoadInt8:
    case kCheckedLoadUint8:
    case kCheckedLoadInt16:
    case kCheckedLoadUint16:
    case kCheckedLoadWord32:
    case kCheckedLoadWord64:
    case kCheckedLoadFloat32:
    case kCheckedLoadFloat64:
      return 5;

    case kArm64Str:
    case kArm64StrD:
    case kArm64StrS:
    case kArm64StrW:
    case kArm64Strb:
    case kArm64Strh:
      return 1;

    case kCheckedStoreWord8:
    case kCheckedStoreWord16:
    case kCheckedStoreWord32:
    case kCheckedStoreWord64:
    case kCheckedStoreFloat32:
    case kCheckedStoreFloat64:
      return 1;

    case kArm64Madd32:
    case kArm64Mneg32:
    case kArm64Msub32:
    case kArm64Mul32:
      return 3;

    case kArm64Madd:
    case kArm64Mneg:
    case kArm64Msub:
    case kArm64Mul:
      return 5;

    case kArm64Idiv32:
    case kArm64Udiv32:
      return 12;

    case kArm64Idiv:
    case kArm64Udiv:
      return 20;

    case kArm64Float32Add:
    case kArm64Float32Sub:
    case kArm64Float64Add:
    case kArm64Float64Sub:
      return 5;

    case kArm64Float32Abs:
    case kArm64Float32Cmp:
    case kArm64Float32Neg:
    case kArm64Float64Abs:
    case kArm64Float64Cmp:
    case kArm64Float64Neg:
      return 3;

    case kArm64Float32Div:
    case kArm64Float32Sqrt:
      return 12;

    case kArm64Float64Div:
    case kArm64Float64Sqrt:
      return 19;

    case kArm64Float32RoundDown:
    case kArm64Float32RoundTiesEven:
    case kArm64Float32RoundTruncate:
    case kArm64Float32RoundUp:
    case kArm64Float64RoundDown:
    case kArm64Float64RoundTiesAway:
    case kArm64Float64RoundTiesEven:
    case kArm64Float64RoundTruncate:
    case kArm64Float64RoundUp:
      return 5;

    case kArm64Float32ToFloat64:
    case kArm64Float64ToFloat32:
    case kArm64Float64ToInt32:
    case kArm64Float64ToUint32:
    case kArm64Float32ToInt64:
    case kArm64Float64ToInt64:
    case kArm64Float32ToUint64:
    case kArm64Float64ToUint64:
    case kArm64Int32ToFloat64:
    case kArm64Int64ToFloat32:
    case kArm64Int64ToFloat64:
    case kArm64Uint32ToFloat64:
    case kArm64Uint64ToFloat32:
    case kArm64Uint64ToFloat64:
      return 5;

    default:
      return 2;
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

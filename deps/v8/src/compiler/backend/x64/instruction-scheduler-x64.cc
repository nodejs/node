// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/instruction-scheduler.h"

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
    case kX64Bswap:
    case kX64Bswap32:
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
    case kX64F64x2Splat:
    case kX64F64x2ExtractLane:
    case kX64F64x2ReplaceLane:
    case kX64F64x2Abs:
    case kX64F64x2Neg:
    case kX64F64x2Sqrt:
    case kX64F64x2Add:
    case kX64F64x2Sub:
    case kX64F64x2Mul:
    case kX64F64x2Div:
    case kX64F64x2Min:
    case kX64F64x2Max:
    case kX64F64x2Eq:
    case kX64F64x2Ne:
    case kX64F64x2Lt:
    case kX64F64x2Le:
    case kX64F64x2Qfma:
    case kX64F64x2Qfms:
    case kX64F32x4Splat:
    case kX64F32x4ExtractLane:
    case kX64F32x4ReplaceLane:
    case kX64F32x4SConvertI32x4:
    case kX64F32x4UConvertI32x4:
    case kX64F32x4RecipApprox:
    case kX64F32x4RecipSqrtApprox:
    case kX64F32x4Abs:
    case kX64F32x4Neg:
    case kX64F32x4Sqrt:
    case kX64F32x4Add:
    case kX64F32x4AddHoriz:
    case kX64F32x4Sub:
    case kX64F32x4Mul:
    case kX64F32x4Div:
    case kX64F32x4Min:
    case kX64F32x4Max:
    case kX64F32x4Eq:
    case kX64F32x4Ne:
    case kX64F32x4Lt:
    case kX64F32x4Le:
    case kX64F32x4Qfma:
    case kX64F32x4Qfms:
    case kX64I64x2Splat:
    case kX64I64x2ExtractLane:
    case kX64I64x2ReplaceLane:
    case kX64I64x2Neg:
    case kX64I64x2Shl:
    case kX64I64x2ShrS:
    case kX64I64x2Add:
    case kX64I64x2Sub:
    case kX64I64x2Mul:
    case kX64I64x2MinS:
    case kX64I64x2MaxS:
    case kX64I64x2Eq:
    case kX64I64x2Ne:
    case kX64I64x2GtS:
    case kX64I64x2GeS:
    case kX64I64x2ShrU:
    case kX64I64x2MinU:
    case kX64I64x2MaxU:
    case kX64I64x2GtU:
    case kX64I64x2GeU:
    case kX64I32x4Splat:
    case kX64I32x4ExtractLane:
    case kX64I32x4ReplaceLane:
    case kX64I32x4SConvertF32x4:
    case kX64I32x4SConvertI16x8Low:
    case kX64I32x4SConvertI16x8High:
    case kX64I32x4Neg:
    case kX64I32x4Shl:
    case kX64I32x4ShrS:
    case kX64I32x4Add:
    case kX64I32x4AddHoriz:
    case kX64I32x4Sub:
    case kX64I32x4Mul:
    case kX64I32x4MinS:
    case kX64I32x4MaxS:
    case kX64I32x4Eq:
    case kX64I32x4Ne:
    case kX64I32x4GtS:
    case kX64I32x4GeS:
    case kX64I32x4UConvertF32x4:
    case kX64I32x4UConvertI16x8Low:
    case kX64I32x4UConvertI16x8High:
    case kX64I32x4ShrU:
    case kX64I32x4MinU:
    case kX64I32x4MaxU:
    case kX64I32x4GtU:
    case kX64I32x4GeU:
    case kX64I16x8Splat:
    case kX64I16x8ExtractLaneU:
    case kX64I16x8ExtractLaneS:
    case kX64I16x8ReplaceLane:
    case kX64I16x8SConvertI8x16Low:
    case kX64I16x8SConvertI8x16High:
    case kX64I16x8Neg:
    case kX64I16x8Shl:
    case kX64I16x8ShrS:
    case kX64I16x8SConvertI32x4:
    case kX64I16x8Add:
    case kX64I16x8AddSaturateS:
    case kX64I16x8AddHoriz:
    case kX64I16x8Sub:
    case kX64I16x8SubSaturateS:
    case kX64I16x8Mul:
    case kX64I16x8MinS:
    case kX64I16x8MaxS:
    case kX64I16x8Eq:
    case kX64I16x8Ne:
    case kX64I16x8GtS:
    case kX64I16x8GeS:
    case kX64I16x8UConvertI8x16Low:
    case kX64I16x8UConvertI8x16High:
    case kX64I16x8UConvertI32x4:
    case kX64I16x8ShrU:
    case kX64I16x8AddSaturateU:
    case kX64I16x8SubSaturateU:
    case kX64I16x8MinU:
    case kX64I16x8MaxU:
    case kX64I16x8GtU:
    case kX64I16x8GeU:
    case kX64I16x8RoundingAverageU:
    case kX64I8x16Splat:
    case kX64I8x16ExtractLaneU:
    case kX64I8x16ExtractLaneS:
    case kX64I8x16ReplaceLane:
    case kX64I8x16SConvertI16x8:
    case kX64I8x16Neg:
    case kX64I8x16Shl:
    case kX64I8x16ShrS:
    case kX64I8x16Add:
    case kX64I8x16AddSaturateS:
    case kX64I8x16Sub:
    case kX64I8x16SubSaturateS:
    case kX64I8x16Mul:
    case kX64I8x16MinS:
    case kX64I8x16MaxS:
    case kX64I8x16Eq:
    case kX64I8x16Ne:
    case kX64I8x16GtS:
    case kX64I8x16GeS:
    case kX64I8x16UConvertI16x8:
    case kX64I8x16AddSaturateU:
    case kX64I8x16SubSaturateU:
    case kX64I8x16ShrU:
    case kX64I8x16MinU:
    case kX64I8x16MaxU:
    case kX64I8x16GtU:
    case kX64I8x16GeU:
    case kX64I8x16RoundingAverageU:
    case kX64S128And:
    case kX64S128Or:
    case kX64S128Xor:
    case kX64S128Not:
    case kX64S128Select:
    case kX64S128Zero:
    case kX64S128AndNot:
    case kX64S1x2AnyTrue:
    case kX64S1x2AllTrue:
    case kX64S1x4AnyTrue:
    case kX64S1x4AllTrue:
    case kX64S1x8AnyTrue:
    case kX64S1x8AllTrue:
    case kX64S8x16Swizzle:
    case kX64S8x16Shuffle:
    case kX64S32x4Swizzle:
    case kX64S32x4Shuffle:
    case kX64S16x8Blend:
    case kX64S16x8HalfShuffle1:
    case kX64S16x8HalfShuffle2:
    case kX64S8x16Alignr:
    case kX64S16x8Dup:
    case kX64S8x16Dup:
    case kX64S16x8UnzipHigh:
    case kX64S16x8UnzipLow:
    case kX64S8x16UnzipHigh:
    case kX64S8x16UnzipLow:
    case kX64S64x2UnpackHigh:
    case kX64S32x4UnpackHigh:
    case kX64S16x8UnpackHigh:
    case kX64S8x16UnpackHigh:
    case kX64S64x2UnpackLow:
    case kX64S32x4UnpackLow:
    case kX64S16x8UnpackLow:
    case kX64S8x16UnpackLow:
    case kX64S8x16TransposeLow:
    case kX64S8x16TransposeHigh:
    case kX64S8x8Reverse:
    case kX64S8x4Reverse:
    case kX64S8x2Reverse:
    case kX64S1x16AnyTrue:
    case kX64S1x16AllTrue:
      return (instr->addressing_mode() == kMode_None)
                 ? kNoOpcodeFlags
                 : kIsLoadOperation | kHasSideEffect;

    case kX64Idiv:
    case kX64Idiv32:
    case kX64Udiv:
    case kX64Udiv32:
      return (instr->addressing_mode() == kMode_None)
                 ? kMayNeedDeoptOrTrapCheck
                 : kMayNeedDeoptOrTrapCheck | kIsLoadOperation | kHasSideEffect;

    case kX64Movsxbl:
    case kX64Movzxbl:
    case kX64Movsxbq:
    case kX64Movzxbq:
    case kX64Movsxwl:
    case kX64Movzxwl:
    case kX64Movsxwq:
    case kX64Movzxwq:
    case kX64Movsxlq:
      DCHECK_LE(1, instr->InputCount());
      return instr->InputAt(0)->IsRegister() ? kNoOpcodeFlags
                                             : kIsLoadOperation;

    case kX64Movb:
    case kX64Movw:
      return kHasSideEffect;

    case kX64Movl:
      if (instr->HasOutput()) {
        DCHECK_LE(1, instr->InputCount());
        return instr->InputAt(0)->IsRegister() ? kNoOpcodeFlags
                                               : kIsLoadOperation;
      } else {
        return kHasSideEffect;
      }

    case kX64MovqDecompressTaggedSigned:
    case kX64MovqDecompressTaggedPointer:
    case kX64MovqDecompressAnyTagged:
    case kX64MovqCompressTagged:
    case kX64Movq:
    case kX64Movsd:
    case kX64Movss:
    case kX64Movdqu:
    case kX64S8x16LoadSplat:
    case kX64S16x8LoadSplat:
    case kX64S32x4LoadSplat:
    case kX64S64x2LoadSplat:
    case kX64I16x8Load8x8S:
    case kX64I16x8Load8x8U:
    case kX64I32x4Load16x4S:
    case kX64I32x4Load16x4U:
    case kX64I64x2Load32x2S:
    case kX64I64x2Load32x2U:
      return instr->HasOutput() ? kIsLoadOperation : kHasSideEffect;

    case kX64Peek:
      return kIsLoadOperation;

    case kX64Push:
    case kX64Poke:
      return kHasSideEffect;

    case kX64MFence:
    case kX64LFence:
      return kHasSideEffect;

    case kX64Word64AtomicLoadUint8:
    case kX64Word64AtomicLoadUint16:
    case kX64Word64AtomicLoadUint32:
    case kX64Word64AtomicLoadUint64:
      return kIsLoadOperation;

    case kX64Word64AtomicStoreWord8:
    case kX64Word64AtomicStoreWord16:
    case kX64Word64AtomicStoreWord32:
    case kX64Word64AtomicStoreWord64:
    case kX64Word64AtomicAddUint8:
    case kX64Word64AtomicAddUint16:
    case kX64Word64AtomicAddUint32:
    case kX64Word64AtomicAddUint64:
    case kX64Word64AtomicSubUint8:
    case kX64Word64AtomicSubUint16:
    case kX64Word64AtomicSubUint32:
    case kX64Word64AtomicSubUint64:
    case kX64Word64AtomicAndUint8:
    case kX64Word64AtomicAndUint16:
    case kX64Word64AtomicAndUint32:
    case kX64Word64AtomicAndUint64:
    case kX64Word64AtomicOrUint8:
    case kX64Word64AtomicOrUint16:
    case kX64Word64AtomicOrUint32:
    case kX64Word64AtomicOrUint64:
    case kX64Word64AtomicXorUint8:
    case kX64Word64AtomicXorUint16:
    case kX64Word64AtomicXorUint32:
    case kX64Word64AtomicXorUint64:
    case kX64Word64AtomicExchangeUint8:
    case kX64Word64AtomicExchangeUint16:
    case kX64Word64AtomicExchangeUint32:
    case kX64Word64AtomicExchangeUint64:
    case kX64Word64AtomicCompareExchangeUint8:
    case kX64Word64AtomicCompareExchangeUint16:
    case kX64Word64AtomicCompareExchangeUint32:
    case kX64Word64AtomicCompareExchangeUint64:
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
  // Basic latency modeling for x64 instructions. They have been determined
  // in an empirical way.
  switch (instr->arch_opcode()) {
    case kSSEFloat64Mul:
      return 5;
    case kX64Imul:
    case kX64Imul32:
    case kX64ImulHigh32:
    case kX64UmulHigh32:
    case kSSEFloat32Cmp:
    case kSSEFloat32Add:
    case kSSEFloat32Sub:
    case kSSEFloat32Abs:
    case kSSEFloat32Neg:
    case kSSEFloat64Cmp:
    case kSSEFloat64Add:
    case kSSEFloat64Sub:
    case kSSEFloat64Max:
    case kSSEFloat64Min:
    case kSSEFloat64Abs:
    case kSSEFloat64Neg:
      return 3;
    case kSSEFloat32Mul:
    case kSSEFloat32ToFloat64:
    case kSSEFloat64ToFloat32:
    case kSSEFloat32Round:
    case kSSEFloat64Round:
    case kSSEFloat32ToInt32:
    case kSSEFloat32ToUint32:
    case kSSEFloat64ToInt32:
    case kSSEFloat64ToUint32:
      return 4;
    case kX64Idiv:
      return 49;
    case kX64Idiv32:
      return 35;
    case kX64Udiv:
      return 38;
    case kX64Udiv32:
      return 26;
    case kSSEFloat32Div:
    case kSSEFloat64Div:
    case kSSEFloat32Sqrt:
    case kSSEFloat64Sqrt:
      return 13;
    case kSSEFloat32ToInt64:
    case kSSEFloat64ToInt64:
    case kSSEFloat32ToUint64:
    case kSSEFloat64ToUint64:
      return 10;
    case kSSEFloat64Mod:
      return 50;
    case kArchTruncateDoubleToI:
      return 6;
    default:
      return 1;
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

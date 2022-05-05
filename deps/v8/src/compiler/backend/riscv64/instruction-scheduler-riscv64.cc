// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/macro-assembler.h"
#include "src/compiler/backend/instruction-scheduler.h"

namespace v8 {
namespace internal {
namespace compiler {

bool InstructionScheduler::SchedulerSupported() { return true; }

int InstructionScheduler::GetTargetInstructionFlags(
    const Instruction* instr) const {
  switch (instr->arch_opcode()) {
    case kRiscvAbsD:
    case kRiscvAbsS:
    case kRiscvAdd32:
    case kRiscvAddD:
    case kRiscvAddS:
    case kRiscvAnd:
    case kRiscvAnd32:
    case kRiscvAssertEqual:
    case kRiscvBitcastDL:
    case kRiscvBitcastLD:
    case kRiscvBitcastInt32ToFloat32:
    case kRiscvBitcastFloat32ToInt32:
    case kRiscvByteSwap32:
    case kRiscvByteSwap64:
    case kRiscvCeilWD:
    case kRiscvCeilWS:
    case kRiscvClz32:
    case kRiscvCmp:
    case kRiscvCmpZero:
    case kRiscvCmpD:
    case kRiscvCmpS:
    case kRiscvCtz32:
    case kRiscvCvtDL:
    case kRiscvCvtDS:
    case kRiscvCvtDUl:
    case kRiscvCvtDUw:
    case kRiscvCvtDW:
    case kRiscvCvtSD:
    case kRiscvCvtSL:
    case kRiscvCvtSUl:
    case kRiscvCvtSUw:
    case kRiscvCvtSW:
    case kRiscvMulHigh64:
    case kRiscvMulHighU32:
    case kRiscvAdd64:
    case kRiscvAddOvf64:
    case kRiscvClz64:
    case kRiscvCtz64:
    case kRiscvDiv64:
    case kRiscvDivU64:
    case kRiscvZeroExtendWord:
    case kRiscvSignExtendWord:
    case kRiscvDiv32:
    case kRiscvDivD:
    case kRiscvDivS:
    case kRiscvDivU32:
    case kRiscvMod64:
    case kRiscvModU64:
    case kRiscvMul64:
    case kRiscvPopcnt64:
    case kRiscvRor64:
    case kRiscvSar64:
    case kRiscvShl64:
    case kRiscvShr64:
    case kRiscvSub64:
    case kRiscvSubOvf64:
    case kRiscvF64x2Abs:
    case kRiscvF64x2Neg:
    case kRiscvF64x2Sqrt:
    case kRiscvF64x2Add:
    case kRiscvF64x2Sub:
    case kRiscvF64x2Mul:
    case kRiscvF64x2Div:
    case kRiscvF64x2Min:
    case kRiscvF64x2Max:
    case kRiscvF64x2Eq:
    case kRiscvF64x2Ne:
    case kRiscvF64x2Lt:
    case kRiscvF64x2Le:
    case kRiscvF64x2Pmin:
    case kRiscvF64x2Pmax:
    case kRiscvF64x2ConvertLowI32x4S:
    case kRiscvF64x2ConvertLowI32x4U:
    case kRiscvF64x2PromoteLowF32x4:
    case kRiscvF64x2Ceil:
    case kRiscvF64x2Floor:
    case kRiscvF64x2Trunc:
    case kRiscvF64x2NearestInt:
    case kRiscvI64x2Splat:
    case kRiscvI64x2ExtractLane:
    case kRiscvI64x2ReplaceLane:
    case kRiscvI64x2Add:
    case kRiscvI64x2Sub:
    case kRiscvI64x2Mul:
    case kRiscvI64x2Neg:
    case kRiscvI64x2Abs:
    case kRiscvI64x2Shl:
    case kRiscvI64x2ShrS:
    case kRiscvI64x2ShrU:
    case kRiscvI64x2BitMask:
    case kRiscvI64x2GtS:
    case kRiscvI64x2GeS:
    case kRiscvF32x4Abs:
    case kRiscvF32x4Add:
    case kRiscvF32x4Eq:
    case kRiscvF32x4ExtractLane:
    case kRiscvF32x4Lt:
    case kRiscvF32x4Le:
    case kRiscvF32x4Max:
    case kRiscvF32x4Min:
    case kRiscvF32x4Mul:
    case kRiscvF32x4Div:
    case kRiscvF32x4Ne:
    case kRiscvF32x4Neg:
    case kRiscvF32x4Sqrt:
    case kRiscvF32x4RecipApprox:
    case kRiscvF32x4RecipSqrtApprox:
    case kRiscvF64x2Qfma:
    case kRiscvF64x2Qfms:
    case kRiscvF32x4Qfma:
    case kRiscvF32x4Qfms:
    case kRiscvF32x4ReplaceLane:
    case kRiscvF32x4SConvertI32x4:
    case kRiscvF32x4Splat:
    case kRiscvF32x4Sub:
    case kRiscvF32x4UConvertI32x4:
    case kRiscvF32x4Pmin:
    case kRiscvF32x4Pmax:
    case kRiscvF32x4DemoteF64x2Zero:
    case kRiscvF32x4Ceil:
    case kRiscvF32x4Floor:
    case kRiscvF32x4Trunc:
    case kRiscvF32x4NearestInt:
    case kRiscvI64x2Eq:
    case kRiscvI64x2Ne:
    case kRiscvF64x2Splat:
    case kRiscvF64x2ExtractLane:
    case kRiscvF64x2ReplaceLane:
    case kRiscvFloat32Max:
    case kRiscvFloat32Min:
    case kRiscvFloat32RoundDown:
    case kRiscvFloat32RoundTiesEven:
    case kRiscvFloat32RoundTruncate:
    case kRiscvFloat32RoundUp:
    case kRiscvFloat64ExtractLowWord32:
    case kRiscvFloat64ExtractHighWord32:
    case kRiscvFloat64InsertLowWord32:
    case kRiscvFloat64InsertHighWord32:
    case kRiscvFloat64Max:
    case kRiscvFloat64Min:
    case kRiscvFloat64RoundDown:
    case kRiscvFloat64RoundTiesEven:
    case kRiscvFloat64RoundTruncate:
    case kRiscvFloat64RoundUp:
    case kRiscvFloat64SilenceNaN:
    case kRiscvFloorWD:
    case kRiscvFloorWS:
    case kRiscvI64x2SConvertI32x4Low:
    case kRiscvI64x2SConvertI32x4High:
    case kRiscvI64x2UConvertI32x4Low:
    case kRiscvI64x2UConvertI32x4High:
    case kRiscvI16x8Add:
    case kRiscvI16x8AddSatS:
    case kRiscvI16x8AddSatU:
    case kRiscvI16x8Eq:
    case kRiscvI16x8ExtractLaneU:
    case kRiscvI16x8ExtractLaneS:
    case kRiscvI16x8GeS:
    case kRiscvI16x8GeU:
    case kRiscvI16x8GtS:
    case kRiscvI16x8GtU:
    case kRiscvI16x8MaxS:
    case kRiscvI16x8MaxU:
    case kRiscvI16x8MinS:
    case kRiscvI16x8MinU:
    case kRiscvI16x8Mul:
    case kRiscvI16x8Ne:
    case kRiscvI16x8Neg:
    case kRiscvI16x8ReplaceLane:
    case kRiscvI8x16SConvertI16x8:
    case kRiscvI16x8SConvertI32x4:
    case kRiscvI16x8SConvertI8x16High:
    case kRiscvI16x8SConvertI8x16Low:
    case kRiscvI16x8Shl:
    case kRiscvI16x8ShrS:
    case kRiscvI16x8ShrU:
    case kRiscvI32x4TruncSatF64x2SZero:
    case kRiscvI32x4TruncSatF64x2UZero:
    case kRiscvI16x8Splat:
    case kRiscvI16x8Sub:
    case kRiscvI16x8SubSatS:
    case kRiscvI16x8SubSatU:
    case kRiscvI8x16UConvertI16x8:
    case kRiscvI16x8UConvertI32x4:
    case kRiscvI16x8UConvertI8x16High:
    case kRiscvI16x8UConvertI8x16Low:
    case kRiscvI16x8RoundingAverageU:
    case kRiscvI16x8Q15MulRSatS:
    case kRiscvI16x8Abs:
    case kRiscvI16x8BitMask:
    case kRiscvI32x4Add:
    case kRiscvI32x4Eq:
    case kRiscvI32x4ExtractLane:
    case kRiscvI32x4GeS:
    case kRiscvI32x4GeU:
    case kRiscvI32x4GtS:
    case kRiscvI32x4GtU:
    case kRiscvI32x4MaxS:
    case kRiscvI32x4MaxU:
    case kRiscvI32x4MinS:
    case kRiscvI32x4MinU:
    case kRiscvI32x4Mul:
    case kRiscvI32x4Ne:
    case kRiscvI32x4Neg:
    case kRiscvI32x4ReplaceLane:
    case kRiscvI32x4SConvertF32x4:
    case kRiscvI32x4SConvertI16x8High:
    case kRiscvI32x4SConvertI16x8Low:
    case kRiscvI32x4Shl:
    case kRiscvI32x4ShrS:
    case kRiscvI32x4ShrU:
    case kRiscvI32x4Splat:
    case kRiscvI32x4Sub:
    case kRiscvI32x4UConvertF32x4:
    case kRiscvI32x4UConvertI16x8High:
    case kRiscvI32x4UConvertI16x8Low:
    case kRiscvI32x4Abs:
    case kRiscvI32x4BitMask:
    case kRiscvI8x16Add:
    case kRiscvI8x16AddSatS:
    case kRiscvI8x16AddSatU:
    case kRiscvI8x16Eq:
    case kRiscvI8x16ExtractLaneU:
    case kRiscvI8x16ExtractLaneS:
    case kRiscvI8x16GeS:
    case kRiscvI8x16GeU:
    case kRiscvI8x16GtS:
    case kRiscvI8x16GtU:
    case kRiscvI8x16MaxS:
    case kRiscvI8x16MaxU:
    case kRiscvI8x16MinS:
    case kRiscvI8x16MinU:
    case kRiscvI8x16Ne:
    case kRiscvI8x16Neg:
    case kRiscvI8x16ReplaceLane:
    case kRiscvI8x16Shl:
    case kRiscvI8x16ShrS:
    case kRiscvI8x16ShrU:
    case kRiscvI8x16Splat:
    case kRiscvI8x16Sub:
    case kRiscvI8x16SubSatS:
    case kRiscvI8x16SubSatU:
    case kRiscvI8x16RoundingAverageU:
    case kRiscvI8x16Abs:
    case kRiscvI8x16BitMask:
    case kRiscvI8x16Popcnt:
    case kRiscvMaxD:
    case kRiscvMaxS:
    case kRiscvMinD:
    case kRiscvMinS:
    case kRiscvMod32:
    case kRiscvModU32:
    case kRiscvMov:
    case kRiscvMul32:
    case kRiscvMulD:
    case kRiscvMulHigh32:
    case kRiscvMulOvf32:
    case kRiscvMulS:
    case kRiscvNegD:
    case kRiscvNegS:
    case kRiscvNor:
    case kRiscvNor32:
    case kRiscvOr:
    case kRiscvOr32:
    case kRiscvPopcnt32:
    case kRiscvRor32:
    case kRiscvRoundWD:
    case kRiscvRoundWS:
    case kRiscvS128And:
    case kRiscvS128Or:
    case kRiscvS128Not:
    case kRiscvS128Select:
    case kRiscvS128AndNot:
    case kRiscvS128Xor:
    case kRiscvS128Const:
    case kRiscvS128Zero:
    case kRiscvS128Load32Zero:
    case kRiscvS128Load64Zero:
    case kRiscvS128AllOnes:
    case kRiscvS16x8InterleaveEven:
    case kRiscvS16x8InterleaveOdd:
    case kRiscvS16x8InterleaveLeft:
    case kRiscvS16x8InterleaveRight:
    case kRiscvS16x8PackEven:
    case kRiscvS16x8PackOdd:
    case kRiscvS16x2Reverse:
    case kRiscvS16x4Reverse:
    case kRiscvI8x16AllTrue:
    case kRiscvI32x4AllTrue:
    case kRiscvI16x8AllTrue:
    case kRiscvV128AnyTrue:
    case kRiscvI64x2AllTrue:
    case kRiscvS32x4InterleaveEven:
    case kRiscvS32x4InterleaveOdd:
    case kRiscvS32x4InterleaveLeft:
    case kRiscvS32x4InterleaveRight:
    case kRiscvS32x4PackEven:
    case kRiscvS32x4PackOdd:
    case kRiscvS32x4Shuffle:
    case kRiscvS8x16Concat:
    case kRiscvS8x16InterleaveEven:
    case kRiscvS8x16InterleaveOdd:
    case kRiscvS8x16InterleaveLeft:
    case kRiscvS8x16InterleaveRight:
    case kRiscvS8x16PackEven:
    case kRiscvS8x16PackOdd:
    case kRiscvS8x2Reverse:
    case kRiscvS8x4Reverse:
    case kRiscvS8x8Reverse:
    case kRiscvI8x16Shuffle:
    case kRiscvVwmul:
    case kRiscvVwmulu:
    case kRiscvVmvSx:
    case kRiscvVcompress:
    case kRiscvVaddVv:
    case kRiscvVwadd:
    case kRiscvVwaddu:
    case kRiscvVrgather:
    case kRiscvVslidedown:
    case kRiscvSar32:
    case kRiscvSignExtendByte:
    case kRiscvSignExtendShort:
    case kRiscvShl32:
    case kRiscvShr32:
    case kRiscvSqrtD:
    case kRiscvSqrtS:
    case kRiscvSub32:
    case kRiscvSubD:
    case kRiscvSubS:
    case kRiscvTruncLD:
    case kRiscvTruncLS:
    case kRiscvTruncUlD:
    case kRiscvTruncUlS:
    case kRiscvTruncUwD:
    case kRiscvTruncUwS:
    case kRiscvTruncWD:
    case kRiscvTruncWS:
    case kRiscvTst:
    case kRiscvXor:
    case kRiscvXor32:
      return kNoOpcodeFlags;

    case kRiscvLb:
    case kRiscvLbu:
    case kRiscvLd:
    case kRiscvLoadDouble:
    case kRiscvLh:
    case kRiscvLhu:
    case kRiscvLw:
    case kRiscvLoadFloat:
    case kRiscvLwu:
    case kRiscvRvvLd:
    case kRiscvPeek:
    case kRiscvUld:
    case kRiscvULoadDouble:
    case kRiscvUlh:
    case kRiscvUlhu:
    case kRiscvUlw:
    case kRiscvUlwu:
    case kRiscvULoadFloat:
    case kRiscvS128LoadSplat:
    case kRiscvS128Load64ExtendU:
    case kRiscvS128Load64ExtendS:
    case kRiscvS128LoadLane:
    case kRiscvWord64AtomicLoadUint64:
    case kRiscvLoadDecompressTaggedSigned:
    case kRiscvLoadDecompressTaggedPointer:
    case kRiscvLoadDecompressAnyTagged:
      return kIsLoadOperation;

    case kRiscvModD:
    case kRiscvModS:
    case kRiscvRvvSt:
    case kRiscvPush:
    case kRiscvSb:
    case kRiscvSd:
    case kRiscvStoreDouble:
    case kRiscvSh:
    case kRiscvStackClaim:
    case kRiscvStoreToStackSlot:
    case kRiscvSw:
    case kRiscvStoreFloat:
    case kRiscvUsd:
    case kRiscvUStoreDouble:
    case kRiscvUsh:
    case kRiscvUsw:
    case kRiscvUStoreFloat:
    case kRiscvSync:
    case kRiscvWord64AtomicStoreWord64:
    case kRiscvWord64AtomicAddUint64:
    case kRiscvWord64AtomicSubUint64:
    case kRiscvWord64AtomicAndUint64:
    case kRiscvWord64AtomicOrUint64:
    case kRiscvWord64AtomicXorUint64:
    case kRiscvWord64AtomicExchangeUint64:
    case kRiscvWord64AtomicCompareExchangeUint64:
    case kRiscvStoreCompressTagged:
    case kRiscvS128StoreLane:
      return kHasSideEffect;

#define CASE(Name) case k##Name:
      COMMON_ARCH_OPCODE_LIST(CASE)
#undef CASE
      // Already covered in architecture independent code.
      UNREACHABLE();
  }

  UNREACHABLE();
}

enum Latency {
  BRANCH = 4,  // Estimated max.
  RINT_S = 4,  // Estimated.
  RINT_D = 4,  // Estimated.

  // TODO(RISCV): remove MULT instructions (MIPS legacy).
  MULT = 4,
  MULTU = 4,
  DMULT = 4,

  MUL32 = 7,

  DIV32 = 50,  // Min:11 Max:50
  DIV64 = 50,
  DIVU32 = 50,
  DIVU64 = 50,

  ABS_S = 4,
  ABS_D = 4,
  NEG_S = 4,
  NEG_D = 4,
  ADD_S = 4,
  ADD_D = 4,
  SUB_S = 4,
  SUB_D = 4,
  MAX_S = 4,  // Estimated.
  MIN_S = 4,
  MAX_D = 4,  // Estimated.
  MIN_D = 4,
  C_cond_S = 4,
  C_cond_D = 4,
  MUL_S = 4,

  MADD_S = 4,
  MSUB_S = 4,
  NMADD_S = 4,
  NMSUB_S = 4,

  CABS_cond_S = 4,
  CABS_cond_D = 4,

  CVT_D_S = 4,
  CVT_PS_PW = 4,

  CVT_S_W = 4,
  CVT_S_L = 4,
  CVT_D_W = 4,
  CVT_D_L = 4,

  CVT_S_D = 4,

  CVT_W_S = 4,
  CVT_W_D = 4,
  CVT_L_S = 4,
  CVT_L_D = 4,

  CEIL_W_S = 4,
  CEIL_W_D = 4,
  CEIL_L_S = 4,
  CEIL_L_D = 4,

  FLOOR_W_S = 4,
  FLOOR_W_D = 4,
  FLOOR_L_S = 4,
  FLOOR_L_D = 4,

  ROUND_W_S = 4,
  ROUND_W_D = 4,
  ROUND_L_S = 4,
  ROUND_L_D = 4,

  TRUNC_W_S = 4,
  TRUNC_W_D = 4,
  TRUNC_L_S = 4,
  TRUNC_L_D = 4,

  MOV_S = 4,
  MOV_D = 4,

  MOVF_S = 4,
  MOVF_D = 4,

  MOVN_S = 4,
  MOVN_D = 4,

  MOVT_S = 4,
  MOVT_D = 4,

  MOVZ_S = 4,
  MOVZ_D = 4,

  MUL_D = 5,
  MADD_D = 5,
  MSUB_D = 5,
  NMADD_D = 5,
  NMSUB_D = 5,

  RECIP_S = 13,
  RECIP_D = 26,

  RSQRT_S = 17,
  RSQRT_D = 36,

  DIV_S = 17,
  SQRT_S = 17,

  DIV_D = 32,
  SQRT_D = 32,

  MOVT_FREG = 4,
  MOVT_HIGH_FREG = 4,
  MOVT_DREG = 4,
  LOAD_FLOAT = 4,
  LOAD_DOUBLE = 4,

  MOVF_FREG = 1,
  MOVF_HIGH_FREG = 1,
  MOVF_HIGH_DREG = 1,
  MOVF_HIGH = 1,
  MOVF_LOW = 1,
  STORE_FLOAT = 1,
  STORE_DOUBLE = 1,
};

int Add64Latency(bool is_operand_register = true) {
  if (is_operand_register) {
    return 1;
  } else {
    return 2;  // Estimated max.
  }
}

int Sub64Latency(bool is_operand_register = true) {
  return Add64Latency(is_operand_register);
}

int AndLatency(bool is_operand_register = true) {
  return Add64Latency(is_operand_register);
}

int OrLatency(bool is_operand_register = true) {
  return Add64Latency(is_operand_register);
}

int NorLatency(bool is_operand_register = true) {
  if (is_operand_register) {
    return 1;
  } else {
    return 2;  // Estimated max.
  }
}

int XorLatency(bool is_operand_register = true) {
  return Add64Latency(is_operand_register);
}

int Mul32Latency(bool is_operand_register = true) {
  if (is_operand_register) {
    return Latency::MUL32;
  } else {
    return Latency::MUL32 + 1;
  }
}

int Mul64Latency(bool is_operand_register = true) {
  int latency = Latency::DMULT + Latency::MOVF_LOW;
  if (!is_operand_register) {
    latency += 1;
  }
  return latency;
}

int Mulh32Latency(bool is_operand_register = true) {
  int latency = Latency::MULT + Latency::MOVF_HIGH;
  if (!is_operand_register) {
    latency += 1;
  }
  return latency;
}

int Mulhu32Latency(bool is_operand_register = true) {
  int latency = Latency::MULTU + Latency::MOVF_HIGH;
  if (!is_operand_register) {
    latency += 1;
  }
  return latency;
}

int Mulh64Latency(bool is_operand_register = true) {
  int latency = Latency::DMULT + Latency::MOVF_HIGH;
  if (!is_operand_register) {
    latency += 1;
  }
  return latency;
}

int Div32Latency(bool is_operand_register = true) {
  if (is_operand_register) {
    return Latency::DIV32;
  } else {
    return Latency::DIV32 + 1;
  }
}

int Divu32Latency(bool is_operand_register = true) {
  if (is_operand_register) {
    return Latency::DIVU32;
  } else {
    return Latency::DIVU32 + 1;
  }
}

int Div64Latency(bool is_operand_register = true) {
  int latency = Latency::DIV64 + Latency::MOVF_LOW;
  if (!is_operand_register) {
    latency += 1;
  }
  return latency;
}

int Divu64Latency(bool is_operand_register = true) {
  int latency = Latency::DIVU64 + Latency::MOVF_LOW;
  if (!is_operand_register) {
    latency += 1;
  }
  return latency;
}

int Mod32Latency(bool is_operand_register = true) {
  int latency = Latency::DIV32 + Latency::MOVF_HIGH;
  if (!is_operand_register) {
    latency += 1;
  }
  return latency;
}

int Modu32Latency(bool is_operand_register = true) {
  int latency = Latency::DIVU32 + Latency::MOVF_HIGH;
  if (!is_operand_register) {
    latency += 1;
  }
  return latency;
}

int Mod64Latency(bool is_operand_register = true) {
  int latency = Latency::DIV64 + Latency::MOVF_HIGH;
  if (!is_operand_register) {
    latency += 1;
  }
  return latency;
}

int Modu64Latency(bool is_operand_register = true) {
  int latency = Latency::DIV64 + Latency::MOVF_HIGH;
  if (!is_operand_register) {
    latency += 1;
  }
  return latency;
}

int MovzLatency() { return 1; }

int MovnLatency() { return 1; }

int CallLatency() {
  // Estimated.
  return Add64Latency(false) + Latency::BRANCH + 5;
}

int JumpLatency() {
  // Estimated max.
  return 1 + Add64Latency() + Latency::BRANCH + 2;
}

int SmiUntagLatency() { return 1; }

int PrepareForTailCallLatency() {
  // Estimated max.
  return 2 * (Add64Latency() + 1 + Add64Latency(false)) + 2 + Latency::BRANCH +
         Latency::BRANCH + 2 * Sub64Latency(false) + 2 + Latency::BRANCH + 1;
}

int AssemblePopArgumentsAdoptFrameLatency() {
  return 1 + Latency::BRANCH + 1 + SmiUntagLatency() +
         PrepareForTailCallLatency();
}

int AssertLatency() { return 1; }

int PrepareCallCFunctionLatency() {
  int frame_alignment = TurboAssembler::ActivationFrameAlignment();
  if (frame_alignment > kSystemPointerSize) {
    return 1 + Sub64Latency(false) + AndLatency(false) + 1;
  } else {
    return Sub64Latency(false);
  }
}

int AdjustBaseAndOffsetLatency() {
  return 3;  // Estimated max.
}

int AlignedMemoryLatency() { return AdjustBaseAndOffsetLatency() + 1; }

int UlhuLatency() {
  return AdjustBaseAndOffsetLatency() + 2 * AlignedMemoryLatency() + 2;
}

int UlwLatency() {
  // Estimated max.
  return AdjustBaseAndOffsetLatency() + 3;
}

int UlwuLatency() { return UlwLatency() + 1; }

int UldLatency() {
  // Estimated max.
  return AdjustBaseAndOffsetLatency() + 3;
}

int ULoadFloatLatency() { return UlwLatency() + Latency::MOVT_FREG; }

int ULoadDoubleLatency() { return UldLatency() + Latency::MOVT_DREG; }

int UshLatency() {
  // Estimated max.
  return AdjustBaseAndOffsetLatency() + 2 + 2 * AlignedMemoryLatency();
}

int UswLatency() { return AdjustBaseAndOffsetLatency() + 2; }

int UsdLatency() { return AdjustBaseAndOffsetLatency() + 2; }

int UStoreFloatLatency() { return Latency::MOVF_FREG + UswLatency(); }

int UStoreDoubleLatency() { return Latency::MOVF_HIGH_DREG + UsdLatency(); }

int LoadFloatLatency() {
  return AdjustBaseAndOffsetLatency() + Latency::LOAD_FLOAT;
}

int StoreFloatLatency() {
  return AdjustBaseAndOffsetLatency() + Latency::STORE_FLOAT;
}

int StoreDoubleLatency() {
  return AdjustBaseAndOffsetLatency() + Latency::STORE_DOUBLE;
}

int LoadDoubleLatency() {
  return AdjustBaseAndOffsetLatency() + Latency::LOAD_DOUBLE;
}

int MultiPushLatency() {
  int latency = Sub64Latency(false);
  for (int16_t i = kNumRegisters - 1; i >= 0; i--) {
    latency++;
  }
  return latency;
}

int MultiPushFPULatency() {
  int latency = Sub64Latency(false);
  for (int16_t i = kNumRegisters - 1; i >= 0; i--) {
    latency += StoreDoubleLatency();
  }
  return latency;
}

int PushCallerSavedLatency(SaveFPRegsMode fp_mode) {
  int latency = MultiPushLatency();
  if (fp_mode == SaveFPRegsMode::kSave) {
    latency += MultiPushFPULatency();
  }
  return latency;
}

int MultiPopLatency() {
  int latency = Add64Latency(false);
  for (int16_t i = 0; i < kNumRegisters; i++) {
    latency++;
  }
  return latency;
}

int MultiPopFPULatency() {
  int latency = Add64Latency(false);
  for (int16_t i = 0; i < kNumRegisters; i++) {
    latency += LoadDoubleLatency();
  }
  return latency;
}

int PopCallerSavedLatency(SaveFPRegsMode fp_mode) {
  int latency = MultiPopLatency();
  if (fp_mode == SaveFPRegsMode::kSave) {
    latency += MultiPopFPULatency();
  }
  return latency;
}

int CallCFunctionHelperLatency() {
  // Estimated.
  int latency = AndLatency(false) + Latency::BRANCH + 2 + CallLatency();
  if (base::OS::ActivationFrameAlignment() > kSystemPointerSize) {
    latency++;
  } else {
    latency += Add64Latency(false);
  }
  return latency;
}

int CallCFunctionLatency() { return 1 + CallCFunctionHelperLatency(); }

int AssembleArchJumpLatency() {
  // Estimated max.
  return Latency::BRANCH;
}

int GenerateSwitchTableLatency() {
  int latency = 6;
  latency += 2;
  return latency;
}

int AssembleArchTableSwitchLatency() {
  return Latency::BRANCH + GenerateSwitchTableLatency();
}

int DropAndRetLatency() {
  // Estimated max.
  return Add64Latency(false) + JumpLatency();
}

int AssemblerReturnLatency() {
  // Estimated max.
  return Add64Latency(false) + MultiPopLatency() + MultiPopFPULatency() +
         Latency::BRANCH + Add64Latency() + 1 + DropAndRetLatency();
}

int TryInlineTruncateDoubleToILatency() {
  return 2 + Latency::TRUNC_W_D + Latency::MOVF_FREG + 2 + AndLatency(false) +
         Latency::BRANCH;
}

int CallStubDelayedLatency() { return 1 + CallLatency(); }

int TruncateDoubleToIDelayedLatency() {
  // TODO(riscv): This no longer reflects how TruncateDoubleToI is called.
  return TryInlineTruncateDoubleToILatency() + 1 + Sub64Latency(false) +
         StoreDoubleLatency() + CallStubDelayedLatency() + Add64Latency(false) +
         1;
}

int CheckPageFlagLatency() {
  return AndLatency(false) + AlignedMemoryLatency() + AndLatency(false) +
         Latency::BRANCH;
}

int SltuLatency(bool is_operand_register = true) {
  if (is_operand_register) {
    return 1;
  } else {
    return 2;  // Estimated max.
  }
}

int BranchShortHelperLatency() {
  return SltuLatency() + 2;  // Estimated max.
}

int BranchShortLatency() { return BranchShortHelperLatency(); }

int MoveLatency() { return 1; }

int MovToFloatParametersLatency() { return 2 * MoveLatency(); }

int MovFromFloatResultLatency() { return MoveLatency(); }

int AddOverflow64Latency() {
  // Estimated max.
  return 6;
}

int SubOverflow64Latency() {
  // Estimated max.
  return 6;
}

int MulOverflow32Latency() {
  // Estimated max.
  return Mul32Latency() + Mulh32Latency() + 2;
}

// TODO(RISCV): This is incorrect for RISC-V.
int Clz64Latency() { return 1; }

int Ctz32Latency() {
  return Add64Latency(false) + XorLatency() + AndLatency() + Clz64Latency() +
         1 + Sub64Latency();
}

int Ctz64Latency() {
  return Add64Latency(false) + XorLatency() + AndLatency() + 1 + Sub64Latency();
}

int Popcnt32Latency() {
  return 2 + AndLatency() + Sub64Latency() + 1 + AndLatency() + 1 +
         AndLatency() + Add64Latency() + 1 + Add64Latency() + 1 + AndLatency() +
         1 + Mul32Latency() + 1;
}

int Popcnt64Latency() {
  return 2 + AndLatency() + Sub64Latency() + 1 + AndLatency() + 1 +
         AndLatency() + Add64Latency() + 1 + Add64Latency() + 1 + AndLatency() +
         1 + Mul64Latency() + 1;
}

int CompareFLatency() { return Latency::C_cond_S; }

int CompareF32Latency() { return CompareFLatency(); }

int CompareF64Latency() { return CompareFLatency(); }

int CompareIsNanFLatency() { return CompareFLatency(); }

int CompareIsNanF32Latency() { return CompareIsNanFLatency(); }

int CompareIsNanF64Latency() { return CompareIsNanFLatency(); }

int NegsLatency() {
  // Estimated.
  return CompareIsNanF32Latency() + 2 * Latency::BRANCH + Latency::NEG_S +
         Latency::MOVF_FREG + 1 + XorLatency() + Latency::MOVT_FREG;
}

int NegdLatency() {
  // Estimated.
  return CompareIsNanF64Latency() + 2 * Latency::BRANCH + Latency::NEG_D +
         Latency::MOVF_HIGH_DREG + 1 + XorLatency() + Latency::MOVT_DREG;
}

int Float64RoundLatency() {
  // For ceil_l_d, floor_l_d, round_l_d, trunc_l_d latency is 4.
  return Latency::MOVF_HIGH_DREG + 1 + Latency::BRANCH + Latency::MOV_D + 4 +
         Latency::MOVF_HIGH_DREG + Latency::BRANCH + Latency::CVT_D_L + 2 +
         Latency::MOVT_HIGH_FREG;
}

int Float32RoundLatency() {
  // For ceil_w_s, floor_w_s, round_w_s, trunc_w_s latency is 4.
  return Latency::MOVF_FREG + 1 + Latency::BRANCH + Latency::MOV_S + 4 +
         Latency::MOVF_FREG + Latency::BRANCH + Latency::CVT_S_W + 2 +
         Latency::MOVT_FREG;
}

int Float32MaxLatency() {
  // Estimated max.
  int latency = CompareIsNanF32Latency() + Latency::BRANCH;
  return latency + 5 * Latency::BRANCH + 2 * CompareF32Latency() +
         Latency::MOVF_FREG + 1 + Latency::MOV_S;
}

int Float64MaxLatency() {
  // Estimated max.
  int latency = CompareIsNanF64Latency() + Latency::BRANCH;
  return latency + 5 * Latency::BRANCH + 2 * CompareF64Latency() +
         Latency::MOVF_HIGH_DREG + Latency::MOV_D;
}

int Float32MinLatency() {
  // Estimated max.
  int latency = CompareIsNanF32Latency() + Latency::BRANCH;
  return latency + 5 * Latency::BRANCH + 2 * CompareF32Latency() +
         Latency::MOVF_FREG + 1 + Latency::MOV_S;
}

int Float64MinLatency() {
  // Estimated max.
  int latency = CompareIsNanF64Latency() + Latency::BRANCH;
  return latency + 5 * Latency::BRANCH + 2 * CompareF32Latency() +
         Latency::MOVF_HIGH_DREG + Latency::MOV_D;
}

int TruncLSLatency(bool load_status) {
  int latency = Latency::TRUNC_L_S + Latency::MOVF_HIGH_DREG;
  if (load_status) {
    latency += SltuLatency() + 7;
  }
  return latency;
}

int TruncLDLatency(bool load_status) {
  int latency = Latency::TRUNC_L_D + Latency::MOVF_HIGH_DREG;
  if (load_status) {
    latency += SltuLatency() + 7;
  }
  return latency;
}

int TruncUlSLatency() {
  // Estimated max.
  return 2 * CompareF32Latency() + CompareIsNanF32Latency() +
         4 * Latency::BRANCH + Latency::SUB_S + 2 * Latency::TRUNC_L_S +
         3 * Latency::MOVF_HIGH_DREG + OrLatency() + Latency::MOVT_FREG +
         Latency::MOV_S + SltuLatency() + 4;
}

int TruncUlDLatency() {
  // Estimated max.
  return 2 * CompareF64Latency() + CompareIsNanF64Latency() +
         4 * Latency::BRANCH + Latency::SUB_D + 2 * Latency::TRUNC_L_D +
         3 * Latency::MOVF_HIGH_DREG + OrLatency() + Latency::MOVT_DREG +
         Latency::MOV_D + SltuLatency() + 4;
}

int PushLatency() { return Add64Latency() + AlignedMemoryLatency(); }

int ByteSwapSignedLatency() { return 2; }

int LlLatency(int offset) {
  bool is_one_instruction = is_int12(offset);
  if (is_one_instruction) {
    return 1;
  } else {
    return 3;
  }
}

int ExtractBitsLatency(bool sign_extend, int size) {
  int latency = 2;
  if (sign_extend) {
    switch (size) {
      case 8:
      case 16:
      case 32:
        latency += 1;
        break;
      default:
        UNREACHABLE();
    }
  }
  return latency;
}

int InsertBitsLatency() { return 2 + Sub64Latency(false) + 2; }

int ScLatency(int offset) { return 3; }

int Word32AtomicExchangeLatency(bool sign_extend, int size) {
  return Add64Latency(false) + 1 + Sub64Latency() + 2 + LlLatency(0) +
         ExtractBitsLatency(sign_extend, size) + InsertBitsLatency() +
         ScLatency(0) + BranchShortLatency() + 1;
}

int Word32AtomicCompareExchangeLatency(bool sign_extend, int size) {
  return 2 + Sub64Latency() + 2 + LlLatency(0) +
         ExtractBitsLatency(sign_extend, size) + InsertBitsLatency() +
         ScLatency(0) + BranchShortLatency() + 1;
}

int InstructionScheduler::GetInstructionLatency(const Instruction* instr) {
  // TODO(RISCV): Verify these latencies for RISC-V (currently using MIPS
  // numbers).
  switch (instr->arch_opcode()) {
    case kArchCallCodeObject:
    case kArchCallWasmFunction:
      return CallLatency();
    case kArchTailCallCodeObject:
    case kArchTailCallWasm:
    case kArchTailCallAddress:
      return JumpLatency();
    case kArchCallJSFunction: {
      int latency = 0;
      if (FLAG_debug_code) {
        latency = 1 + AssertLatency();
      }
      return latency + 1 + Add64Latency(false) + CallLatency();
    }
    case kArchPrepareCallCFunction:
      return PrepareCallCFunctionLatency();
    case kArchSaveCallerRegisters: {
      auto fp_mode =
          static_cast<SaveFPRegsMode>(MiscField::decode(instr->opcode()));
      return PushCallerSavedLatency(fp_mode);
    }
    case kArchRestoreCallerRegisters: {
      auto fp_mode =
          static_cast<SaveFPRegsMode>(MiscField::decode(instr->opcode()));
      return PopCallerSavedLatency(fp_mode);
    }
    case kArchPrepareTailCall:
      return 2;
    case kArchCallCFunction:
      return CallCFunctionLatency();
    case kArchJmp:
      return AssembleArchJumpLatency();
    case kArchTableSwitch:
      return AssembleArchTableSwitchLatency();
    case kArchAbortCSADcheck:
      return CallLatency() + 1;
    case kArchDebugBreak:
      return 1;
    case kArchComment:
    case kArchNop:
    case kArchThrowTerminator:
    case kArchDeoptimize:
      return 0;
    case kArchRet:
      return AssemblerReturnLatency();
    case kArchFramePointer:
      return 1;
    case kArchParentFramePointer:
      // Estimated max.
      return AlignedMemoryLatency();
    case kArchTruncateDoubleToI:
      return TruncateDoubleToIDelayedLatency();
    case kArchStoreWithWriteBarrier:
      return Add64Latency() + 1 + CheckPageFlagLatency();
    case kArchStackSlot:
      // Estimated max.
      return Add64Latency(false) + AndLatency(false) + AssertLatency() +
             Add64Latency(false) + AndLatency(false) + BranchShortLatency() +
             1 + Sub64Latency() + Add64Latency();
    case kIeee754Float64Acos:
    case kIeee754Float64Acosh:
    case kIeee754Float64Asin:
    case kIeee754Float64Asinh:
    case kIeee754Float64Atan:
    case kIeee754Float64Atanh:
    case kIeee754Float64Atan2:
    case kIeee754Float64Cos:
    case kIeee754Float64Cosh:
    case kIeee754Float64Cbrt:
    case kIeee754Float64Exp:
    case kIeee754Float64Expm1:
    case kIeee754Float64Log:
    case kIeee754Float64Log1p:
    case kIeee754Float64Log10:
    case kIeee754Float64Log2:
    case kIeee754Float64Pow:
    case kIeee754Float64Sin:
    case kIeee754Float64Sinh:
    case kIeee754Float64Tan:
    case kIeee754Float64Tanh:
      return PrepareCallCFunctionLatency() + MovToFloatParametersLatency() +
             CallCFunctionLatency() + MovFromFloatResultLatency();
    case kRiscvAdd32:
    case kRiscvAdd64:
      return Add64Latency(instr->InputAt(1)->IsRegister());
    case kRiscvAddOvf64:
      return AddOverflow64Latency();
    case kRiscvSub32:
    case kRiscvSub64:
      return Sub64Latency(instr->InputAt(1)->IsRegister());
    case kRiscvSubOvf64:
      return SubOverflow64Latency();
    case kRiscvMul32:
      return Mul32Latency();
    case kRiscvMulOvf32:
      return MulOverflow32Latency();
    case kRiscvMulHigh32:
      return Mulh32Latency();
    case kRiscvMulHighU32:
      return Mulhu32Latency();
    case kRiscvMulHigh64:
      return Mulh64Latency();
    case kRiscvDiv32: {
      int latency = Div32Latency(instr->InputAt(1)->IsRegister());
      return latency + MovzLatency();
    }
    case kRiscvDivU32: {
      int latency = Divu32Latency(instr->InputAt(1)->IsRegister());
      return latency + MovzLatency();
    }
    case kRiscvMod32:
      return Mod32Latency();
    case kRiscvModU32:
      return Modu32Latency();
    case kRiscvMul64:
      return Mul64Latency();
    case kRiscvDiv64: {
      int latency = Div64Latency();
      return latency + MovzLatency();
    }
    case kRiscvDivU64: {
      int latency = Divu64Latency();
      return latency + MovzLatency();
    }
    case kRiscvMod64:
      return Mod64Latency();
    case kRiscvModU64:
      return Modu64Latency();
    case kRiscvAnd:
      return AndLatency(instr->InputAt(1)->IsRegister());
    case kRiscvAnd32: {
      bool is_operand_register = instr->InputAt(1)->IsRegister();
      int latency = AndLatency(is_operand_register);
      if (is_operand_register) {
        return latency + 2;
      } else {
        return latency + 1;
      }
    }
    case kRiscvOr:
      return OrLatency(instr->InputAt(1)->IsRegister());
    case kRiscvOr32: {
      bool is_operand_register = instr->InputAt(1)->IsRegister();
      int latency = OrLatency(is_operand_register);
      if (is_operand_register) {
        return latency + 2;
      } else {
        return latency + 1;
      }
    }
    case kRiscvNor:
      return NorLatency(instr->InputAt(1)->IsRegister());
    case kRiscvNor32: {
      bool is_operand_register = instr->InputAt(1)->IsRegister();
      int latency = NorLatency(is_operand_register);
      if (is_operand_register) {
        return latency + 2;
      } else {
        return latency + 1;
      }
    }
    case kRiscvXor:
      return XorLatency(instr->InputAt(1)->IsRegister());
    case kRiscvXor32: {
      bool is_operand_register = instr->InputAt(1)->IsRegister();
      int latency = XorLatency(is_operand_register);
      if (is_operand_register) {
        return latency + 2;
      } else {
        return latency + 1;
      }
    }
    case kRiscvClz32:
    case kRiscvClz64:
      return Clz64Latency();
    case kRiscvCtz32:
      return Ctz32Latency();
    case kRiscvCtz64:
      return Ctz64Latency();
    case kRiscvPopcnt32:
      return Popcnt32Latency();
    case kRiscvPopcnt64:
      return Popcnt64Latency();
    case kRiscvShl32:
      return 1;
    case kRiscvShr32:
    case kRiscvSar32:
    case kRiscvZeroExtendWord:
      return 2;
    case kRiscvSignExtendWord:
    case kRiscvShl64:
    case kRiscvShr64:
    case kRiscvSar64:
    case kRiscvRor32:
    case kRiscvRor64:
      return 1;
    case kRiscvTst:
      return AndLatency(instr->InputAt(1)->IsRegister());
    case kRiscvMov:
      return 1;
    case kRiscvCmpS:
      return MoveLatency() + CompareF32Latency();
    case kRiscvAddS:
      return Latency::ADD_S;
    case kRiscvSubS:
      return Latency::SUB_S;
    case kRiscvMulS:
      return Latency::MUL_S;
    case kRiscvDivS:
      return Latency::DIV_S;
    case kRiscvModS:
      return PrepareCallCFunctionLatency() + MovToFloatParametersLatency() +
             CallCFunctionLatency() + MovFromFloatResultLatency();
    case kRiscvAbsS:
      return Latency::ABS_S;
    case kRiscvNegS:
      return NegdLatency();
    case kRiscvSqrtS:
      return Latency::SQRT_S;
    case kRiscvMaxS:
      return Latency::MAX_S;
    case kRiscvMinS:
      return Latency::MIN_S;
    case kRiscvCmpD:
      return MoveLatency() + CompareF64Latency();
    case kRiscvAddD:
      return Latency::ADD_D;
    case kRiscvSubD:
      return Latency::SUB_D;
    case kRiscvMulD:
      return Latency::MUL_D;
    case kRiscvDivD:
      return Latency::DIV_D;
    case kRiscvModD:
      return PrepareCallCFunctionLatency() + MovToFloatParametersLatency() +
             CallCFunctionLatency() + MovFromFloatResultLatency();
    case kRiscvAbsD:
      return Latency::ABS_D;
    case kRiscvNegD:
      return NegdLatency();
    case kRiscvSqrtD:
      return Latency::SQRT_D;
    case kRiscvMaxD:
      return Latency::MAX_D;
    case kRiscvMinD:
      return Latency::MIN_D;
    case kRiscvFloat64RoundDown:
    case kRiscvFloat64RoundTruncate:
    case kRiscvFloat64RoundUp:
    case kRiscvFloat64RoundTiesEven:
      return Float64RoundLatency();
    case kRiscvFloat32RoundDown:
    case kRiscvFloat32RoundTruncate:
    case kRiscvFloat32RoundUp:
    case kRiscvFloat32RoundTiesEven:
      return Float32RoundLatency();
    case kRiscvFloat32Max:
      return Float32MaxLatency();
    case kRiscvFloat64Max:
      return Float64MaxLatency();
    case kRiscvFloat32Min:
      return Float32MinLatency();
    case kRiscvFloat64Min:
      return Float64MinLatency();
    case kRiscvFloat64SilenceNaN:
      return Latency::SUB_D;
    case kRiscvCvtSD:
      return Latency::CVT_S_D;
    case kRiscvCvtDS:
      return Latency::CVT_D_S;
    case kRiscvCvtDW:
      return Latency::MOVT_FREG + Latency::CVT_D_W;
    case kRiscvCvtSW:
      return Latency::MOVT_FREG + Latency::CVT_S_W;
    case kRiscvCvtSUw:
      return 1 + Latency::MOVT_DREG + Latency::CVT_S_L;
    case kRiscvCvtSL:
      return Latency::MOVT_DREG + Latency::CVT_S_L;
    case kRiscvCvtDL:
      return Latency::MOVT_DREG + Latency::CVT_D_L;
    case kRiscvCvtDUw:
      return 1 + Latency::MOVT_DREG + Latency::CVT_D_L;
    case kRiscvCvtDUl:
      return 2 * Latency::BRANCH + 3 + 2 * Latency::MOVT_DREG +
             2 * Latency::CVT_D_L + Latency::ADD_D;
    case kRiscvCvtSUl:
      return 2 * Latency::BRANCH + 3 + 2 * Latency::MOVT_DREG +
             2 * Latency::CVT_S_L + Latency::ADD_S;
    case kRiscvFloorWD:
      return Latency::FLOOR_W_D + Latency::MOVF_FREG;
    case kRiscvCeilWD:
      return Latency::CEIL_W_D + Latency::MOVF_FREG;
    case kRiscvRoundWD:
      return Latency::ROUND_W_D + Latency::MOVF_FREG;
    case kRiscvTruncWD:
      return Latency::TRUNC_W_D + Latency::MOVF_FREG;
    case kRiscvFloorWS:
      return Latency::FLOOR_W_S + Latency::MOVF_FREG;
    case kRiscvCeilWS:
      return Latency::CEIL_W_S + Latency::MOVF_FREG;
    case kRiscvRoundWS:
      return Latency::ROUND_W_S + Latency::MOVF_FREG;
    case kRiscvTruncWS:
      return Latency::TRUNC_W_S + Latency::MOVF_FREG + 2 + MovnLatency();
    case kRiscvTruncLS:
      return TruncLSLatency(instr->OutputCount() > 1);
    case kRiscvTruncLD:
      return TruncLDLatency(instr->OutputCount() > 1);
    case kRiscvTruncUwD:
      // Estimated max.
      return CompareF64Latency() + 2 * Latency::BRANCH +
             2 * Latency::TRUNC_W_D + Latency::SUB_D + OrLatency() +
             Latency::MOVT_FREG + Latency::MOVF_FREG + Latency::MOVT_HIGH_FREG +
             1;
    case kRiscvTruncUwS:
      // Estimated max.
      return CompareF32Latency() + 2 * Latency::BRANCH +
             2 * Latency::TRUNC_W_S + Latency::SUB_S + OrLatency() +
             Latency::MOVT_FREG + 2 * Latency::MOVF_FREG + 2 + MovzLatency();
    case kRiscvTruncUlS:
      return TruncUlSLatency();
    case kRiscvTruncUlD:
      return TruncUlDLatency();
    case kRiscvBitcastDL:
      return Latency::MOVF_HIGH_DREG;
    case kRiscvBitcastLD:
      return Latency::MOVT_DREG;
    case kRiscvFloat64ExtractLowWord32:
      return Latency::MOVF_FREG;
    case kRiscvFloat64InsertLowWord32:
      return Latency::MOVF_HIGH_FREG + Latency::MOVT_FREG +
             Latency::MOVT_HIGH_FREG;
    case kRiscvFloat64ExtractHighWord32:
      return Latency::MOVF_HIGH_FREG;
    case kRiscvFloat64InsertHighWord32:
      return Latency::MOVT_HIGH_FREG;
    case kRiscvSignExtendByte:
    case kRiscvSignExtendShort:
      return 1;
    case kRiscvLbu:
    case kRiscvLb:
    case kRiscvLhu:
    case kRiscvLh:
    case kRiscvLwu:
    case kRiscvLw:
    case kRiscvLd:
    case kRiscvSb:
    case kRiscvSh:
    case kRiscvSw:
    case kRiscvSd:
      return AlignedMemoryLatency();
    case kRiscvLoadFloat:
      return ULoadFloatLatency();
    case kRiscvLoadDouble:
      return LoadDoubleLatency();
    case kRiscvStoreFloat:
      return StoreFloatLatency();
    case kRiscvStoreDouble:
      return StoreDoubleLatency();
    case kRiscvUlhu:
    case kRiscvUlh:
      return UlhuLatency();
    case kRiscvUlwu:
      return UlwuLatency();
    case kRiscvUlw:
      return UlwLatency();
    case kRiscvUld:
      return UldLatency();
    case kRiscvULoadFloat:
      return ULoadFloatLatency();
    case kRiscvULoadDouble:
      return ULoadDoubleLatency();
    case kRiscvUsh:
      return UshLatency();
    case kRiscvUsw:
      return UswLatency();
    case kRiscvUsd:
      return UsdLatency();
    case kRiscvUStoreFloat:
      return UStoreFloatLatency();
    case kRiscvUStoreDouble:
      return UStoreDoubleLatency();
    case kRiscvPush: {
      int latency = 0;
      if (instr->InputAt(0)->IsFPRegister()) {
        latency = StoreDoubleLatency() + Sub64Latency(false);
      } else {
        latency = PushLatency();
      }
      return latency;
    }
    case kRiscvPeek: {
      int latency = 0;
      if (instr->OutputAt(0)->IsFPRegister()) {
        auto op = LocationOperand::cast(instr->OutputAt(0));
        switch (op->representation()) {
          case MachineRepresentation::kFloat64:
            latency = LoadDoubleLatency();
            break;
          case MachineRepresentation::kFloat32:
            latency = Latency::LOAD_FLOAT;
            break;
          default:
            UNREACHABLE();
        }
      } else {
        latency = AlignedMemoryLatency();
      }
      return latency;
    }
    case kRiscvStackClaim:
      return Sub64Latency(false);
    case kRiscvStoreToStackSlot: {
      int latency = 0;
      if (instr->InputAt(0)->IsFPRegister()) {
        if (instr->InputAt(0)->IsSimd128Register()) {
          latency = 1;  // Estimated value.
        } else {
          latency = StoreDoubleLatency();
        }
      } else {
        latency = AlignedMemoryLatency();
      }
      return latency;
    }
    case kRiscvByteSwap64:
      return ByteSwapSignedLatency();
    case kRiscvByteSwap32:
      return ByteSwapSignedLatency();
    case kAtomicLoadInt8:
    case kAtomicLoadUint8:
    case kAtomicLoadInt16:
    case kAtomicLoadUint16:
    case kAtomicLoadWord32:
      return 2;
    case kAtomicStoreWord8:
    case kAtomicStoreWord16:
    case kAtomicStoreWord32:
      return 3;
    case kAtomicExchangeInt8:
      return Word32AtomicExchangeLatency(true, 8);
    case kAtomicExchangeUint8:
      return Word32AtomicExchangeLatency(false, 8);
    case kAtomicExchangeInt16:
      return Word32AtomicExchangeLatency(true, 16);
    case kAtomicExchangeUint16:
      return Word32AtomicExchangeLatency(false, 16);
    case kAtomicExchangeWord32:
      return 2 + LlLatency(0) + 1 + ScLatency(0) + BranchShortLatency() + 1;
    case kAtomicCompareExchangeInt8:
      return Word32AtomicCompareExchangeLatency(true, 8);
    case kAtomicCompareExchangeUint8:
      return Word32AtomicCompareExchangeLatency(false, 8);
    case kAtomicCompareExchangeInt16:
      return Word32AtomicCompareExchangeLatency(true, 16);
    case kAtomicCompareExchangeUint16:
      return Word32AtomicCompareExchangeLatency(false, 16);
    case kAtomicCompareExchangeWord32:
      return 3 + LlLatency(0) + BranchShortLatency() + 1 + ScLatency(0) +
             BranchShortLatency() + 1;
    case kRiscvAssertEqual:
      return AssertLatency();
    default:
      return 1;
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

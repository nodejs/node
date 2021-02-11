// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/code-generator.h"
#include "src/compiler/backend/instruction-scheduler.h"

namespace v8 {
namespace internal {
namespace compiler {

bool InstructionScheduler::SchedulerSupported() { return true; }

int InstructionScheduler::GetTargetInstructionFlags(
    const Instruction* instr) const {
  switch (instr->arch_opcode()) {
    case kMipsAbsD:
    case kMipsAbsS:
    case kMipsAdd:
    case kMipsAddD:
    case kMipsAddOvf:
    case kMipsAddPair:
    case kMipsAddS:
    case kMipsAnd:
    case kMipsByteSwap32:
    case kMipsCeilWD:
    case kMipsCeilWS:
    case kMipsClz:
    case kMipsCmp:
    case kMipsCmpD:
    case kMipsCmpS:
    case kMipsCtz:
    case kMipsCvtDS:
    case kMipsCvtDUw:
    case kMipsCvtDW:
    case kMipsCvtSD:
    case kMipsCvtSUw:
    case kMipsCvtSW:
    case kMipsDiv:
    case kMipsDivD:
    case kMipsDivS:
    case kMipsDivU:
    case kMipsExt:
    case kMipsF64x2Abs:
    case kMipsF64x2Neg:
    case kMipsF64x2Sqrt:
    case kMipsF64x2Add:
    case kMipsF64x2Sub:
    case kMipsF64x2Mul:
    case kMipsF64x2Div:
    case kMipsF64x2Min:
    case kMipsF64x2Max:
    case kMipsF64x2Eq:
    case kMipsF64x2Ne:
    case kMipsF64x2Lt:
    case kMipsF64x2Le:
    case kMipsF64x2Splat:
    case kMipsF64x2ExtractLane:
    case kMipsF64x2ReplaceLane:
    case kMipsF64x2Pmin:
    case kMipsF64x2Pmax:
    case kMipsF64x2Ceil:
    case kMipsF64x2Floor:
    case kMipsF64x2Trunc:
    case kMipsF64x2NearestInt:
    case kMipsI64x2Add:
    case kMipsI64x2Sub:
    case kMipsI64x2Mul:
    case kMipsI64x2Neg:
    case kMipsI64x2Shl:
    case kMipsI64x2ShrS:
    case kMipsI64x2ShrU:
    case kMipsF32x4Abs:
    case kMipsF32x4Add:
    case kMipsF32x4AddHoriz:
    case kMipsF32x4Eq:
    case kMipsF32x4ExtractLane:
    case kMipsF32x4Le:
    case kMipsF32x4Lt:
    case kMipsF32x4Max:
    case kMipsF32x4Min:
    case kMipsF32x4Mul:
    case kMipsF32x4Div:
    case kMipsF32x4Ne:
    case kMipsF32x4Neg:
    case kMipsF32x4Sqrt:
    case kMipsF32x4RecipApprox:
    case kMipsF32x4RecipSqrtApprox:
    case kMipsF32x4ReplaceLane:
    case kMipsF32x4SConvertI32x4:
    case kMipsF32x4Splat:
    case kMipsF32x4Sub:
    case kMipsF32x4UConvertI32x4:
    case kMipsF32x4Pmin:
    case kMipsF32x4Pmax:
    case kMipsF32x4Ceil:
    case kMipsF32x4Floor:
    case kMipsF32x4Trunc:
    case kMipsF32x4NearestInt:
    case kMipsFloat32Max:
    case kMipsFloat32Min:
    case kMipsFloat32RoundDown:
    case kMipsFloat32RoundTiesEven:
    case kMipsFloat32RoundTruncate:
    case kMipsFloat32RoundUp:
    case kMipsFloat64ExtractHighWord32:
    case kMipsFloat64ExtractLowWord32:
    case kMipsFloat64InsertHighWord32:
    case kMipsFloat64InsertLowWord32:
    case kMipsFloat64Max:
    case kMipsFloat64Min:
    case kMipsFloat64RoundDown:
    case kMipsFloat64RoundTiesEven:
    case kMipsFloat64RoundTruncate:
    case kMipsFloat64RoundUp:
    case kMipsFloat64SilenceNaN:
    case kMipsFloorWD:
    case kMipsFloorWS:
    case kMipsI16x8Add:
    case kMipsI16x8AddHoriz:
    case kMipsI16x8AddSatS:
    case kMipsI16x8AddSatU:
    case kMipsI16x8Eq:
    case kMipsI16x8ExtractLaneU:
    case kMipsI16x8ExtractLaneS:
    case kMipsI16x8GeS:
    case kMipsI16x8GeU:
    case kMipsI16x8RoundingAverageU:
    case kMipsI16x8GtS:
    case kMipsI16x8GtU:
    case kMipsI16x8MaxS:
    case kMipsI16x8MaxU:
    case kMipsI16x8MinS:
    case kMipsI16x8MinU:
    case kMipsI16x8Mul:
    case kMipsI16x8Ne:
    case kMipsI16x8Neg:
    case kMipsI16x8ReplaceLane:
    case kMipsI16x8SConvertI32x4:
    case kMipsI16x8SConvertI8x16High:
    case kMipsI16x8SConvertI8x16Low:
    case kMipsI16x8Shl:
    case kMipsI16x8ShrS:
    case kMipsI16x8ShrU:
    case kMipsI16x8Splat:
    case kMipsI16x8Sub:
    case kMipsI16x8SubSatS:
    case kMipsI16x8SubSatU:
    case kMipsI16x8UConvertI32x4:
    case kMipsI16x8UConvertI8x16High:
    case kMipsI16x8UConvertI8x16Low:
    case kMipsI16x8Abs:
    case kMipsI16x8BitMask:
    case kMipsI32x4Add:
    case kMipsI32x4AddHoriz:
    case kMipsI32x4Eq:
    case kMipsI32x4ExtractLane:
    case kMipsI32x4GeS:
    case kMipsI32x4GeU:
    case kMipsI32x4GtS:
    case kMipsI32x4GtU:
    case kMipsI32x4MaxS:
    case kMipsI32x4MaxU:
    case kMipsI32x4MinS:
    case kMipsI32x4MinU:
    case kMipsI32x4Mul:
    case kMipsI32x4Ne:
    case kMipsI32x4Neg:
    case kMipsI32x4ReplaceLane:
    case kMipsI32x4SConvertF32x4:
    case kMipsI32x4SConvertI16x8High:
    case kMipsI32x4SConvertI16x8Low:
    case kMipsI32x4Shl:
    case kMipsI32x4ShrS:
    case kMipsI32x4ShrU:
    case kMipsI32x4Splat:
    case kMipsI32x4Sub:
    case kMipsI32x4UConvertF32x4:
    case kMipsI32x4UConvertI16x8High:
    case kMipsI32x4UConvertI16x8Low:
    case kMipsI32x4Abs:
    case kMipsI32x4BitMask:
    case kMipsI32x4DotI16x8S:
    case kMipsI8x16Add:
    case kMipsI8x16AddSatS:
    case kMipsI8x16AddSatU:
    case kMipsI8x16Eq:
    case kMipsI8x16ExtractLaneU:
    case kMipsI8x16ExtractLaneS:
    case kMipsI8x16GeS:
    case kMipsI8x16GeU:
    case kMipsI8x16RoundingAverageU:
    case kMipsI8x16GtS:
    case kMipsI8x16GtU:
    case kMipsI8x16MaxS:
    case kMipsI8x16MaxU:
    case kMipsI8x16MinS:
    case kMipsI8x16MinU:
    case kMipsI8x16Mul:
    case kMipsI8x16Ne:
    case kMipsI8x16Neg:
    case kMipsI8x16ReplaceLane:
    case kMipsI8x16SConvertI16x8:
    case kMipsI8x16Shl:
    case kMipsI8x16ShrS:
    case kMipsI8x16ShrU:
    case kMipsI8x16Splat:
    case kMipsI8x16Sub:
    case kMipsI8x16SubSatS:
    case kMipsI8x16SubSatU:
    case kMipsI8x16UConvertI16x8:
    case kMipsI8x16Abs:
    case kMipsI8x16BitMask:
    case kMipsIns:
    case kMipsLsa:
    case kMipsMaddD:
    case kMipsMaddS:
    case kMipsMaxD:
    case kMipsMaxS:
    case kMipsMinD:
    case kMipsMinS:
    case kMipsMod:
    case kMipsModU:
    case kMipsMov:
    case kMipsMsubD:
    case kMipsMsubS:
    case kMipsMul:
    case kMipsMulD:
    case kMipsMulHigh:
    case kMipsMulHighU:
    case kMipsMulOvf:
    case kMipsMulPair:
    case kMipsMulS:
    case kMipsNegD:
    case kMipsNegS:
    case kMipsNor:
    case kMipsOr:
    case kMipsPopcnt:
    case kMipsRor:
    case kMipsRoundWD:
    case kMipsRoundWS:
    case kMipsS128And:
    case kMipsS128Not:
    case kMipsS128Or:
    case kMipsS128Select:
    case kMipsS128Xor:
    case kMipsS128Zero:
    case kMipsS128AndNot:
    case kMipsS16x2Reverse:
    case kMipsS16x4Reverse:
    case kMipsS16x8InterleaveEven:
    case kMipsS16x8InterleaveLeft:
    case kMipsS16x8InterleaveOdd:
    case kMipsS16x8InterleaveRight:
    case kMipsS16x8PackEven:
    case kMipsS16x8PackOdd:
    case kMipsV8x16AllTrue:
    case kMipsV8x16AnyTrue:
    case kMipsV32x4AllTrue:
    case kMipsV32x4AnyTrue:
    case kMipsV16x8AllTrue:
    case kMipsV16x8AnyTrue:
    case kMipsS32x4InterleaveEven:
    case kMipsS32x4InterleaveLeft:
    case kMipsS32x4InterleaveOdd:
    case kMipsS32x4InterleaveRight:
    case kMipsS32x4PackEven:
    case kMipsS32x4PackOdd:
    case kMipsS32x4Shuffle:
    case kMipsS8x16Concat:
    case kMipsS8x16InterleaveEven:
    case kMipsS8x16InterleaveLeft:
    case kMipsS8x16InterleaveOdd:
    case kMipsS8x16InterleaveRight:
    case kMipsS8x16PackEven:
    case kMipsS8x16PackOdd:
    case kMipsI8x16Shuffle:
    case kMipsI8x16Swizzle:
    case kMipsS8x2Reverse:
    case kMipsS8x4Reverse:
    case kMipsS8x8Reverse:
    case kMipsSar:
    case kMipsSarPair:
    case kMipsSeb:
    case kMipsSeh:
    case kMipsShl:
    case kMipsShlPair:
    case kMipsShr:
    case kMipsShrPair:
    case kMipsSqrtD:
    case kMipsSqrtS:
    case kMipsSub:
    case kMipsSubD:
    case kMipsSubOvf:
    case kMipsSubPair:
    case kMipsSubS:
    case kMipsTruncUwD:
    case kMipsTruncUwS:
    case kMipsTruncWD:
    case kMipsTruncWS:
    case kMipsTst:
    case kMipsXor:
      return kNoOpcodeFlags;

    case kMipsLb:
    case kMipsLbu:
    case kMipsLdc1:
    case kMipsLh:
    case kMipsLhu:
    case kMipsLw:
    case kMipsLwc1:
    case kMipsMsaLd:
    case kMipsPeek:
    case kMipsUldc1:
    case kMipsUlh:
    case kMipsUlhu:
    case kMipsUlw:
    case kMipsUlwc1:
    case kMipsS128Load8Splat:
    case kMipsS128Load16Splat:
    case kMipsS128Load32Splat:
    case kMipsS128Load64Splat:
    case kMipsS128Load8x8S:
    case kMipsS128Load8x8U:
    case kMipsS128Load16x4S:
    case kMipsS128Load16x4U:
    case kMipsS128Load32x2S:
    case kMipsS128Load32x2U:
    case kMipsWord32AtomicPairLoad:
      return kIsLoadOperation;

    case kMipsModD:
    case kMipsModS:
    case kMipsMsaSt:
    case kMipsPush:
    case kMipsSb:
    case kMipsSdc1:
    case kMipsSh:
    case kMipsStackClaim:
    case kMipsStoreToStackSlot:
    case kMipsSw:
    case kMipsSwc1:
    case kMipsUsdc1:
    case kMipsUsh:
    case kMipsUsw:
    case kMipsUswc1:
    case kMipsSync:
    case kMipsWord32AtomicPairStore:
    case kMipsWord32AtomicPairAdd:
    case kMipsWord32AtomicPairSub:
    case kMipsWord32AtomicPairAnd:
    case kMipsWord32AtomicPairOr:
    case kMipsWord32AtomicPairXor:
    case kMipsWord32AtomicPairExchange:
    case kMipsWord32AtomicPairCompareExchange:
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

  MULT = 4,
  MULTU = 4,
  MADD = 4,
  MADDU = 4,
  MSUB = 4,
  MSUBU = 4,

  MUL = 7,
  MULU = 7,
  MUH = 7,
  MUHU = 7,

  DIV = 50,  // Min:11 Max:50
  DIVU = 50,

  ABS_S = 4,
  ABS_D = 4,
  NEG_S = 4,
  NEG_D = 4,
  ADD_S = 4,
  ADD_D = 4,
  SUB_S = 4,
  SUB_D = 4,
  MAX_S = 4,  // Estimated.
  MAX_D = 4,  // Estimated.
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

  MTC1 = 4,
  MTHC1 = 4,
  DMTC1 = 4,
  LWC1 = 4,
  LDC1 = 4,
  LDXC1 = 4,
  LUXC1 = 4,
  LWXC1 = 4,

  MFC1 = 1,
  MFHC1 = 1,
  MFHI = 1,
  MFLO = 1,
  DMFC1 = 1,
  SWC1 = 1,
  SDC1 = 1,
  SDXC1 = 1,
  SUXC1 = 1,
  SWXC1 = 1,
};

int ClzLatency() {
  if (IsMipsArchVariant(kLoongson)) {
    return (6 + 2 * Latency::BRANCH);
  } else {
    return 1;
  }
}

int RorLatency(bool is_operand_register = true) {
  if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
    return 1;
  } else {
    if (is_operand_register) {
      return 4;
    } else {
      return 3;  // Estimated max.
    }
  }
}

int AdduLatency(bool is_operand_register = true) {
  if (is_operand_register) {
    return 1;
  } else {
    return 2;  // Estimated max.
  }
}

int XorLatency(bool is_operand_register = true) {
  return AdduLatency(is_operand_register);
}

int AndLatency(bool is_operand_register = true) {
  return AdduLatency(is_operand_register);
}

int OrLatency(bool is_operand_register = true) {
  return AdduLatency(is_operand_register);
}

int SubuLatency(bool is_operand_register = true) {
  return AdduLatency(is_operand_register);
}

int MulLatency(bool is_operand_register = true) {
  if (is_operand_register) {
    if (IsMipsArchVariant(kLoongson)) {
      return Latency::MULT + 1;
    } else {
      return Latency::MUL + 1;
    }
  } else {
    if (IsMipsArchVariant(kLoongson)) {
      return Latency::MULT + 2;
    } else {
      return Latency::MUL + 2;
    }
  }
}

int NorLatency(bool is_operand_register = true) {
  if (is_operand_register) {
    return 1;
  } else {
    return 2;
  }
}

int InsLatency() {
  if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
    return 1;
  } else {
    return SubuLatency(false) + 7;
  }
}

int ShlPairLatency(bool is_operand_register = true) {
  if (is_operand_register) {
    int latency =
        AndLatency(false) + NorLatency() + OrLatency() + AndLatency(false) + 4;
    if (IsMipsArchVariant(kLoongson) || IsMipsArchVariant(kMips32r6)) {
      return latency + Latency::BRANCH + 2;
    } else {
      return latency + 2;
    }
  } else {
    return 2;
  }
}

int ShrPairLatency(bool is_operand_register = true, uint32_t shift = 0) {
  if (is_operand_register) {
    int latency =
        AndLatency(false) + NorLatency() + OrLatency() + AndLatency(false) + 4;
    if (IsMipsArchVariant(kLoongson) || IsMipsArchVariant(kMips32r6)) {
      return latency + Latency::BRANCH + 2;
    } else {
      return latency + 2;
    }
  } else {
    // Estimated max.
    return (InsLatency() + 2 > OrLatency() + 3) ? InsLatency() + 2
                                                : OrLatency() + 3;
  }
}

int SarPairLatency(bool is_operand_register = true, uint32_t shift = 0) {
  if (is_operand_register) {
    return AndLatency(false) + NorLatency() + OrLatency() + AndLatency(false) +
           Latency::BRANCH + 6;
  } else {
    shift = shift & 0x3F;
    if (shift == 0) {
      return 2;
    } else if (shift < 32) {
      if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
        return InsLatency() + 2;
      } else {
        return OrLatency() + 3;
      }
    } else if (shift == 32) {
      return 2;
    } else {
      return 2;
    }
  }
}

int ExtLatency() {
  if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
    return 1;
  } else {
    // Estimated max.
    return 2;
  }
}

int LsaLatency() {
  // Estimated max.
  return AdduLatency() + 1;
}

int SltLatency(bool is_operand_register = true) {
  if (is_operand_register) {
    return 1;
  } else {
    return 2;  // Estimated max.
  }
}

int SltuLatency(bool is_operand_register = true) {
  return SltLatency(is_operand_register);
}

int AddPairLatency() { return 3 * AdduLatency() + SltLatency(); }

int SubPairLatency() { return SltuLatency() + 3 * SubuLatency(); }

int MuluLatency(bool is_operand_register = true) {
  int latency = 0;
  if (!is_operand_register) latency++;
  if (!IsMipsArchVariant(kMips32r6)) {
    return latency + Latency::MULTU + 2;
  } else {
    return latency + Latency::MULU + Latency::MUHU;
  }
}

int MulPairLatency() {
  return MuluLatency() + 2 * MulLatency() + 2 * AdduLatency();
}

int MaddSLatency() {
  if (IsMipsArchVariant(kMips32r2)) {
    return Latency::MADD_D;
  } else {
    return Latency::MUL_D + Latency::ADD_D;
  }
}

int MaddDLatency() {
  if (IsMipsArchVariant(kMips32r2)) {
    return Latency::MADD_D;
  } else {
    return Latency::MUL_D + Latency::ADD_D;
  }
}

int MsubSLatency() {
  if (IsMipsArchVariant(kMips32r2)) {
    return Latency::MSUB_S;
  } else {
    return Latency::MUL_S + Latency::SUB_S;
  }
}

int MsubDLatency() {
  if (IsMipsArchVariant(kMips32r2)) {
    return Latency::MSUB_D;
  } else {
    return Latency::MUL_D + Latency::SUB_D;
  }
}

int Mfhc1Latency() {
  if (IsFp32Mode()) {
    return Latency::MFC1;
  } else {
    return 1;
  }
}

int Mthc1Latency() {
  if (IsFp32Mode()) {
    return Latency::MTC1;
  } else {
    return 1;
  }
}

int MoveLatency(bool is_double_register = true) {
  if (!is_double_register) {
    return Latency::MTC1 + 1;
  } else {
    return Mthc1Latency() + 1;  // Estimated.
  }
}

int Float64RoundLatency() {
  if (IsMipsArchVariant(kMips32r6)) {
    return Latency::RINT_D + 4;
  } else {
    // For ceil_l_d, floor_l_d, round_l_d, trunc_l_d latency is 4.
    return Mfhc1Latency() + ExtLatency() + Latency::BRANCH + Latency::MOV_D +
           4 + MoveLatency() + 1 + Latency::BRANCH + Latency::CVT_D_L;
  }
}

int Float32RoundLatency() {
  if (IsMipsArchVariant(kMips32r6)) {
    return Latency::RINT_S + 4;
  } else {
    // For ceil_w_s, floor_w_s, round_w_s, trunc_w_s latency is 4.
    return Latency::MFC1 + ExtLatency() + Latency::BRANCH + Latency::MOV_S + 4 +
           Latency::MFC1 + Latency::BRANCH + Latency::CVT_S_W;
  }
}

int CvtDUwLatency() {
  if (IsFp64Mode()) {
    return Latency::MTC1 + Mthc1Latency() + Latency::CVT_D_L;
  } else {
    return Latency::BRANCH + Latency::MTC1 + 1 + Latency::MTC1 +
           Mthc1Latency() + Latency::CVT_D_W + Latency::BRANCH +
           Latency::ADD_D + Latency::CVT_D_W;
  }
}

int CvtSUwLatency() { return CvtDUwLatency() + Latency::CVT_S_D; }

int Floor_w_dLatency() {
  if (IsMipsArchVariant(kLoongson)) {
    return Mfhc1Latency() + Latency::FLOOR_W_D + Mthc1Latency();
  } else {
    return Latency::FLOOR_W_D;
  }
}

int FloorWDLatency() { return Floor_w_dLatency() + Latency::MFC1; }

int Ceil_w_dLatency() {
  if (IsMipsArchVariant(kLoongson)) {
    return Mfhc1Latency() + Latency::CEIL_W_D + Mthc1Latency();
  } else {
    return Latency::CEIL_W_D;
  }
}

int CeilWDLatency() { return Ceil_w_dLatency() + Latency::MFC1; }

int Round_w_dLatency() {
  if (IsMipsArchVariant(kLoongson)) {
    return Mfhc1Latency() + Latency::ROUND_W_D + Mthc1Latency();
  } else {
    return Latency::ROUND_W_D;
  }
}

int RoundWDLatency() { return Round_w_dLatency() + Latency::MFC1; }

int Trunc_w_dLatency() {
  if (IsMipsArchVariant(kLoongson)) {
    return Mfhc1Latency() + Latency::TRUNC_W_D + Mthc1Latency();
  } else {
    return Latency::TRUNC_W_D;
  }
}

int MovnLatency() {
  if (IsMipsArchVariant(kLoongson) || IsMipsArchVariant(kMips32r6)) {
    return Latency::BRANCH + 1;
  } else {
    return 1;
  }
}

int Trunc_uw_dLatency() {
  return 1 + Latency::MTC1 + Mthc1Latency() + Latency::BRANCH + Latency::SUB_D +
         Latency::TRUNC_W_D + Latency::MFC1 + OrLatency(false) +
         Latency::BRANCH + Latency::TRUNC_W_D + Latency::MFC1;
}

int Trunc_uw_sLatency() {
  return 1 + Latency::MTC1 + Latency::BRANCH + Latency::SUB_S +
         Latency::TRUNC_W_S + Latency::MFC1 + OrLatency(false) +
         Latency::TRUNC_W_S + Latency::MFC1;
}

int MovzLatency() {
  if (IsMipsArchVariant(kLoongson) || IsMipsArchVariant(kMips32r6)) {
    return Latency::BRANCH + 1;
  } else {
    return 1;
  }
}

int FmoveLowLatency() {
  if (IsFp32Mode()) {
    return Latency::MTC1;
  } else {
    return Latency::MFHC1 + Latency::MTC1 + Latency::MTHC1;
  }
}

int SebLatency() {
  if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
    return 1;
  } else {
    return 2;
  }
}

int SehLatency() {
  if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
    return 1;
  } else {
    return 2;
  }
}

int UlhuLatency() {
  if (IsMipsArchVariant(kMips32r6)) {
    return 1;
  } else {
    return 4;
  }
}

int UlhLatency() {
  if (IsMipsArchVariant(kMips32r6)) {
    return 1;
  } else {
    return 4;
  }
}

int AdjustBaseAndOffsetLatency() {
  return 3;  // Estimated max.
}

int UshLatency() {
  if (IsMipsArchVariant(kMips32r6)) {
    return 1;
  } else {
    return AdjustBaseAndOffsetLatency() + 4;  // Estimated max.
  }
}

int UlwLatency() {
  if (IsMipsArchVariant(kMips32r6)) {
    return 1;
  } else {
    return AdjustBaseAndOffsetLatency() + 3;  // Estimated max.
  }
}

int UswLatency() {
  if (IsMipsArchVariant(kMips32r6)) {
    return 1;
  } else {
    return AdjustBaseAndOffsetLatency() + 2;
  }
}

int Ulwc1Latency() {
  if (IsMipsArchVariant(kMips32r6)) {
    return Latency::LWC1;
  } else {
    return UlwLatency() + Latency::MTC1;
  }
}

int Uswc1Latency() {
  if (IsMipsArchVariant(kMips32r6)) {
    return Latency::SWC1;
  } else {
    return Latency::MFC1 + UswLatency();
  }
}

int Ldc1Latency() {
  int latency = AdjustBaseAndOffsetLatency() + Latency::LWC1;
  if (IsFp32Mode()) {
    return latency + Latency::LWC1;
  } else {
    return latency + 1 + Mthc1Latency();
  }
}

int Uldc1Latency() {
  if (IsMipsArchVariant(kMips32r6)) {
    return Ldc1Latency();
  } else {
    return 2 * UlwLatency() + Latency::MTC1 + Mthc1Latency();
  }
}

int Sdc1Latency() {
  int latency = AdjustBaseAndOffsetLatency() + Latency::SWC1;
  if (IsFp32Mode()) {
    return latency + Latency::SWC1;
  } else {
    return latency + Mfhc1Latency() + 1;
  }
}

int Usdc1Latency() {
  if (IsMipsArchVariant(kMips32r6)) {
    return Sdc1Latency();
  } else {
    return Latency::MFC1 + 2 * UswLatency() + Mfhc1Latency();
  }
}

int PushRegisterLatency() { return AdduLatency(false) + 1; }

int ByteSwapSignedLatency() {
  // operand_size == 4
  if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
    return 2;
  } else if (IsMipsArchVariant(kMips32r1) || IsMipsArchVariant(kLoongson)) {
    return 10;
  }
}

int LlLatency(int offset) {
  bool is_one_instruction =
      IsMipsArchVariant(kMips32r6) ? is_int9(offset) : is_int16(offset);
  if (is_one_instruction) {
    return 1;
  } else {
    return 3;
  }
}

int ExtractBitsLatency(int size, bool sign_extend) {
  int latency = 1 + ExtLatency();
  if (size == 8) {
    if (sign_extend) {
      return latency + SebLatency();
    } else {
      return 0;
    }
  } else if (size == 16) {
    if (sign_extend) {
      return latency + SehLatency();
    } else {
      return 0;
    }
  } else {
    UNREACHABLE();
  }
}

int NegLatency() { return 1; }

int InsertBitsLatency() {
  return RorLatency() + InsLatency() + SubuLatency(false) + NegLatency() +
         RorLatency();
}

int ScLatency(int offset) {
  bool is_one_instruction =
      IsMipsArchVariant(kMips32r6) ? is_int9(offset) : is_int16(offset);
  if (is_one_instruction) {
    return 1;
  } else {
    return 3;
  }
}

int BranchShortHelperR6Latency() {
  return 2;  // Estimated max.
}

int BranchShortHelperLatency() {
  return SltLatency() + 2;  // Estimated max.
}

int BranchShortLatency(BranchDelaySlot bdslot = PROTECT) {
  if (IsMipsArchVariant(kMips32r6) && bdslot == PROTECT) {
    return BranchShortHelperR6Latency();
  } else {
    return BranchShortHelperLatency();
  }
}

int Word32AtomicExchangeLatency(bool sign_extend, int size) {
  return AdduLatency() + 1 + SubuLatency() + 2 + LlLatency(0) +
         ExtractBitsLatency(size, sign_extend) + InsertBitsLatency() +
         ScLatency(0) + BranchShortLatency() + 1;
}

int Word32AtomicCompareExchangeLatency(bool sign_extend, int size) {
  return AdduLatency() + 1 + SubuLatency() + 2 + LlLatency(0) +
         ExtractBitsLatency(size, sign_extend) + BranchShortLatency() + 1;
}

int AddOverflowLatency() {
  return 6;  // Estimated max.
}

int SubOverflowLatency() {
  return 6;  // Estimated max.
}

int MulhLatency(bool is_operand_register = true) {
  if (is_operand_register) {
    if (!IsMipsArchVariant(kMips32r6)) {
      return Latency::MULT + Latency::MFHI;
    } else {
      return Latency::MUH;
    }
  } else {
    if (!IsMipsArchVariant(kMips32r6)) {
      return 1 + Latency::MULT + Latency::MFHI;
    } else {
      return 1 + Latency::MUH;
    }
  }
}

int MulhuLatency(bool is_operand_register = true) {
  if (is_operand_register) {
    if (!IsMipsArchVariant(kMips32r6)) {
      return Latency::MULTU + Latency::MFHI;
    } else {
      return Latency::MUHU;
    }
  } else {
    if (!IsMipsArchVariant(kMips32r6)) {
      return 1 + Latency::MULTU + Latency::MFHI;
    } else {
      return 1 + Latency::MUHU;
    }
  }
}

int MulOverflowLatency() {
  return MulLatency() + 4;  // Estimated max.
}

int ModLatency(bool is_operand_register = true) {
  if (is_operand_register) {
    if (!IsMipsArchVariant(kMips32r6)) {
      return Latency::DIV + Latency::MFHI;
    } else {
      return 1;
    }
  } else {
    if (!IsMipsArchVariant(kMips32r6)) {
      return 1 + Latency::DIV + Latency::MFHI;
    } else {
      return 2;
    }
  }
}

int ModuLatency(bool is_operand_register = true) {
  return ModLatency(is_operand_register);
}

int DivLatency(bool is_operand_register = true) {
  if (is_operand_register) {
    if (!IsMipsArchVariant(kMips32r6)) {
      return Latency::DIV + Latency::MFLO;
    } else {
      return Latency::DIV;
    }
  } else {
    if (!IsMipsArchVariant(kMips32r6)) {
      return 1 + Latency::DIV + Latency::MFLO;
    } else {
      return 1 + Latency::DIV;
    }
  }
}

int DivuLatency(bool is_operand_register = true) {
  if (is_operand_register) {
    if (!IsMipsArchVariant(kMips32r6)) {
      return Latency::DIVU + Latency::MFLO;
    } else {
      return Latency::DIVU;
    }
  } else {
    if (!IsMipsArchVariant(kMips32r6)) {
      return 1 + Latency::DIVU + Latency::MFLO;
    } else {
      return 1 + Latency::DIVU;
    }
  }
}

int CtzLatency() {
  if (IsMipsArchVariant(kMips32r6)) {
    return RorLatency(false) + 2 + ClzLatency();
  } else {
    return AdduLatency(false) + XorLatency() + AndLatency() + ClzLatency() + 1 +
           SubuLatency();
  }
}

int PopcntLatency() {
  return 4 * AndLatency() + SubuLatency() + 2 * AdduLatency() + MulLatency() +
         8;
}

int CompareFLatency() { return Latency::C_cond_S; }

int CompareIsNanFLatency() { return CompareFLatency(); }

int CompareIsNanF32Latency() { return CompareIsNanFLatency(); }

int Neg_sLatency() {
  if (IsMipsArchVariant(kMips32r6)) {
    return Latency::NEG_S;
  } else {
    // Estimated.
    return CompareIsNanF32Latency() + 2 * Latency::BRANCH + Latency::NEG_S +
           Latency::MFC1 + 1 + XorLatency() + Latency::MTC1;
  }
}

int CompareIsNanF64Latency() { return CompareIsNanFLatency(); }

int Neg_dLatency() {
  if (IsMipsArchVariant(kMips32r6)) {
    return Latency::NEG_D;
  } else {
    // Estimated.
    return CompareIsNanF64Latency() + 2 * Latency::BRANCH + Latency::NEG_D +
           Mfhc1Latency() + 1 + XorLatency() + Mthc1Latency();
  }
}

int CompareF32Latency() { return CompareFLatency(); }

int Move_sLatency() {
  return Latency::MOV_S;  // Estimated max.
}

int Float32MaxLatency() {
  // Estimated max.
  int latency = CompareIsNanF32Latency() + Latency::BRANCH;
  if (IsMipsArchVariant(kMips32r6)) {
    return latency + Latency::MAX_S;
  } else {
    return latency + 5 * Latency::BRANCH + 2 * CompareF32Latency() +
           Latency::MFC1 + Move_sLatency();
  }
}

int CompareF64Latency() { return CompareF32Latency(); }

int Move_dLatency() {
  return Latency::MOV_D;  // Estimated max.
}

int Float64MaxLatency() {
  // Estimated max.
  int latency = CompareIsNanF64Latency() + Latency::BRANCH;
  if (IsMipsArchVariant(kMips32r6)) {
    return latency + Latency::MAX_D;
  } else {
    return latency + 5 * Latency::BRANCH + 2 * CompareF64Latency() +
           Latency::MFHC1 + 2 * Move_dLatency();
  }
}

int PrepareCallCFunctionLatency() {
  int frame_alignment = TurboAssembler::ActivationFrameAlignment();
  if (frame_alignment > kSystemPointerSize) {
    return 1 + SubuLatency(false) + AndLatency(false) + 1;
  } else {
    return SubuLatency(false);
  }
}

int MovToFloatParametersLatency() { return 2 * MoveLatency(); }

int CallLatency() {
  // Estimated.
  return AdduLatency(false) + Latency::BRANCH + 3;
}

int CallCFunctionHelperLatency() {
  // Estimated.
  int latency = AndLatency(false) + Latency::BRANCH + 2 + CallLatency();
  if (base::OS::ActivationFrameAlignment() > kSystemPointerSize) {
    latency++;
  } else {
    latency += AdduLatency(false);
  }
  return latency;
}

int CallCFunctionLatency() { return 1 + CallCFunctionHelperLatency(); }

int MovFromFloatResultLatency() { return MoveLatency(); }

int Float32MinLatency() {
  // Estimated max.
  return CompareIsNanF32Latency() + Latency::BRANCH +
         2 * (CompareF32Latency() + Latency::BRANCH) + Latency::MFC1 +
         2 * Latency::BRANCH + Move_sLatency();
}

int Float64MinLatency() {
  // Estimated max.
  return CompareIsNanF64Latency() + Latency::BRANCH +
         2 * (CompareF64Latency() + Latency::BRANCH) + Mfhc1Latency() +
         2 * Latency::BRANCH + Move_dLatency();
}

int SmiUntagLatency() { return 1; }

int PrepareForTailCallLatency() {
  // Estimated max.
  return 2 * (LsaLatency() + AdduLatency(false)) + 2 + Latency::BRANCH +
         Latency::BRANCH + 2 * SubuLatency(false) + 2 + Latency::BRANCH + 1;
}

int AssemblePopArgumentsAdaptorFrameLatency() {
  return 1 + Latency::BRANCH + 1 + SmiUntagLatency() +
         PrepareForTailCallLatency();
}

int JumpLatency() {
  // Estimated max.
  return 1 + AdduLatency(false) + Latency::BRANCH + 2;
}

int AssertLatency() { return 1; }

int MultiPushLatency() {
  int latency = SubuLatency(false);
  for (int16_t i = kNumRegisters - 1; i >= 0; i--) {
    latency++;
  }
  return latency;
}

int MultiPushFPULatency() {
  int latency = SubuLatency(false);
  for (int16_t i = kNumRegisters - 1; i >= 0; i--) {
    latency += Sdc1Latency();
  }
  return latency;
}

int PushCallerSavedLatency(SaveFPRegsMode fp_mode) {
  int latency = MultiPushLatency();
  if (fp_mode == kSaveFPRegs) {
    latency += MultiPushFPULatency();
  }
  return latency;
}

int MultiPopFPULatency() {
  int latency = 0;
  for (int16_t i = 0; i < kNumRegisters; i++) {
    latency += Ldc1Latency();
  }
  return latency++;
}

int MultiPopLatency() {
  int latency = 0;
  for (int16_t i = 0; i < kNumRegisters; i++) {
    latency++;
  }
  return latency++;
}

int PopCallerSavedLatency(SaveFPRegsMode fp_mode) {
  int latency = 0;
  if (fp_mode == kSaveFPRegs) {
    latency += MultiPopFPULatency();
  }
  return latency + MultiPopLatency();
}

int AssembleArchJumpLatency() {
  // Estimated max.
  return Latency::BRANCH;
}

int AssembleArchBinarySearchSwitchLatency(int cases) {
  if (cases < CodeGenerator::kBinarySearchSwitchMinimalCases) {
    return cases * (1 + Latency::BRANCH) + AssembleArchJumpLatency();
  }
  return 1 + Latency::BRANCH + AssembleArchBinarySearchSwitchLatency(cases / 2);
}

int GenerateSwitchTableLatency() {
  int latency = 0;
  if (kArchVariant >= kMips32r6) {
    latency = LsaLatency() + 2;
  } else {
    latency = 6;
  }
  latency += 2;
  return latency;
}

int AssembleArchTableSwitchLatency() {
  return Latency::BRANCH + GenerateSwitchTableLatency();
}

int AssembleReturnLatency() {
  // Estimated max.
  return AdduLatency(false) + MultiPopLatency() + MultiPopFPULatency() +
         Latency::BRANCH + 1 + AdduLatency() + 8;
}

int TryInlineTruncateDoubleToILatency() {
  return 2 + Latency::TRUNC_W_D + Latency::MFC1 + 2 + AndLatency(false) +
         Latency::BRANCH;
}

int CallStubDelayedLatency() { return 1 + CallLatency(); }

int TruncateDoubleToIDelayedLatency() {
  // TODO(mips): This no longer reflects how TruncateDoubleToI is called.
  return TryInlineTruncateDoubleToILatency() + 1 + SubuLatency(false) +
         Sdc1Latency() + CallStubDelayedLatency() + AdduLatency(false) + 1;
}

int CheckPageFlagLatency() {
  return 2 * AndLatency(false) + 1 + Latency::BRANCH;
}

int InstructionScheduler::GetInstructionLatency(const Instruction* instr) {
  // Basic latency modeling for MIPS32 instructions. They have been determined
  // in an empirical way.
  switch (instr->arch_opcode()) {
    case kArchCallCodeObject:
    case kArchCallWasmFunction:
      return CallLatency();
    case kArchTailCallCodeObjectFromJSFunction:
    case kArchTailCallCodeObject: {
      int latency = 0;
      if (instr->arch_opcode() == kArchTailCallCodeObjectFromJSFunction) {
        latency = AssemblePopArgumentsAdaptorFrameLatency();
      }
      return latency + JumpLatency();
    }
    case kArchTailCallWasm:
    case kArchTailCallAddress:
      return JumpLatency();
    case kArchCallJSFunction: {
      int latency = 0;
      if (FLAG_debug_code) {
        latency = 1 + AssertLatency();
      }
      return latency + 1 + AdduLatency(false) + CallLatency();
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
      return 2;  // Estimated max.
    case kArchCallCFunction:
      return CallCFunctionLatency();
    case kArchJmp:
      return AssembleArchJumpLatency();
    case kArchBinarySearchSwitch:
      return AssembleArchBinarySearchSwitchLatency((instr->InputCount() - 2) /
                                                   2);
    case kArchTableSwitch:
      return AssembleArchTableSwitchLatency();
    case kArchAbortCSAAssert:
      return CallLatency() + 1;
    case kArchComment:
    case kArchDeoptimize:
      return 0;
    case kArchRet:
      return AssembleReturnLatency();
    case kArchTruncateDoubleToI:
      return TruncateDoubleToIDelayedLatency();
    case kArchStoreWithWriteBarrier:
      return AdduLatency() + 1 + CheckPageFlagLatency();
    case kArchStackSlot: {
      // Estimated max.
      return AdduLatency(false) + AndLatency(false) + AssertLatency() +
             AdduLatency(false) + AndLatency(false) + BranchShortLatency() + 1 +
             SubuLatency() + AdduLatency();
    }
    case kArchWordPoisonOnSpeculation:
      return AndLatency();
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
    case kMipsAdd:
      return AdduLatency(instr->InputAt(1)->IsRegister());
    case kMipsAnd:
      return AndLatency(instr->InputAt(1)->IsRegister());
    case kMipsOr:
      return OrLatency(instr->InputAt(1)->IsRegister());
    case kMipsXor:
      return XorLatency(instr->InputAt(1)->IsRegister());
    case kMipsSub:
      return SubuLatency(instr->InputAt(1)->IsRegister());
    case kMipsNor:
      return NorLatency(instr->InputAt(1)->IsRegister());
    case kMipsAddOvf:
      return AddOverflowLatency();
    case kMipsSubOvf:
      return SubOverflowLatency();
    case kMipsMul:
      return MulLatency(false);
    case kMipsMulHigh:
      return MulhLatency(instr->InputAt(1)->IsRegister());
    case kMipsMulHighU:
      return MulhuLatency(instr->InputAt(1)->IsRegister());
    case kMipsMulOvf:
      return MulOverflowLatency();
    case kMipsMod:
      return ModLatency(instr->InputAt(1)->IsRegister());
    case kMipsModU:
      return ModuLatency(instr->InputAt(1)->IsRegister());
    case kMipsDiv: {
      int latency = DivLatency(instr->InputAt(1)->IsRegister());
      if (IsMipsArchVariant(kMips32r6)) {
        return latency++;
      } else {
        return latency + MovzLatency();
      }
    }
    case kMipsDivU: {
      int latency = DivuLatency(instr->InputAt(1)->IsRegister());
      if (IsMipsArchVariant(kMips32r6)) {
        return latency++;
      } else {
        return latency + MovzLatency();
      }
    }
    case kMipsClz:
      return ClzLatency();
    case kMipsCtz:
      return CtzLatency();
    case kMipsPopcnt:
      return PopcntLatency();
    case kMipsShlPair: {
      if (instr->InputAt(2)->IsRegister()) {
        return ShlPairLatency();
      } else {
        return ShlPairLatency(false);
      }
    }
    case kMipsShrPair: {
      if (instr->InputAt(2)->IsRegister()) {
        return ShrPairLatency();
      } else {
        // auto immediate_operand = ImmediateOperand::cast(instr->InputAt(2));
        // return ShrPairLatency(false, immediate_operand->inline_value());
        return 1;
      }
    }
    case kMipsSarPair: {
      if (instr->InputAt(2)->IsRegister()) {
        return SarPairLatency();
      } else {
        return SarPairLatency(false);
      }
    }
    case kMipsExt:
      return ExtLatency();
    case kMipsIns:
      return InsLatency();
    case kMipsRor:
      return RorLatency(instr->InputAt(1)->IsRegister());
    case kMipsLsa:
      return LsaLatency();
    case kMipsModS:
    case kMipsModD:
      return PrepareCallCFunctionLatency() + MovToFloatParametersLatency() +
             CallCFunctionLatency() + MovFromFloatResultLatency();
    case kMipsAddPair:
      return AddPairLatency();
    case kMipsSubPair:
      return SubPairLatency();
    case kMipsMulPair:
      return MulPairLatency();
    case kMipsMaddS:
      return MaddSLatency();
    case kMipsMaddD:
      return MaddDLatency();
    case kMipsMsubS:
      return MsubSLatency();
    case kMipsMsubD:
      return MsubDLatency();
    case kMipsNegS:
      return Neg_sLatency();
    case kMipsNegD:
      return Neg_dLatency();
    case kMipsFloat64RoundDown:
    case kMipsFloat64RoundTruncate:
    case kMipsFloat64RoundUp:
    case kMipsFloat64RoundTiesEven:
      return Float64RoundLatency();
    case kMipsFloat32RoundDown:
    case kMipsFloat32RoundTruncate:
    case kMipsFloat32RoundUp:
    case kMipsFloat32RoundTiesEven:
      return Float32RoundLatency();
    case kMipsFloat32Max:
      return Float32MaxLatency();
    case kMipsFloat64Max:
      return Float64MaxLatency();
    case kMipsFloat32Min:
      return Float32MinLatency();
    case kMipsFloat64Min:
      return Float64MinLatency();
    case kMipsCvtSUw:
      return CvtSUwLatency();
    case kMipsCvtDUw:
      return CvtDUwLatency();
    case kMipsFloorWD:
      return FloorWDLatency();
    case kMipsCeilWD:
      return CeilWDLatency();
    case kMipsRoundWD:
      return RoundWDLatency();
    case kMipsTruncWD:
      return Trunc_w_dLatency() + Latency::MFC1;
    case kMipsTruncWS:
      return Latency::TRUNC_W_S + Latency::MFC1 + AdduLatency(false) +
             SltLatency() + MovnLatency();
    case kMipsTruncUwD:
      return Trunc_uw_dLatency();
    case kMipsTruncUwS:
      return Trunc_uw_sLatency() + AdduLatency(false) + MovzLatency();
    case kMipsFloat64ExtractLowWord32:
      return Latency::MFC1;
    case kMipsFloat64ExtractHighWord32:
      return Mfhc1Latency();
    case kMipsFloat64InsertLowWord32: {
      if (IsFp32Mode()) {
        return Latency::MTC1;
      } else {
        return Latency::MFHC1 + Latency::MTC1 + Latency::MTHC1;
      }
    }
    case kMipsFloat64InsertHighWord32:
      return Mthc1Latency();
    case kMipsFloat64SilenceNaN:
      return Latency::SUB_D;
    case kMipsSeb:
      return SebLatency();
    case kMipsSeh:
      return SehLatency();
    case kMipsUlhu:
      return UlhuLatency();
    case kMipsUlh:
      return UlhLatency();
    case kMipsUsh:
      return UshLatency();
    case kMipsUlw:
      return UlwLatency();
    case kMipsUsw:
      return UswLatency();
    case kMipsUlwc1:
      return Ulwc1Latency();
    case kMipsSwc1:
      return MoveLatency(false) + Latency::SWC1;  // Estimated max.
    case kMipsUswc1:
      return MoveLatency(false) + Uswc1Latency();  // Estimated max.
    case kMipsLdc1:
      return Ldc1Latency();
    case kMipsUldc1:
      return Uldc1Latency();
    case kMipsSdc1:
      return MoveLatency(false) + Sdc1Latency();  // Estimated max.
    case kMipsUsdc1:
      return MoveLatency(false) + Usdc1Latency();  // Estimated max.
    case kMipsPush: {
      if (instr->InputAt(0)->IsFPRegister()) {
        auto op = LocationOperand::cast(instr->InputAt(0));
        switch (op->representation()) {
          case MachineRepresentation::kFloat32:
            return Latency::SWC1 + SubuLatency(false);
            break;
          case MachineRepresentation::kFloat64:
            return Sdc1Latency() + SubuLatency(false);
            break;
          default: {
            UNREACHABLE();
            break;
          }
        }
      } else {
        return PushRegisterLatency();
      }
      break;
    }
    case kMipsPeek: {
      if (instr->OutputAt(0)->IsFPRegister()) {
        auto op = LocationOperand::cast(instr->OutputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          return Ldc1Latency();
        } else {
          return Latency::LWC1;
        }
      } else {
        return 1;
      }
      break;
    }
    case kMipsStackClaim:
      return SubuLatency(false);
    case kMipsStoreToStackSlot: {
      if (instr->InputAt(0)->IsFPRegister()) {
        auto op = LocationOperand::cast(instr->InputAt(0));
        if (op->representation() == MachineRepresentation::kFloat64) {
          return Sdc1Latency();
        } else if (op->representation() == MachineRepresentation::kFloat32) {
          return Latency::SWC1;
        } else {
          return 1;  // Estimated value.
        }
      } else {
        return 1;
      }
      break;
    }
    case kMipsByteSwap32:
      return ByteSwapSignedLatency();
    case kWord32AtomicLoadInt8:
    case kWord32AtomicLoadUint8:
    case kWord32AtomicLoadInt16:
    case kWord32AtomicLoadUint16:
    case kWord32AtomicLoadWord32:
      return 2;
    case kWord32AtomicStoreWord8:
    case kWord32AtomicStoreWord16:
    case kWord32AtomicStoreWord32:
      return 3;
    case kWord32AtomicExchangeInt8:
      return Word32AtomicExchangeLatency(true, 8);
    case kWord32AtomicExchangeUint8:
      return Word32AtomicExchangeLatency(false, 8);
    case kWord32AtomicExchangeInt16:
      return Word32AtomicExchangeLatency(true, 16);
    case kWord32AtomicExchangeUint16:
      return Word32AtomicExchangeLatency(false, 16);
    case kWord32AtomicExchangeWord32: {
      return 1 + AdduLatency() + Ldc1Latency() + 1 + ScLatency(0) +
             BranchShortLatency() + 1;
    }
    case kWord32AtomicCompareExchangeInt8:
      return Word32AtomicCompareExchangeLatency(true, 8);
    case kWord32AtomicCompareExchangeUint8:
      return Word32AtomicCompareExchangeLatency(false, 8);
    case kWord32AtomicCompareExchangeInt16:
      return Word32AtomicCompareExchangeLatency(true, 16);
    case kWord32AtomicCompareExchangeUint16:
      return Word32AtomicCompareExchangeLatency(false, 16);
    case kWord32AtomicCompareExchangeWord32:
      return AdduLatency() + 1 + LlLatency(0) + BranchShortLatency() + 1;
    case kMipsTst:
      return AndLatency(instr->InputAt(1)->IsRegister());
    case kMipsCmpS:
      return MoveLatency() + CompareF32Latency();
    case kMipsCmpD:
      return MoveLatency() + CompareF64Latency();
    case kArchNop:
    case kArchThrowTerminator:
    case kMipsCmp:
      return 0;
    case kArchDebugBreak:
    case kArchFramePointer:
    case kArchParentFramePointer:
    case kMipsShl:
    case kMipsShr:
    case kMipsSar:
    case kMipsMov:
    case kMipsMaxS:
    case kMipsMinS:
    case kMipsMaxD:
    case kMipsMinD:
    case kMipsLbu:
    case kMipsLb:
    case kMipsSb:
    case kMipsLhu:
    case kMipsLh:
    case kMipsSh:
    case kMipsLw:
    case kMipsSw:
    case kMipsLwc1:
      return 1;
    case kMipsAddS:
      return Latency::ADD_S;
    case kMipsSubS:
      return Latency::SUB_S;
    case kMipsMulS:
      return Latency::MUL_S;
    case kMipsAbsS:
      return Latency::ABS_S;
    case kMipsAddD:
      return Latency::ADD_D;
    case kMipsSubD:
      return Latency::SUB_D;
    case kMipsAbsD:
      return Latency::ABS_D;
    case kMipsCvtSD:
      return Latency::CVT_S_D;
    case kMipsCvtDS:
      return Latency::CVT_D_S;
    case kMipsMulD:
      return Latency::MUL_D;
    case kMipsFloorWS:
      return Latency::FLOOR_W_S;
    case kMipsCeilWS:
      return Latency::CEIL_W_S;
    case kMipsRoundWS:
      return Latency::ROUND_W_S;
    case kMipsCvtDW:
      return Latency::CVT_D_W;
    case kMipsCvtSW:
      return Latency::CVT_S_W;
    case kMipsDivS:
      return Latency::DIV_S;
    case kMipsSqrtS:
      return Latency::SQRT_S;
    case kMipsDivD:
      return Latency::DIV_D;
    case kMipsSqrtD:
      return Latency::SQRT_D;
    default:
      return 1;
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

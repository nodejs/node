// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

#define ARM64_DEFINE_FP_STATICS

#include "src/arm64/assembler-arm64-inl.h"
#include "src/arm64/instructions-arm64.h"

namespace v8 {
namespace internal {


bool Instruction::IsLoad() const {
  if (Mask(LoadStoreAnyFMask) != LoadStoreAnyFixed) {
    return false;
  }

  if (Mask(LoadStorePairAnyFMask) == LoadStorePairAnyFixed) {
    return Mask(LoadStorePairLBit) != 0;
  } else {
    LoadStoreOp op = static_cast<LoadStoreOp>(Mask(LoadStoreOpMask));
    switch (op) {
      case LDRB_w:
      case LDRH_w:
      case LDR_w:
      case LDR_x:
      case LDRSB_w:
      case LDRSB_x:
      case LDRSH_w:
      case LDRSH_x:
      case LDRSW_x:
      case LDR_s:
      case LDR_d: return true;
      default: return false;
    }
  }
}


bool Instruction::IsStore() const {
  if (Mask(LoadStoreAnyFMask) != LoadStoreAnyFixed) {
    return false;
  }

  if (Mask(LoadStorePairAnyFMask) == LoadStorePairAnyFixed) {
    return Mask(LoadStorePairLBit) == 0;
  } else {
    LoadStoreOp op = static_cast<LoadStoreOp>(Mask(LoadStoreOpMask));
    switch (op) {
      case STRB_w:
      case STRH_w:
      case STR_w:
      case STR_x:
      case STR_s:
      case STR_d: return true;
      default: return false;
    }
  }
}


static uint64_t RotateRight(uint64_t value,
                            unsigned int rotate,
                            unsigned int width) {
  DCHECK(width <= 64);
  rotate &= 63;
  return ((value & ((1UL << rotate) - 1UL)) << (width - rotate)) |
         (value >> rotate);
}


static uint64_t RepeatBitsAcrossReg(unsigned reg_size,
                                    uint64_t value,
                                    unsigned width) {
  DCHECK((width == 2) || (width == 4) || (width == 8) || (width == 16) ||
         (width == 32));
  DCHECK((reg_size == kWRegSizeInBits) || (reg_size == kXRegSizeInBits));
  uint64_t result = value & ((1UL << width) - 1UL);
  for (unsigned i = width; i < reg_size; i *= 2) {
    result |= (result << i);
  }
  return result;
}


// Logical immediates can't encode zero, so a return value of zero is used to
// indicate a failure case. Specifically, where the constraints on imm_s are not
// met.
uint64_t Instruction::ImmLogical() {
  unsigned reg_size = SixtyFourBits() ? kXRegSizeInBits : kWRegSizeInBits;
  int32_t n = BitN();
  int32_t imm_s = ImmSetBits();
  int32_t imm_r = ImmRotate();

  // An integer is constructed from the n, imm_s and imm_r bits according to
  // the following table:
  //
  //  N   imms    immr    size        S             R
  //  1  ssssss  rrrrrr    64    UInt(ssssss)  UInt(rrrrrr)
  //  0  0sssss  xrrrrr    32    UInt(sssss)   UInt(rrrrr)
  //  0  10ssss  xxrrrr    16    UInt(ssss)    UInt(rrrr)
  //  0  110sss  xxxrrr     8    UInt(sss)     UInt(rrr)
  //  0  1110ss  xxxxrr     4    UInt(ss)      UInt(rr)
  //  0  11110s  xxxxxr     2    UInt(s)       UInt(r)
  // (s bits must not be all set)
  //
  // A pattern is constructed of size bits, where the least significant S+1
  // bits are set. The pattern is rotated right by R, and repeated across a
  // 32 or 64-bit value, depending on destination register width.
  //

  if (n == 1) {
    if (imm_s == 0x3F) {
      return 0;
    }
    uint64_t bits = (1UL << (imm_s + 1)) - 1;
    return RotateRight(bits, imm_r, 64);
  } else {
    if ((imm_s >> 1) == 0x1F) {
      return 0;
    }
    for (int width = 0x20; width >= 0x2; width >>= 1) {
      if ((imm_s & width) == 0) {
        int mask = width - 1;
        if ((imm_s & mask) == mask) {
          return 0;
        }
        uint64_t bits = (1UL << ((imm_s & mask) + 1)) - 1;
        return RepeatBitsAcrossReg(reg_size,
                                   RotateRight(bits, imm_r & mask, width),
                                   width);
      }
    }
  }
  UNREACHABLE();
  return 0;
}


float Instruction::ImmFP32() {
  //  ImmFP: abcdefgh (8 bits)
  // Single: aBbb.bbbc.defg.h000.0000.0000.0000.0000 (32 bits)
  // where B is b ^ 1
  uint32_t bits = ImmFP();
  uint32_t bit7 = (bits >> 7) & 0x1;
  uint32_t bit6 = (bits >> 6) & 0x1;
  uint32_t bit5_to_0 = bits & 0x3f;
  uint32_t result = (bit7 << 31) | ((32 - bit6) << 25) | (bit5_to_0 << 19);

  return rawbits_to_float(result);
}


double Instruction::ImmFP64() {
  //  ImmFP: abcdefgh (8 bits)
  // Double: aBbb.bbbb.bbcd.efgh.0000.0000.0000.0000
  //         0000.0000.0000.0000.0000.0000.0000.0000 (64 bits)
  // where B is b ^ 1
  uint32_t bits = ImmFP();
  uint64_t bit7 = (bits >> 7) & 0x1;
  uint64_t bit6 = (bits >> 6) & 0x1;
  uint64_t bit5_to_0 = bits & 0x3f;
  uint64_t result = (bit7 << 63) | ((256 - bit6) << 54) | (bit5_to_0 << 48);

  return rawbits_to_double(result);
}


LSDataSize CalcLSPairDataSize(LoadStorePairOp op) {
  switch (op) {
    case STP_x:
    case LDP_x:
    case STP_d:
    case LDP_d: return LSDoubleWord;
    default: return LSWord;
  }
}


int64_t Instruction::ImmPCOffset() {
  int64_t offset;
  if (IsPCRelAddressing()) {
    // PC-relative addressing. Only ADR is supported.
    offset = ImmPCRel();
  } else if (BranchType() != UnknownBranchType) {
    // All PC-relative branches.
    // Relative branch offsets are instruction-size-aligned.
    offset = ImmBranch() << kInstructionSizeLog2;
  } else if (IsUnresolvedInternalReference()) {
    // Internal references are always word-aligned.
    offset = ImmUnresolvedInternalReference() << kInstructionSizeLog2;
  } else {
    // Load literal (offset from PC).
    DCHECK(IsLdrLiteral());
    // The offset is always shifted by 2 bits, even for loads to 64-bits
    // registers.
    offset = ImmLLiteral() << kInstructionSizeLog2;
  }
  return offset;
}


Instruction* Instruction::ImmPCOffsetTarget() {
  return InstructionAtOffset(ImmPCOffset());
}


bool Instruction::IsValidImmPCOffset(ImmBranchType branch_type,
                                     ptrdiff_t offset) {
  return is_intn(offset, ImmBranchRangeBitwidth(branch_type));
}


bool Instruction::IsTargetInImmPCOffsetRange(Instruction* target) {
  return IsValidImmPCOffset(BranchType(), DistanceTo(target));
}


void Instruction::SetImmPCOffsetTarget(Isolate* isolate, Instruction* target) {
  if (IsPCRelAddressing()) {
    SetPCRelImmTarget(isolate, target);
  } else if (BranchType() != UnknownBranchType) {
    SetBranchImmTarget(target);
  } else if (IsUnresolvedInternalReference()) {
    SetUnresolvedInternalReferenceImmTarget(isolate, target);
  } else {
    // Load literal (offset from PC).
    SetImmLLiteral(target);
  }
}


void Instruction::SetPCRelImmTarget(Isolate* isolate, Instruction* target) {
  // ADRP is not supported, so 'this' must point to an ADR instruction.
  DCHECK(IsAdr());

  ptrdiff_t target_offset = DistanceTo(target);
  Instr imm;
  if (Instruction::IsValidPCRelOffset(target_offset)) {
    imm = Assembler::ImmPCRelAddress(static_cast<int>(target_offset));
    SetInstructionBits(Mask(~ImmPCRel_mask) | imm);
  } else {
    PatchingAssembler patcher(isolate, this,
                              PatchingAssembler::kAdrFarPatchableNInstrs);
    patcher.PatchAdrFar(target_offset);
  }
}


void Instruction::SetBranchImmTarget(Instruction* target) {
  DCHECK(IsAligned(DistanceTo(target), kInstructionSize));
  DCHECK(IsValidImmPCOffset(BranchType(),
                            DistanceTo(target) >> kInstructionSizeLog2));
  int offset = static_cast<int>(DistanceTo(target) >> kInstructionSizeLog2);
  Instr branch_imm = 0;
  uint32_t imm_mask = 0;
  switch (BranchType()) {
    case CondBranchType: {
      branch_imm = Assembler::ImmCondBranch(offset);
      imm_mask = ImmCondBranch_mask;
      break;
    }
    case UncondBranchType: {
      branch_imm = Assembler::ImmUncondBranch(offset);
      imm_mask = ImmUncondBranch_mask;
      break;
    }
    case CompareBranchType: {
      branch_imm = Assembler::ImmCmpBranch(offset);
      imm_mask = ImmCmpBranch_mask;
      break;
    }
    case TestBranchType: {
      branch_imm = Assembler::ImmTestBranch(offset);
      imm_mask = ImmTestBranch_mask;
      break;
    }
    default: UNREACHABLE();
  }
  SetInstructionBits(Mask(~imm_mask) | branch_imm);
}


void Instruction::SetUnresolvedInternalReferenceImmTarget(Isolate* isolate,
                                                          Instruction* target) {
  DCHECK(IsUnresolvedInternalReference());
  DCHECK(IsAligned(DistanceTo(target), kInstructionSize));
  DCHECK(is_int32(DistanceTo(target) >> kInstructionSizeLog2));
  int32_t target_offset =
      static_cast<int32_t>(DistanceTo(target) >> kInstructionSizeLog2);
  uint32_t high16 = unsigned_bitextract_32(31, 16, target_offset);
  uint32_t low16 = unsigned_bitextract_32(15, 0, target_offset);

  PatchingAssembler patcher(isolate, this, 2);
  patcher.brk(high16);
  patcher.brk(low16);
}


void Instruction::SetImmLLiteral(Instruction* source) {
  DCHECK(IsLdrLiteral());
  DCHECK(IsAligned(DistanceTo(source), kInstructionSize));
  DCHECK(Assembler::IsImmLLiteral(DistanceTo(source)));
  Instr imm = Assembler::ImmLLiteral(
      static_cast<int>(DistanceTo(source) >> kLoadLiteralScaleLog2));
  Instr mask = ImmLLiteral_mask;

  SetInstructionBits(Mask(~mask) | imm);
}


// TODO(jbramley): We can't put this inline in the class because things like
// xzr and Register are not defined in that header. Consider adding
// instructions-arm64-inl.h to work around this.
bool InstructionSequence::IsInlineData() const {
  // Inline data is encoded as a single movz instruction which writes to xzr
  // (x31).
  return IsMovz() && SixtyFourBits() && (Rd() == kZeroRegCode);
  // TODO(all): If we extend ::InlineData() to support bigger data, we need
  // to update this method too.
}


// TODO(jbramley): We can't put this inline in the class because things like
// xzr and Register are not defined in that header. Consider adding
// instructions-arm64-inl.h to work around this.
uint64_t InstructionSequence::InlineData() const {
  DCHECK(IsInlineData());
  uint64_t payload = ImmMoveWide();
  // TODO(all): If we extend ::InlineData() to support bigger data, we need
  // to update this method too.
  return payload;
}


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM64

// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

#include "src/crankshaft/arm64/delayed-masm-arm64.h"
#include "src/crankshaft/arm64/lithium-codegen-arm64.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm_)


void DelayedMasm::StackSlotMove(LOperand* src, LOperand* dst) {
  DCHECK((src->IsStackSlot() && dst->IsStackSlot()) ||
         (src->IsDoubleStackSlot() && dst->IsDoubleStackSlot()));
  MemOperand src_operand = cgen_->ToMemOperand(src);
  MemOperand dst_operand = cgen_->ToMemOperand(dst);
  if (pending_ == kStackSlotMove) {
    DCHECK(pending_pc_ == masm_->pc_offset());
    UseScratchRegisterScope scope(masm_);
    DoubleRegister temp1 = scope.AcquireD();
    DoubleRegister temp2 = scope.AcquireD();
    switch (MemOperand::AreConsistentForPair(pending_address_src_,
                                             src_operand)) {
      case MemOperand::kNotPair:
        __ Ldr(temp1, pending_address_src_);
        __ Ldr(temp2, src_operand);
        break;
      case MemOperand::kPairAB:
        __ Ldp(temp1, temp2, pending_address_src_);
        break;
      case MemOperand::kPairBA:
        __ Ldp(temp2, temp1, src_operand);
        break;
    }
    switch (MemOperand::AreConsistentForPair(pending_address_dst_,
                                             dst_operand)) {
      case MemOperand::kNotPair:
        __ Str(temp1, pending_address_dst_);
        __ Str(temp2, dst_operand);
        break;
      case MemOperand::kPairAB:
        __ Stp(temp1, temp2, pending_address_dst_);
        break;
      case MemOperand::kPairBA:
        __ Stp(temp2, temp1, dst_operand);
        break;
    }
    ResetPending();
    return;
  }

  EmitPending();
  pending_ = kStackSlotMove;
  pending_address_src_ = src_operand;
  pending_address_dst_ = dst_operand;
#ifdef DEBUG
  pending_pc_ = masm_->pc_offset();
#endif
}


void DelayedMasm::StoreConstant(uint64_t value, const MemOperand& operand) {
  DCHECK(!scratch_register_acquired_);
  if ((pending_ == kStoreConstant) && (value == pending_value_)) {
    MemOperand::PairResult result =
        MemOperand::AreConsistentForPair(pending_address_dst_, operand);
    if (result != MemOperand::kNotPair) {
      const MemOperand& dst =
          (result == MemOperand::kPairAB) ?
              pending_address_dst_ :
              operand;
      DCHECK(pending_pc_ == masm_->pc_offset());
      if (pending_value_ == 0) {
        __ Stp(xzr, xzr, dst);
      } else {
        SetSavedValue(pending_value_);
        __ Stp(ScratchRegister(), ScratchRegister(), dst);
      }
      ResetPending();
      return;
    }
  }

  EmitPending();
  pending_ = kStoreConstant;
  pending_address_dst_ = operand;
  pending_value_ = value;
#ifdef DEBUG
  pending_pc_ = masm_->pc_offset();
#endif
}


void DelayedMasm::Load(const CPURegister& rd, const MemOperand& operand) {
  if ((pending_ == kLoad) &&
      pending_register_.IsSameSizeAndType(rd)) {
    switch (MemOperand::AreConsistentForPair(pending_address_src_, operand)) {
      case MemOperand::kNotPair:
        break;
      case MemOperand::kPairAB:
        DCHECK(pending_pc_ == masm_->pc_offset());
        DCHECK(!IsScratchRegister(pending_register_) ||
               scratch_register_acquired_);
        DCHECK(!IsScratchRegister(rd) || scratch_register_acquired_);
        __ Ldp(pending_register_, rd, pending_address_src_);
        ResetPending();
        return;
      case MemOperand::kPairBA:
        DCHECK(pending_pc_ == masm_->pc_offset());
        DCHECK(!IsScratchRegister(pending_register_) ||
               scratch_register_acquired_);
        DCHECK(!IsScratchRegister(rd) || scratch_register_acquired_);
        __ Ldp(rd, pending_register_, operand);
        ResetPending();
        return;
    }
  }

  EmitPending();
  pending_ = kLoad;
  pending_register_ = rd;
  pending_address_src_ = operand;
#ifdef DEBUG
  pending_pc_ = masm_->pc_offset();
#endif
}


void DelayedMasm::Store(const CPURegister& rd, const MemOperand& operand) {
  if ((pending_ == kStore) &&
      pending_register_.IsSameSizeAndType(rd)) {
    switch (MemOperand::AreConsistentForPair(pending_address_dst_, operand)) {
      case MemOperand::kNotPair:
        break;
      case MemOperand::kPairAB:
        DCHECK(pending_pc_ == masm_->pc_offset());
        __ Stp(pending_register_, rd, pending_address_dst_);
        ResetPending();
        return;
      case MemOperand::kPairBA:
        DCHECK(pending_pc_ == masm_->pc_offset());
        __ Stp(rd, pending_register_, operand);
        ResetPending();
        return;
    }
  }

  EmitPending();
  pending_ = kStore;
  pending_register_ = rd;
  pending_address_dst_ = operand;
#ifdef DEBUG
  pending_pc_ = masm_->pc_offset();
#endif
}


void DelayedMasm::EmitPending() {
  DCHECK((pending_ == kNone) || (pending_pc_ == masm_->pc_offset()));
  switch (pending_) {
    case kNone:
      return;
    case kStoreConstant:
      if (pending_value_ == 0) {
        __ Str(xzr, pending_address_dst_);
      } else {
        SetSavedValue(pending_value_);
        __ Str(ScratchRegister(), pending_address_dst_);
      }
      break;
    case kLoad:
      DCHECK(!IsScratchRegister(pending_register_) ||
              scratch_register_acquired_);
      __ Ldr(pending_register_, pending_address_src_);
      break;
    case kStore:
      __ Str(pending_register_, pending_address_dst_);
      break;
    case kStackSlotMove: {
      UseScratchRegisterScope scope(masm_);
      DoubleRegister temp = scope.AcquireD();
      __ Ldr(temp, pending_address_src_);
      __ Str(temp, pending_address_dst_);
      break;
    }
  }
  ResetPending();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM64

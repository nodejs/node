// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_ARM64_DELAYED_MASM_ARM64_INL_H_
#define V8_CRANKSHAFT_ARM64_DELAYED_MASM_ARM64_INL_H_

#include "src/arm64/macro-assembler-arm64-inl.h"
#include "src/crankshaft/arm64/delayed-masm-arm64.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm_)

DelayedMasm::DelayedMasm(LCodeGen* owner, MacroAssembler* masm,
                         const Register& scratch_register)
    : cgen_(owner),
      masm_(masm),
      scratch_register_(scratch_register),
      scratch_register_used_(false),
      pending_(kNone),
      saved_value_(0) {
#ifdef DEBUG
  pending_register_ = no_reg;
  pending_value_ = 0;
  pending_pc_ = 0;
  scratch_register_acquired_ = false;
#endif
}

void DelayedMasm::EndDelayedUse() {
  EmitPending();
  DCHECK(!scratch_register_acquired_);
  ResetSavedValue();
}


void DelayedMasm::Mov(const Register& rd,
                      const Operand& operand,
                      DiscardMoveMode discard_mode) {
  EmitPending();
  DCHECK(!IsScratchRegister(rd) || scratch_register_acquired_);
  __ Mov(rd, operand, discard_mode);
}


void DelayedMasm::Fmov(FPRegister fd, FPRegister fn) {
  EmitPending();
  __ Fmov(fd, fn);
}


void DelayedMasm::Fmov(FPRegister fd, double imm) {
  EmitPending();
  __ Fmov(fd, imm);
}


void DelayedMasm::LoadObject(Register result, Handle<Object> object) {
  EmitPending();
  DCHECK(!IsScratchRegister(result) || scratch_register_acquired_);
  __ LoadObject(result, object);
}

void DelayedMasm::InitializeRootRegister() { masm_->InitializeRootRegister(); }

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_ARM64_DELAYED_MASM_ARM64_INL_H_

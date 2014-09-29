// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_DELAYED_MASM_ARM64_INL_H_
#define V8_ARM64_DELAYED_MASM_ARM64_INL_H_

#include "src/arm64/delayed-masm-arm64.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm_)


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


#undef __

} }  // namespace v8::internal

#endif  // V8_ARM64_DELAYED_MASM_ARM64_INL_H_

// Copyright 2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "codegen-inl.h"
#include "register-allocator-inl.h"

namespace v8 { namespace internal {

// -------------------------------------------------------------------------
// Result implementation.

Result::Result(Register reg, CodeGenerator* cgen)
    : static_type_(),
      type_(REGISTER),
      cgen_(cgen) {
  data_.reg_ = reg;
  ASSERT(reg.is_valid());
  cgen_->allocator()->Use(reg);
}


Result::Result(Register reg, CodeGenerator* cgen, StaticType static_type)
    : static_type_(static_type),
      type_(REGISTER),
      cgen_(cgen) {
  data_.reg_ = reg;
  ASSERT(reg.is_valid());
  cgen_->allocator()->Use(reg);
}


void Result::CopyTo(Result* destination) const {
  destination->static_type_ = static_type_;
  destination->type_ = type();
  destination->cgen_ = cgen_;

  if (is_register()) {
    destination->data_.reg_ = reg();
    cgen_->allocator()->Use(reg());
  } else if (is_constant()) {
    destination->data_.handle_ = data_.handle_;
  } else {
    ASSERT(!is_valid());
  }
}


// -------------------------------------------------------------------------
// RegisterFile implementation.

void RegisterFile::CopyTo(RegisterFile* other) {
  for (int i = 0; i < kNumRegisters; i++) {
    other->ref_counts_[i] = ref_counts_[i];
  }
}


// -------------------------------------------------------------------------
// RegisterAllocator implementation.


Result RegisterAllocator::AllocateWithoutSpilling() {
  // Return the first free register, if any.
  int free_reg = registers_.ScanForFreeRegister();
  if (free_reg < kNumRegisters) {
    Register free_result = { free_reg };
    return Result(free_result, cgen_);
  }
  return Result(cgen_);
}


Result RegisterAllocator::Allocate() {
  Result result = AllocateWithoutSpilling();
  if (!result.is_valid()) {
    // Ask the current frame to spill a register.
    ASSERT(cgen_->has_valid_frame());
    Register free_reg = cgen_->frame()->SpillAnyRegister();
    if (free_reg.is_valid()) {
      ASSERT(!is_used(free_reg));
      return Result(free_reg, cgen_);
    }
  }
  return result;
}


Result RegisterAllocator::Allocate(Register target) {
  // If the target is not referenced, it can simply be allocated.
  if (!is_used(target)) {
    return Result(target, cgen_);
  }
  // If the target is only referenced in the frame, it can be spilled and
  // then allocated.
  ASSERT(cgen_->has_valid_frame());
  if (cgen_->frame()->is_used(target) && count(target) == 1)  {
    cgen_->frame()->Spill(target);
    ASSERT(!is_used(target));
    return Result(target, cgen_);
  }
  // Otherwise (if it's referenced outside the frame) we cannot allocate it.
  return Result(cgen_);
}


} }  // namespace v8::internal

// Copyright 2008 the V8 project authors. All rights reserved.
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

void Result::ToRegister() {
  ASSERT(is_valid());
  if (is_constant()) {
    Result fresh = cgen_->allocator()->Allocate();
    ASSERT(fresh.is_valid());
    if (cgen_->IsUnsafeSmi(handle())) {
      cgen_->LoadUnsafeSmi(fresh.reg(), handle());
    } else {
      cgen_->masm()->Set(fresh.reg(), Immediate(handle()));
    }
    // This result becomes a copy of the fresh one.
    *this = fresh;
  }
  ASSERT(is_register());
}


void Result::ToRegister(Register target) {
  ASSERT(is_valid());
  if (!is_register() || !reg().is(target)) {
    Result fresh = cgen_->allocator()->Allocate(target);
    ASSERT(fresh.is_valid());
    if (is_register()) {
      cgen_->masm()->mov(fresh.reg(), reg());
    } else {
      ASSERT(is_constant());
      if (cgen_->IsUnsafeSmi(handle())) {
        cgen_->LoadUnsafeSmi(fresh.reg(), handle());
      } else {
        cgen_->masm()->Set(fresh.reg(), Immediate(handle()));
      }
    }
    *this = fresh;
  } else if (is_register() && reg().is(target)) {
    ASSERT(cgen_->has_valid_frame());
    cgen_->frame()->Spill(target);
    ASSERT(cgen_->allocator()->count(target) == 1);
  }
  ASSERT(is_register());
  ASSERT(reg().is(target));
}


// -------------------------------------------------------------------------
// RegisterAllocator implementation.

RegisterFile RegisterAllocator::Reserved() {
  RegisterFile reserved;
  reserved.Use(esp);
  reserved.Use(ebp);
  reserved.Use(esi);
  return reserved;
}


void RegisterAllocator::UnuseReserved(RegisterFile* register_file) {
  register_file->ref_counts_[esp.code()] = 0;
  register_file->ref_counts_[ebp.code()] = 0;
  register_file->ref_counts_[esi.code()] = 0;
}


void RegisterAllocator::Initialize() {
  Reset();
  // The following register is live on function entry, saved in the
  // frame, and available for allocation during execution.
  Use(edi);  // JS function.
}


void RegisterAllocator::Reset() {
  registers_.Reset();
  // The following registers are live on function entry and reserved
  // during execution.
  Use(esp);  // Stack pointer.
  Use(ebp);  // Frame pointer (caller's frame pointer on entry).
  Use(esi);  // Context (callee's context on entry).
}


Result RegisterAllocator::AllocateByteRegisterWithoutSpilling() {
  Result result = AllocateWithoutSpilling();
  // Check that the register is a byte register.  If not, unuse the
  // register if valid and return an invalid result.
  if (result.is_valid() && !result.reg().is_byte_register()) {
    result.Unuse();
    return Result(cgen_);
  }
  return result;
}


} }  // namespace v8::internal

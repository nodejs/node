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

void Result::ToRegister() {
  UNIMPLEMENTED();
}


void Result::ToRegister(Register target) {
  UNIMPLEMENTED();
}


// -------------------------------------------------------------------------
// RegisterAllocator implementation.

RegisterFile RegisterAllocator::Reserved() {
  RegisterFile reserved;
  reserved.Use(sp);
  reserved.Use(fp);
  reserved.Use(cp);
  reserved.Use(pc);
  return reserved;
}


void RegisterAllocator::UnuseReserved(RegisterFile* register_file) {
  register_file->ref_counts_[sp.code()] = 0;
  register_file->ref_counts_[fp.code()] = 0;
  register_file->ref_counts_[cp.code()] = 0;
  register_file->ref_counts_[pc.code()] = 0;
}


void RegisterAllocator::Initialize() {
  Reset();
  // The following registers are live on function entry, saved in the
  // frame, and available for allocation during execution.
  Use(r1);  // JS function.
  Use(lr);  // Return address.
}


void RegisterAllocator::Reset() {
  registers_.Reset();
  // The following registers are live on function entry and reserved
  // during execution.
  Use(sp);  // Stack pointer.
  Use(fp);  // Frame pointer (caller's frame pointer on entry).
  Use(cp);  // Context context (callee's context on entry).
  Use(pc);  // Program counter.
}


Result RegisterAllocator::AllocateByteRegisterWithoutSpilling() {
  UNIMPLEMENTED();
  Result invalid(cgen_);
  return invalid;
}


} }  // namespace v8::internal

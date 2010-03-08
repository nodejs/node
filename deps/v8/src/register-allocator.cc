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
#include "virtual-frame-inl.h"

namespace v8 {
namespace internal {

// -------------------------------------------------------------------------
// Result implementation.


Result::Result(Register reg, NumberInfo::Type info) {
  ASSERT(reg.is_valid() && !RegisterAllocator::IsReserved(reg));
  CodeGeneratorScope::Current()->allocator()->Use(reg);
  value_ = TypeField::encode(REGISTER)
      | NumberInfoField::encode(info)
      | DataField::encode(reg.code_);
}


Result::ZoneObjectList* Result::ConstantList() {
  static ZoneObjectList list(10);
  return &list;
}


NumberInfo::Type Result::number_info() {
  ASSERT(is_valid());
  if (!is_constant()) return NumberInfoField::decode(value_);
  Handle<Object> value = handle();
  if (value->IsSmi()) return NumberInfo::kSmi;
  if (value->IsHeapNumber()) return NumberInfo::kHeapNumber;
  return NumberInfo::kUnknown;
}


void Result::set_number_info(NumberInfo::Type info) {
  ASSERT(is_valid());
  value_ = value_ & ~NumberInfoField::mask();
  value_ = value_ | NumberInfoField::encode(info);
}


// -------------------------------------------------------------------------
// RegisterAllocator implementation.


Result RegisterAllocator::AllocateWithoutSpilling() {
  // Return the first free register, if any.
  int num = registers_.ScanForFreeRegister();
  if (num == RegisterAllocator::kInvalidRegister) {
    return Result();
  }
  return Result(RegisterAllocator::ToRegister(num));
}


Result RegisterAllocator::Allocate() {
  Result result = AllocateWithoutSpilling();
  if (!result.is_valid()) {
    // Ask the current frame to spill a register.
    ASSERT(cgen_->has_valid_frame());
    Register free_reg = cgen_->frame()->SpillAnyRegister();
    if (free_reg.is_valid()) {
      ASSERT(!is_used(free_reg));
      return Result(free_reg);
    }
  }
  return result;
}


Result RegisterAllocator::Allocate(Register target) {
  // If the target is not referenced, it can simply be allocated.
  if (!is_used(target)) {
    return Result(target);
  }
  // If the target is only referenced in the frame, it can be spilled and
  // then allocated.
  ASSERT(cgen_->has_valid_frame());
  if (cgen_->frame()->is_used(target) && count(target) == 1)  {
    cgen_->frame()->Spill(target);
    ASSERT(!is_used(target));
    return Result(target);
  }
  // Otherwise (if it's referenced outside the frame) we cannot allocate it.
  return Result();
}


} }  // namespace v8::internal

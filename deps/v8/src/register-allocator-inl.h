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

#ifndef V8_REGISTER_ALLOCATOR_INL_H_
#define V8_REGISTER_ALLOCATOR_INL_H_

#include "codegen.h"
#include "register-allocator.h"

#if V8_TARGET_ARCH_IA32
#include "ia32/register-allocator-ia32-inl.h"
#elif V8_TARGET_ARCH_X64
#include "x64/register-allocator-x64-inl.h"
#elif V8_TARGET_ARCH_ARM
#include "arm/register-allocator-arm-inl.h"
#elif V8_TARGET_ARCH_MIPS
#include "mips/register-allocator-mips-inl.h"
#else
#error Unsupported target architecture.
#endif


namespace v8 {
namespace internal {

Result::Result(const Result& other) {
  other.CopyTo(this);
}


Result& Result::operator=(const Result& other) {
  if (this != &other) {
    Unuse();
    other.CopyTo(this);
  }
  return *this;
}


Result::~Result() {
  if (is_register()) {
    CodeGeneratorScope::Current()->allocator()->Unuse(reg());
  }
}


void Result::Unuse() {
  if (is_register()) {
    CodeGeneratorScope::Current()->allocator()->Unuse(reg());
  }
  invalidate();
}


void Result::CopyTo(Result* destination) const {
  destination->value_ = value_;
  if (is_register()) {
    CodeGeneratorScope::Current()->allocator()->Use(reg());
  }
}


bool RegisterAllocator::is_used(Register reg) {
  return registers_.is_used(ToNumber(reg));
}


int RegisterAllocator::count(Register reg) {
  return registers_.count(ToNumber(reg));
}


void RegisterAllocator::Use(Register reg) {
  registers_.Use(ToNumber(reg));
}


void RegisterAllocator::Unuse(Register reg) {
  registers_.Unuse(ToNumber(reg));
}


TypeInfo Result::type_info() const {
  ASSERT(is_valid());
  return TypeInfo::FromInt(TypeInfoField::decode(value_));
}


void Result::set_type_info(TypeInfo info) {
  ASSERT(is_valid());
  value_ &= ~TypeInfoField::mask();
  value_ |= TypeInfoField::encode(info.ToInt());
}


bool Result::is_number() const {
  return type_info().IsNumber();
}


bool Result::is_smi() const {
  return type_info().IsSmi();
}


bool Result::is_integer32() const {
  return type_info().IsInteger32();
}


bool Result::is_double() const {
  return type_info().IsDouble();
}

} }  // namespace v8::internal

#endif  // V8_REGISTER_ALLOCATOR_INL_H_

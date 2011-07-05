// Copyright 2010 the V8 project authors. All rights reserved.
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

#ifndef V8_LITHIUM_ALLOCATOR_INL_H_
#define V8_LITHIUM_ALLOCATOR_INL_H_

#include "lithium-allocator.h"

#if V8_TARGET_ARCH_IA32
#include "ia32/lithium-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "x64/lithium-x64.h"
#elif V8_TARGET_ARCH_ARM
#include "arm/lithium-arm.h"
#else
#error "Unknown architecture."
#endif

namespace v8 {
namespace internal {

bool LAllocator::IsGapAt(int index) { return chunk_->IsGapAt(index); }


LInstruction* LAllocator::InstructionAt(int index) {
  return chunk_->instructions()->at(index);
}


LGap* LAllocator::GapAt(int index) {
  return chunk_->GetGapAt(index);
}


TempIterator::TempIterator(LInstruction* instr)
    : instr_(instr),
      limit_(instr->TempCount()),
      current_(0) {
  current_ = AdvanceToNext(0);
}


bool TempIterator::HasNext() { return current_ < limit_; }


LOperand* TempIterator::Next() {
  ASSERT(HasNext());
  return instr_->TempAt(current_);
}


int TempIterator::AdvanceToNext(int start) {
  while (start < limit_ && instr_->TempAt(start) == NULL) start++;
  return start;
}


void TempIterator::Advance() {
  current_ = AdvanceToNext(current_ + 1);
}


InputIterator::InputIterator(LInstruction* instr)
    : instr_(instr),
      limit_(instr->InputCount()),
      current_(0) {
  current_ = AdvanceToNext(0);
}


bool InputIterator::HasNext() { return current_ < limit_; }


LOperand* InputIterator::Next() {
  ASSERT(HasNext());
  return instr_->InputAt(current_);
}


void InputIterator::Advance() {
  current_ = AdvanceToNext(current_ + 1);
}


int InputIterator::AdvanceToNext(int start) {
  while (start < limit_ && instr_->InputAt(start)->IsConstantOperand()) start++;
  return start;
}


UseIterator::UseIterator(LInstruction* instr)
    : input_iterator_(instr), env_iterator_(instr->environment()) { }


bool UseIterator::HasNext() {
  return input_iterator_.HasNext() || env_iterator_.HasNext();
}


LOperand* UseIterator::Next() {
  ASSERT(HasNext());
  return input_iterator_.HasNext()
      ? input_iterator_.Next()
      : env_iterator_.Next();
}


void UseIterator::Advance() {
  input_iterator_.HasNext()
      ? input_iterator_.Advance()
      : env_iterator_.Advance();
}

} }  // namespace v8::internal

#endif  // V8_LITHIUM_ALLOCATOR_INL_H_

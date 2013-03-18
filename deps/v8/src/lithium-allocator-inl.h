// Copyright 2011 the V8 project authors. All rights reserved.
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
#elif V8_TARGET_ARCH_MIPS
#include "mips/lithium-mips.h"
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
  SkipUninteresting();
}


bool TempIterator::Done() { return current_ >= limit_; }


LOperand* TempIterator::Current() {
  ASSERT(!Done());
  return instr_->TempAt(current_);
}


void TempIterator::SkipUninteresting() {
  while (current_ < limit_ && instr_->TempAt(current_) == NULL) ++current_;
}


void TempIterator::Advance() {
  ++current_;
  SkipUninteresting();
}


InputIterator::InputIterator(LInstruction* instr)
    : instr_(instr),
      limit_(instr->InputCount()),
      current_(0) {
  SkipUninteresting();
}


bool InputIterator::Done() { return current_ >= limit_; }


LOperand* InputIterator::Current() {
  ASSERT(!Done());
  ASSERT(instr_->InputAt(current_) != NULL);
  return instr_->InputAt(current_);
}


void InputIterator::Advance() {
  ++current_;
  SkipUninteresting();
}


void InputIterator::SkipUninteresting() {
  while (current_ < limit_) {
    LOperand* current = instr_->InputAt(current_);
    if (current != NULL && !current->IsConstantOperand()) break;
    ++current_;
  }
}


UseIterator::UseIterator(LInstruction* instr)
    : input_iterator_(instr), env_iterator_(instr->environment()) { }


bool UseIterator::Done() {
  return input_iterator_.Done() && env_iterator_.Done();
}


LOperand* UseIterator::Current() {
  ASSERT(!Done());
  LOperand* result = input_iterator_.Done()
      ? env_iterator_.Current()
      : input_iterator_.Current();
  ASSERT(result != NULL);
  return result;
}


void UseIterator::Advance() {
  input_iterator_.Done()
      ? env_iterator_.Advance()
      : input_iterator_.Advance();
}


void LAllocator::SetLiveRangeAssignedRegister(
    LiveRange* range,
    int reg,
    RegisterKind register_kind,
    Zone* zone) {
  if (register_kind == DOUBLE_REGISTERS) {
    assigned_double_registers_->Add(reg);
  } else {
    assigned_registers_->Add(reg);
  }
  range->set_assigned_register(reg, register_kind, zone);
}


} }  // namespace v8::internal

#endif  // V8_LITHIUM_ALLOCATOR_INL_H_

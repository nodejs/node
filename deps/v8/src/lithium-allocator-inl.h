// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LITHIUM_ALLOCATOR_INL_H_
#define V8_LITHIUM_ALLOCATOR_INL_H_

#include "lithium-allocator.h"

#if V8_TARGET_ARCH_IA32
#include "ia32/lithium-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "x64/lithium-x64.h"
#elif V8_TARGET_ARCH_ARM64
#include "arm64/lithium-arm64.h"
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


void LAllocator::SetLiveRangeAssignedRegister(LiveRange* range, int reg) {
  if (range->Kind() == DOUBLE_REGISTERS) {
    assigned_double_registers_->Add(reg);
  } else {
    ASSERT(range->Kind() == GENERAL_REGISTERS);
    assigned_registers_->Add(reg);
  }
  range->set_assigned_register(reg, chunk()->zone());
}


} }  // namespace v8::internal

#endif  // V8_LITHIUM_ALLOCATOR_INL_H_

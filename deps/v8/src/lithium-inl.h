// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LITHIUM_INL_H_
#define V8_LITHIUM_INL_H_

#include "src/lithium.h"

#if V8_TARGET_ARCH_IA32
#include "src/ia32/lithium-ia32.h"  // NOLINT
#elif V8_TARGET_ARCH_X64
#include "src/x64/lithium-x64.h"  // NOLINT
#elif V8_TARGET_ARCH_ARM64
#include "src/arm64/lithium-arm64.h"  // NOLINT
#elif V8_TARGET_ARCH_ARM
#include "src/arm/lithium-arm.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS
#include "src/mips/lithium-mips.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS64
#include "src/mips64/lithium-mips64.h"  // NOLINT
#elif V8_TARGET_ARCH_PPC
#include "src/ppc/lithium-ppc.h"  // NOLINT
#elif V8_TARGET_ARCH_X87
#include "src/x87/lithium-x87.h"  // NOLINT
#else
#error "Unknown architecture."
#endif

namespace v8 {
namespace internal {

TempIterator::TempIterator(LInstruction* instr)
    : instr_(instr), limit_(instr->TempCount()), current_(0) {
  SkipUninteresting();
}


bool TempIterator::Done() { return current_ >= limit_; }


LOperand* TempIterator::Current() {
  DCHECK(!Done());
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
    : instr_(instr), limit_(instr->InputCount()), current_(0) {
  SkipUninteresting();
}


bool InputIterator::Done() { return current_ >= limit_; }


LOperand* InputIterator::Current() {
  DCHECK(!Done());
  DCHECK(instr_->InputAt(current_) != NULL);
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
    : input_iterator_(instr), env_iterator_(instr->environment()) {}


bool UseIterator::Done() {
  return input_iterator_.Done() && env_iterator_.Done();
}


LOperand* UseIterator::Current() {
  DCHECK(!Done());
  LOperand* result = input_iterator_.Done() ? env_iterator_.Current()
                                            : input_iterator_.Current();
  DCHECK(result != NULL);
  return result;
}


void UseIterator::Advance() {
  input_iterator_.Done() ? env_iterator_.Advance() : input_iterator_.Advance();
}
}
}  // namespace v8::internal

#endif  // V8_LITHIUM_INL_H_

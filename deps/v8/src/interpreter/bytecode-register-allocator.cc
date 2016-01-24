// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-register-allocator.h"

#include "src/interpreter/bytecode-array-builder.h"

namespace v8 {
namespace internal {
namespace interpreter {

BytecodeRegisterAllocator::BytecodeRegisterAllocator(
    BytecodeArrayBuilder* builder)
    : builder_(builder),
      allocated_(builder->zone()),
      next_consecutive_register_(-1),
      next_consecutive_count_(-1) {}


BytecodeRegisterAllocator::~BytecodeRegisterAllocator() {
  for (auto i = allocated_.rbegin(); i != allocated_.rend(); i++) {
    builder_->ReturnTemporaryRegister(*i);
  }
  allocated_.clear();
}


Register BytecodeRegisterAllocator::NewRegister() {
  int allocated = -1;
  if (next_consecutive_count_ <= 0) {
    allocated = builder_->BorrowTemporaryRegister();
  } else {
    allocated = builder_->BorrowTemporaryRegisterNotInRange(
        next_consecutive_register_,
        next_consecutive_register_ + next_consecutive_count_ - 1);
  }
  allocated_.push_back(allocated);
  return Register(allocated);
}


bool BytecodeRegisterAllocator::RegisterIsAllocatedInThisScope(
    Register reg) const {
  for (auto i = allocated_.begin(); i != allocated_.end(); i++) {
    if (*i == reg.index()) return true;
  }
  return false;
}


void BytecodeRegisterAllocator::PrepareForConsecutiveAllocations(size_t count) {
  if (static_cast<int>(count) > next_consecutive_count_) {
    next_consecutive_register_ =
        builder_->PrepareForConsecutiveTemporaryRegisters(count);
    next_consecutive_count_ = static_cast<int>(count);
  }
}


Register BytecodeRegisterAllocator::NextConsecutiveRegister() {
  DCHECK_GE(next_consecutive_register_, 0);
  DCHECK_GT(next_consecutive_count_, 0);
  builder_->BorrowConsecutiveTemporaryRegister(next_consecutive_register_);
  allocated_.push_back(next_consecutive_register_);
  next_consecutive_count_--;
  return Register(next_consecutive_register_++);
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-register-allocator.h"

#include "src/interpreter/bytecode-array-builder.h"

namespace v8 {
namespace internal {
namespace interpreter {

TemporaryRegisterAllocator::TemporaryRegisterAllocator(Zone* zone,
                                                       int allocation_base)
    : free_temporaries_(zone),
      allocation_base_(allocation_base),
      allocation_count_(0) {}

Register TemporaryRegisterAllocator::first_temporary_register() const {
  DCHECK(allocation_count() > 0);
  return Register(allocation_base());
}

Register TemporaryRegisterAllocator::last_temporary_register() const {
  DCHECK(allocation_count() > 0);
  return Register(allocation_base() + allocation_count() - 1);
}

int TemporaryRegisterAllocator::AllocateTemporaryRegister() {
  allocation_count_ += 1;
  return allocation_base() + allocation_count() - 1;
}

int TemporaryRegisterAllocator::BorrowTemporaryRegister() {
  if (free_temporaries_.empty()) {
    return AllocateTemporaryRegister();
  } else {
    auto pos = free_temporaries_.begin();
    int retval = *pos;
    free_temporaries_.erase(pos);
    return retval;
  }
}

int TemporaryRegisterAllocator::BorrowTemporaryRegisterNotInRange(
    int start_index, int end_index) {
  if (free_temporaries_.empty()) {
    int next_allocation = allocation_base() + allocation_count();
    while (next_allocation >= start_index && next_allocation <= end_index) {
      free_temporaries_.insert(AllocateTemporaryRegister());
      next_allocation += 1;
    }
    return AllocateTemporaryRegister();
  }

  ZoneSet<int>::iterator index = free_temporaries_.lower_bound(start_index);
  if (index == free_temporaries_.begin()) {
    // If start_index is the first free register, check for a register
    // greater than end_index.
    index = free_temporaries_.upper_bound(end_index);
    if (index == free_temporaries_.end()) {
      return AllocateTemporaryRegister();
    }
  } else {
    // If there is a free register < start_index
    index--;
  }

  int retval = *index;
  free_temporaries_.erase(index);
  return retval;
}

int TemporaryRegisterAllocator::PrepareForConsecutiveTemporaryRegisters(
    size_t count) {
  if (count == 0) {
    return -1;
  }

  // TODO(oth): replace use of set<> here for free_temporaries with a
  // more efficient structure. And/or partition into two searches -
  // one before the translation window and one after.

  // A run will require at least |count| free temporaries.
  while (free_temporaries_.size() < count) {
    free_temporaries_.insert(AllocateTemporaryRegister());
  }

  // Search within existing temporaries for a run.
  auto start = free_temporaries_.begin();
  size_t run_length = 0;
  for (auto run_end = start; run_end != free_temporaries_.end(); run_end++) {
    int expected = *start + static_cast<int>(run_length);
    if (*run_end != expected) {
      start = run_end;
      run_length = 0;
    }
    Register reg_start(*start);
    Register reg_expected(expected);
    if (RegisterTranslator::DistanceToTranslationWindow(reg_start) > 0 &&
        RegisterTranslator::DistanceToTranslationWindow(reg_expected) <= 0) {
      // Run straddles the lower edge of the translation window. Registers
      // after the start of this boundary are displaced by the register
      // translator to provide a hole for translation. Runs either side
      // of the boundary are fine.
      start = run_end;
      run_length = 0;
    }
    if (++run_length == count) {
      return *start;
    }
  }

  // Continue run if possible across existing last temporary.
  if (allocation_count_ > 0 && (start == free_temporaries_.end() ||
                                *start + static_cast<int>(run_length) !=
                                    last_temporary_register().index() + 1)) {
    run_length = 0;
  }

  // Pad temporaries if extended run would cross translation boundary.
  Register reg_first(*start);
  Register reg_last(*start + static_cast<int>(count) - 1);
  DCHECK_GT(RegisterTranslator::DistanceToTranslationWindow(reg_first),
            RegisterTranslator::DistanceToTranslationWindow(reg_last));
  while (RegisterTranslator::DistanceToTranslationWindow(reg_first) > 0 &&
         RegisterTranslator::DistanceToTranslationWindow(reg_last) <= 0) {
    auto pos_insert_pair =
        free_temporaries_.insert(AllocateTemporaryRegister());
    reg_first = Register(*pos_insert_pair.first);
    reg_last = Register(reg_first.index() + static_cast<int>(count) - 1);
    run_length = 0;
  }

  // Ensure enough registers for run.
  while (run_length++ < count) {
    free_temporaries_.insert(AllocateTemporaryRegister());
  }

  int run_start =
      last_temporary_register().index() - static_cast<int>(count) + 1;
  DCHECK(RegisterTranslator::DistanceToTranslationWindow(Register(run_start)) <=
             0 ||
         RegisterTranslator::DistanceToTranslationWindow(
             Register(run_start + static_cast<int>(count) - 1)) > 0);
  return run_start;
}

bool TemporaryRegisterAllocator::RegisterIsLive(Register reg) const {
  if (allocation_count_ > 0) {
    DCHECK(reg >= first_temporary_register() &&
           reg <= last_temporary_register());
    return free_temporaries_.find(reg.index()) == free_temporaries_.end();
  } else {
    return false;
  }
}

void TemporaryRegisterAllocator::BorrowConsecutiveTemporaryRegister(
    int reg_index) {
  DCHECK(free_temporaries_.find(reg_index) != free_temporaries_.end());
  free_temporaries_.erase(reg_index);
}

void TemporaryRegisterAllocator::ReturnTemporaryRegister(int reg_index) {
  DCHECK(free_temporaries_.find(reg_index) == free_temporaries_.end());
  free_temporaries_.insert(reg_index);
}

BytecodeRegisterAllocator::BytecodeRegisterAllocator(
    Zone* zone, TemporaryRegisterAllocator* allocator)
    : base_allocator_(allocator),
      allocated_(zone),
      next_consecutive_register_(-1),
      next_consecutive_count_(-1) {}

BytecodeRegisterAllocator::~BytecodeRegisterAllocator() {
  for (auto i = allocated_.rbegin(); i != allocated_.rend(); i++) {
    base_allocator()->ReturnTemporaryRegister(*i);
  }
  allocated_.clear();
}


Register BytecodeRegisterAllocator::NewRegister() {
  int allocated = -1;
  if (next_consecutive_count_ <= 0) {
    allocated = base_allocator()->BorrowTemporaryRegister();
  } else {
    allocated = base_allocator()->BorrowTemporaryRegisterNotInRange(
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
        base_allocator()->PrepareForConsecutiveTemporaryRegisters(count);
    next_consecutive_count_ = static_cast<int>(count);
  }
}


Register BytecodeRegisterAllocator::NextConsecutiveRegister() {
  DCHECK_GE(next_consecutive_register_, 0);
  DCHECK_GT(next_consecutive_count_, 0);
  base_allocator()->BorrowConsecutiveTemporaryRegister(
      next_consecutive_register_);
  allocated_.push_back(next_consecutive_register_);
  next_consecutive_count_--;
  return Register(next_consecutive_register_++);
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

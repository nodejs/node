// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_REGISTER_ALLOCATOR_H_
#define V8_INTERPRETER_BYTECODE_REGISTER_ALLOCATOR_H_

#include "src/interpreter/bytecodes.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodeArrayBuilder;
class Register;
class TemporaryRegisterObserver;

class TemporaryRegisterAllocator final {
 public:
  TemporaryRegisterAllocator(Zone* zone, int start_index);

  // Borrow a temporary register.
  int BorrowTemporaryRegister();

  // Borrow a temporary register from the register range outside of
  // |start_index| to |end_index|.
  int BorrowTemporaryRegisterNotInRange(int start_index, int end_index);

  // Return a temporary register when no longer used.
  void ReturnTemporaryRegister(int reg_index);

  // Ensure a run of consecutive registers is available. Each register in
  // the range should be borrowed with BorrowConsecutiveTemporaryRegister().
  // Returns the start index of the run.
  int PrepareForConsecutiveTemporaryRegisters(size_t count);

  // Borrow a register from a range prepared with
  // PrepareForConsecutiveTemporaryRegisters().
  void BorrowConsecutiveTemporaryRegister(int reg_index);

  // Returns true if |reg| is a temporary register and is currently
  // borrowed.
  bool RegisterIsLive(Register reg) const;

  // Returns the first register in the range of temporary registers.
  Register first_temporary_register() const;

  // Returns the last register in the range of temporary registers.
  Register last_temporary_register() const;

  // Returns the start index of temporary register allocations.
  int allocation_base() const { return allocation_base_; }

  // Returns the number of temporary register allocations made.
  int allocation_count() const { return allocation_count_; }

  // Sets an observer for temporary register events.
  void set_observer(TemporaryRegisterObserver* observer);

 private:
  // Allocate a temporary register.
  int AllocateTemporaryRegister();

  ZoneSet<int> free_temporaries_;
  int allocation_base_;
  int allocation_count_;
  TemporaryRegisterObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(TemporaryRegisterAllocator);
};

class TemporaryRegisterObserver {
 public:
  virtual ~TemporaryRegisterObserver() {}
  virtual void TemporaryRegisterFreeEvent(Register reg) = 0;
};

// A class that allows the instantiator to allocate temporary registers that are
// cleaned up when scope is closed.
class BytecodeRegisterAllocator final {
 public:
  explicit BytecodeRegisterAllocator(Zone* zone,
                                     TemporaryRegisterAllocator* allocator);
  ~BytecodeRegisterAllocator();
  Register NewRegister();

  // Ensure |count| consecutive allocations are available.
  void PrepareForConsecutiveAllocations(size_t count);

  // Get the next consecutive allocation after calling
  // PrepareForConsecutiveAllocations.
  Register NextConsecutiveRegister();

  // Returns true if |reg| is allocated in this allocator.
  bool RegisterIsAllocatedInThisScope(Register reg) const;

  // Returns true if unused consecutive allocations remain.
  bool HasConsecutiveAllocations() const { return next_consecutive_count_ > 0; }

 private:
  TemporaryRegisterAllocator* base_allocator() const { return base_allocator_; }

  TemporaryRegisterAllocator* base_allocator_;
  ZoneVector<int> allocated_;
  int next_consecutive_register_;
  int next_consecutive_count_;

  DISALLOW_COPY_AND_ASSIGN(BytecodeRegisterAllocator);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8


#endif  // V8_INTERPRETER_BYTECODE_REGISTER_ALLOCATOR_H_

// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_REGISTER_ALLOCATOR_H_
#define V8_INTERPRETER_BYTECODE_REGISTER_ALLOCATOR_H_

#include "src/interpreter/bytecode-register.h"

namespace v8 {
namespace internal {
namespace interpreter {

// A class that allows the allocation of contiguous temporary registers.
class BytecodeRegisterAllocator final {
 public:
  // Enables observation of register allocation and free events.
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void RegisterAllocateEvent(Register reg) = 0;
    virtual void RegisterListAllocateEvent(RegisterList reg_list) = 0;
    virtual void RegisterListFreeEvent(RegisterList reg_list) = 0;
    virtual void RegisterFreeEvent(Register reg_list) = 0;
  };

  explicit BytecodeRegisterAllocator(int start_index)
      : next_register_index_(start_index),
        max_register_count_(start_index),
        observer_(nullptr) {}
  ~BytecodeRegisterAllocator() = default;
  BytecodeRegisterAllocator(const BytecodeRegisterAllocator&) = delete;
  BytecodeRegisterAllocator& operator=(const BytecodeRegisterAllocator&) =
      delete;

  // Returns a new register.
  Register NewRegister() {
    Register reg(next_register_index_++);
    max_register_count_ = std::max(next_register_index_, max_register_count_);
    if (observer_) {
      observer_->RegisterAllocateEvent(reg);
    }
    return reg;
  }

  // Returns a consecutive list of |count| new registers.
  RegisterList NewRegisterList(int count) {
    RegisterList reg_list(next_register_index_, count);
    next_register_index_ += count;
    max_register_count_ = std::max(next_register_index_, max_register_count_);
    if (observer_) {
      observer_->RegisterListAllocateEvent(reg_list);
    }
    return reg_list;
  }

  // Returns a growable register list.
  RegisterList NewGrowableRegisterList() {
    RegisterList reg_list(next_register_index_, 0);
    return reg_list;
  }

  // Appends a new register to |reg_list| increasing it's count by one and
  // returning the register added.
  //
  // Note: no other new registers must be currently allocated since the register
  // list was originally allocated.
  Register GrowRegisterList(RegisterList* reg_list) {
    Register reg(NewRegister());
    reg_list->IncrementRegisterCount();
    // If the following CHECK fails then a register was allocated (and not
    // freed) between the creation of the RegisterList and this call to add a
    // Register.
    CHECK_EQ(reg.index(), reg_list->last_register().index());
    return reg;
  }

  // Release all registers above |register_index|.
  void ReleaseRegisters(int register_index) {
    int count = next_register_index_ - register_index;
    next_register_index_ = register_index;
    if (observer_) {
      observer_->RegisterListFreeEvent(RegisterList(register_index, count));
    }
  }

  // Release last allocated register
  void ReleaseRegister(Register reg) {
    DCHECK_EQ(next_register_index_ - 1, reg.index());
    if (observer_) {
      observer_->RegisterFreeEvent(reg);
    }
    next_register_index_--;
  }

  // Returns true if the register |reg| is a live register.
  bool RegisterIsLive(Register reg) const {
    return reg.index() < next_register_index_;
  }

  // Returns a register list for all currently live registers.
  RegisterList AllLiveRegisters() const {
    return RegisterList(0, next_register_index());
  }

  void set_observer(Observer* observer) { observer_ = observer; }

  int next_register_index() const { return next_register_index_; }
  int maximum_register_count() const { return max_register_count_; }

 private:
  int next_register_index_;
  int max_register_count_;
  Observer* observer_;
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8


#endif  // V8_INTERPRETER_BYTECODE_REGISTER_ALLOCATOR_H_

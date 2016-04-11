// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_REGISTER_ALLOCATOR_H_
#define V8_INTERPRETER_BYTECODE_REGISTER_ALLOCATOR_H_

#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodeArrayBuilder;
class Register;

// A class than allows the instantiator to allocate temporary registers that are
// cleaned up when scope is closed.
class BytecodeRegisterAllocator {
 public:
  explicit BytecodeRegisterAllocator(BytecodeArrayBuilder* builder);
  ~BytecodeRegisterAllocator();
  Register NewRegister();

  void PrepareForConsecutiveAllocations(size_t count);
  Register NextConsecutiveRegister();

  bool RegisterIsAllocatedInThisScope(Register reg) const;

  bool HasConsecutiveAllocations() const { return next_consecutive_count_ > 0; }

 private:
  void* operator new(size_t size);
  void operator delete(void* p);

  BytecodeArrayBuilder* builder_;
  ZoneVector<int> allocated_;
  int next_consecutive_register_;
  int next_consecutive_count_;

  DISALLOW_COPY_AND_ASSIGN(BytecodeRegisterAllocator);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8


#endif  // V8_INTERPRETER_BYTECODE_REGISTER_ALLOCATOR_H_

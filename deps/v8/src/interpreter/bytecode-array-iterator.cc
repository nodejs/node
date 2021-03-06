// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-array-iterator.h"
#include "src/objects/code-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {
namespace interpreter {

BytecodeArrayIterator::BytecodeArrayIterator(
    std::unique_ptr<AbstractBytecodeArray> bytecode_array)
    : BytecodeArrayAccessor(std::move(bytecode_array), 0) {}

BytecodeArrayIterator::BytecodeArrayIterator(
    Handle<BytecodeArray> bytecode_array)
    : BytecodeArrayAccessor(bytecode_array, 0) {}

void BytecodeArrayIterator::Advance() {
  SetOffset(current_offset() + current_bytecode_size());
}

bool BytecodeArrayIterator::done() const {
  return current_offset() >= bytecode_length();
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

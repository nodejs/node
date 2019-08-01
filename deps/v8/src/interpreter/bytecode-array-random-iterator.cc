// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-array-random-iterator.h"
#include "src/objects/code-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {
namespace interpreter {

BytecodeArrayRandomIterator::BytecodeArrayRandomIterator(
    Handle<BytecodeArray> bytecode_array, Zone* zone)
    : BytecodeArrayAccessor(bytecode_array, 0), offsets_(zone) {
  // Run forwards through the bytecode array to determine the offset of each
  // bytecode.
  while (current_offset() < bytecode_array->length()) {
    offsets_.push_back(current_offset());
    SetOffset(current_offset() + current_bytecode_size());
  }
  GoToStart();
}

bool BytecodeArrayRandomIterator::IsValid() const {
  return current_index_ >= 0 &&
         static_cast<size_t>(current_index_) < offsets_.size();
}

void BytecodeArrayRandomIterator::UpdateOffsetFromIndex() {
  if (IsValid()) {
    SetOffset(offsets_[current_index_]);
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_BYTECODE_ITERATOR_INL_H_
#define V8_REGEXP_REGEXP_BYTECODE_ITERATOR_INL_H_

#include "src/regexp/regexp-bytecode-iterator.h"
// Include the non-inl header before the rest of the headers.

#include "src/regexp/regexp-bytecodes-inl.h"

namespace v8 {
namespace internal {
namespace regexp {

bool BytecodeIterator::done() const { return cursor_ >= end_; }

void BytecodeIterator::advance() {
  DCHECK(!done());
  cursor_ += current_size();
}

Bytecode BytecodeIterator::current_bytecode() const {
  DCHECK(!done());
  DCHECK_LE(cursor_ + sizeof(uint32_t), end_);
  return Bytecodes::FromPtr(cursor_);
}

uint8_t BytecodeIterator::current_size() const {
  DCHECK(!done());
  return Bytecodes::Size(current_bytecode());
}

uint32_t BytecodeIterator::current_offset() const {
  return static_cast<uint32_t>(cursor_ - start_);
}

uint8_t* BytecodeIterator::current_address() const { return cursor_; }

void BytecodeIterator::reset() { cursor_ = start_; }

template <typename Func>
void BytecodeIterator::ForEachBytecode(Func&& fun) {
  for (; !done(); advance()) {
    Bytecodes::DispatchOnBytecode(current_bytecode(), fun);
  }
}

}  // namespace regexp
}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_BYTECODE_ITERATOR_INL_H_

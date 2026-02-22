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

bool RegExpBytecodeIterator::done() const { return cursor_ >= end_; }

void RegExpBytecodeIterator::advance() {
  DCHECK(!done());
  cursor_ += current_size();
}

RegExpBytecode RegExpBytecodeIterator::current_bytecode() const {
  DCHECK(!done());
  DCHECK_LE(cursor_ + sizeof(uint32_t), end_);
  return RegExpBytecodes::FromPtr(cursor_);
}

uint8_t RegExpBytecodeIterator::current_size() const {
  DCHECK(!done());
  return RegExpBytecodes::Size(current_bytecode());
}

int RegExpBytecodeIterator::current_offset() const {
  return static_cast<int>(cursor_ - start_);
}

uint8_t* RegExpBytecodeIterator::current_address() const { return cursor_; }

void RegExpBytecodeIterator::reset() { cursor_ = start_; }

template <typename Func>
void RegExpBytecodeIterator::ForEachBytecode(Func&& fun) {
  for (; !done(); advance()) {
    RegExpBytecodes::DispatchOnBytecode(current_bytecode(), fun);
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_BYTECODE_ITERATOR_INL_H_

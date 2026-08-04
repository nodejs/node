// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-bytecode-iterator.h"

#include "src/objects/fixed-array-inl.h"
#include "src/regexp/regexp-bytecode-iterator-inl.h"
#include "src/regexp/regexp-bytecodes-inl.h"

namespace v8 {
namespace internal {
namespace regexp {

BytecodeIterator::BytecodeIterator(DirectHandle<TrustedByteArray> bytecode)
    : BytecodeIterator(bytecode, 0) {}

BytecodeIterator::BytecodeIterator(DirectHandle<TrustedByteArray> bytecode,
                                   uint32_t offset)
    : bytecode_(bytecode),
      start_(bytecode->begin()),
      end_(bytecode->end()),
      cursor_(start_ + offset) {
  Isolate::Current()->main_thread_local_heap()->AddGCEpilogueCallback(
      UpdatePointersCallback, this);
#ifdef DEBUG
  if (V8_UNLIKELY(v8_flags.enable_slow_asserts)) {
    DCHECK_LT(offset, bytecode->ulength().value());
    const uint8_t* start_with_offset = cursor_;
    cursor_ = start_;
    while (cursor_ < start_with_offset) advance();
    // `offset` must not point into the middle of a bytecode.
    DCHECK_EQ(cursor_, start_with_offset);
  }
#endif  // DEBUG
}

BytecodeIterator::~BytecodeIterator() {
  Isolate::Current()->main_thread_local_heap()->RemoveGCEpilogueCallback(
      UpdatePointersCallback, this);
}

// static
void BytecodeIterator::UpdatePointersCallback(void* iterator) {
  reinterpret_cast<BytecodeIterator*>(iterator)->UpdatePointers();
}

void BytecodeIterator::UpdatePointers() {
  DisallowGarbageCollection no_gc;
  uint8_t* start = bytecode_->begin();
  if (start != start_) {
    start_ = start;
    uint8_t* end = bytecode_->end();
    size_t distance_to_end = end_ - cursor_;
    cursor_ = end - distance_to_end;
    end_ = end;
  }
}

}  // namespace regexp
}  // namespace internal
}  // namespace v8

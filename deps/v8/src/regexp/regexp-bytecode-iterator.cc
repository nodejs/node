// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-bytecode-iterator.h"

#include "src/objects/fixed-array-inl.h"
#include "src/regexp/regexp-bytecode-iterator-inl.h"
#include "src/regexp/regexp-bytecodes-inl.h"

namespace v8 {
namespace internal {

RegExpBytecodeIterator::RegExpBytecodeIterator(
    DirectHandle<TrustedByteArray> bytecode)
    : bytecode_(bytecode),
      start_(bytecode->begin()),
      end_(bytecode->end()),
      cursor_(start_) {
  Isolate::Current()->main_thread_local_heap()->AddGCEpilogueCallback(
      UpdatePointersCallback, this);
}

RegExpBytecodeIterator::~RegExpBytecodeIterator() {
  Isolate::Current()->main_thread_local_heap()->RemoveGCEpilogueCallback(
      UpdatePointersCallback, this);
}

// static
void RegExpBytecodeIterator::UpdatePointersCallback(void* iterator) {
  reinterpret_cast<RegExpBytecodeIterator*>(iterator)->UpdatePointers();
}

void RegExpBytecodeIterator::UpdatePointers() {
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

}  // namespace internal
}  // namespace v8

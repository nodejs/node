// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_STATE_H_
#define V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_STATE_H_

#include "include/v8-cppgc.h"
#include "src/heap/heap.h"

namespace v8 {

class JSMemberBase;

namespace internal {

class JSMemberBaseExtractor {
 public:
  static Address* ObjectReference(const JSMemberBase& ref) {
    return reinterpret_cast<Address*>(ref.val_);
  }
};

class UnifiedHeapMarkingState {
 public:
  explicit UnifiedHeapMarkingState(Heap& heap) : heap_(heap) {}

  UnifiedHeapMarkingState(const UnifiedHeapMarkingState&) = delete;
  UnifiedHeapMarkingState& operator=(const UnifiedHeapMarkingState&) = delete;

  inline void MarkAndPush(const JSMemberBase&);

 private:
  Heap& heap_;
};

void UnifiedHeapMarkingState::MarkAndPush(const JSMemberBase& ref) {
  heap_.RegisterExternallyReferencedObject(
      JSMemberBaseExtractor::ObjectReference(ref));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_STATE_H_

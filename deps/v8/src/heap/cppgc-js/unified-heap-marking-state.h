// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_STATE_H_
#define V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_STATE_H_

#include "include/v8-cppgc.h"
#include "include/v8.h"
#include "src/base/logging.h"
#include "src/heap/heap.h"

namespace v8 {

namespace internal {

class BasicTracedReferenceExtractor {
 public:
  static Address* ObjectReference(const TracedReferenceBase& ref) {
    return reinterpret_cast<Address*>(ref.val_);
  }
};

class UnifiedHeapMarkingState {
 public:
  explicit UnifiedHeapMarkingState(Heap* heap) : heap_(heap) {}

  UnifiedHeapMarkingState(const UnifiedHeapMarkingState&) = delete;
  UnifiedHeapMarkingState& operator=(const UnifiedHeapMarkingState&) = delete;

  inline void MarkAndPush(const TracedReferenceBase&);

 private:
  Heap* heap_;
};

void UnifiedHeapMarkingState::MarkAndPush(const TracedReferenceBase& ref) {
  // The same visitor is used in testing scenarios without attaching the heap to
  // an Isolate under the assumption that no non-empty v8 references are found.
  // Having the following DCHECK crash means that the heap is in detached mode
  // but we find traceable pointers into an Isolate.
  DCHECK_NOT_NULL(heap_);
  heap_->RegisterExternallyReferencedObject(
      BasicTracedReferenceExtractor::ObjectReference(ref));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_STATE_H_

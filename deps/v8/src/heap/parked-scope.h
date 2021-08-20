// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_PARKED_SCOPE_H_
#define V8_HEAP_PARKED_SCOPE_H_

#include "src/base/platform/mutex.h"
#include "src/execution/local-isolate.h"
#include "src/heap/local-heap.h"

namespace v8 {
namespace internal {

// Scope that explicitly parks a thread, prohibiting access to the heap and the
// creation of handles.
class V8_NODISCARD ParkedScope {
 public:
  explicit ParkedScope(LocalIsolate* local_isolate)
      : ParkedScope(local_isolate->heap()) {}
  explicit ParkedScope(LocalHeap* local_heap) : local_heap_(local_heap) {
    local_heap_->Park();
  }

  ~ParkedScope() { local_heap_->Unpark(); }

 private:
  LocalHeap* const local_heap_;
};

// Scope that explicitly unparks a thread, allowing access to the heap and the
// creation of handles.
class V8_NODISCARD UnparkedScope {
 public:
  explicit UnparkedScope(LocalIsolate* local_isolate)
      : UnparkedScope(local_isolate->heap()) {}
  explicit UnparkedScope(LocalHeap* local_heap) : local_heap_(local_heap) {
    local_heap_->Unpark();
  }

  ~UnparkedScope() { local_heap_->Park(); }

 private:
  LocalHeap* const local_heap_;
};

class V8_NODISCARD ParkedMutexGuard {
  base::Mutex* guard_;

 public:
  explicit ParkedMutexGuard(LocalIsolate* local_isolate, base::Mutex* guard)
      : ParkedMutexGuard(local_isolate->heap(), guard) {}
  explicit ParkedMutexGuard(LocalHeap* local_heap, base::Mutex* guard)
      : guard_(guard) {
    ParkedScope scope(local_heap);
    guard_->Lock();
  }

  ~ParkedMutexGuard() { guard_->Unlock(); }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_PARKED_SCOPE_H_

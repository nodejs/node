// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/stress-marking-observer.h"
#include "src/heap/incremental-marking.h"

namespace v8 {
namespace internal {

// TODO(majeski): meaningful step_size
StressMarkingObserver::StressMarkingObserver(Heap* heap)
    : AllocationObserver(64), heap_(heap) {}

void StressMarkingObserver::Step(int bytes_allocated, Address soon_object,
                                 size_t size) {
  heap_->StartIncrementalMarkingIfAllocationLimitIsReached(Heap::kNoGCFlags,
                                                           kNoGCCallbackFlags);
}

}  // namespace internal
}  // namespace v8

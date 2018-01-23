// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_ARRAY_BUFFER_COLLECTOR_H_
#define V8_HEAP_ARRAY_BUFFER_COLLECTOR_H_

#include <vector>

#include "src/base/platform/mutex.h"
#include "src/objects/js-array.h"

namespace v8 {
namespace internal {

class Heap;

// To support background processing of array buffer backing stores, we process
// array buffers using the ArrayBufferTracker class. The ArrayBufferCollector
// keeps track of garbage backing stores so that they can be freed on a
// background thread.
class ArrayBufferCollector {
 public:
  explicit ArrayBufferCollector(Heap* heap) : heap_(heap) {}

  ~ArrayBufferCollector() { FreeAllocations(); }

  // These allocations will begin to be freed once FreeAllocations() is called,
  // or on TearDown.
  void AddGarbageAllocations(
      std::vector<JSArrayBuffer::Allocation>* allocations);

  // Calls FreeAllocations() on a background thread.
  void FreeAllocationsOnBackgroundThread();

 private:
  class FreeingTask;

  // Begin freeing the allocations added through AddGarbageAllocations. Also
  // called by TearDown.
  void FreeAllocations();

  Heap* heap_;
  base::Mutex allocations_mutex_;
  std::vector<std::vector<JSArrayBuffer::Allocation>*> allocations_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_ARRAY_BUFFER_COLLECTOR_H_

// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "src/api/api.h"
#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/semaphore.h"
#include "src/common/globals.h"
#include "src/handles/handles-inl.h"
#include "src/handles/local-handles-inl.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/concurrent-allocator-inl.h"
#include "src/heap/heap.h"
#include "src/heap/local-heap.h"
#include "src/heap/safepoint.h"
#include "src/objects/heap-number.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {

const int kNumObjects = 100;
const int kObjectSize = 10 * kTaggedSize;

class ConcurrentAllocationThread final : public v8::base::Thread {
 public:
  explicit ConcurrentAllocationThread(Heap* heap)
      : v8::base::Thread(base::Thread::Options("ThreadWithLocalHeap")),
        heap_(heap) {}

  void Run() override {
    LocalHeap local_heap(heap_);
    ConcurrentAllocator* allocator = local_heap.old_space_allocator();

    for (int i = 0; i < kNumObjects; i++) {
      AllocationResult result =
          allocator->Allocate(kObjectSize, AllocationAlignment::kWordAligned,
                              AllocationOrigin::kRuntime);

      if (result.IsRetry()) {
        break;
      }
    }
  }

  Heap* heap_;
};

TEST(ConcurrentAllocationInOldSpace) {
  CcTest::InitializeVM();
  FLAG_local_heaps = true;
  FLAG_concurrent_allocation = true;
  Isolate* isolate = CcTest::i_isolate();

  std::vector<std::unique_ptr<ConcurrentAllocationThread>> threads;

  const int kThreads = 4;

  for (int i = 0; i < kThreads; i++) {
    auto thread = std::make_unique<ConcurrentAllocationThread>(isolate->heap());
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  for (auto& thread : threads) {
    thread->Join();
  }
}

}  // namespace internal
}  // namespace v8

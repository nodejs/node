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

const int kNumIterations = 2000;
const int kObjectSize = 10 * kTaggedSize;
const int kLargeObjectSize = 8 * KB;

class ConcurrentAllocationThread final : public v8::base::Thread {
 public:
  explicit ConcurrentAllocationThread(Heap* heap, std::atomic<int>* pending)
      : v8::base::Thread(base::Thread::Options("ThreadWithLocalHeap")),
        heap_(heap),
        pending_(pending) {}

  void Run() override {
    LocalHeap local_heap(heap_);
    ConcurrentAllocator* allocator = local_heap.old_space_allocator();

    for (int i = 0; i < kNumIterations; i++) {
      Address address = allocator->AllocateOrFail(
          kObjectSize, AllocationAlignment::kWordAligned,
          AllocationOrigin::kRuntime);
      heap_->CreateFillerObjectAt(address, kObjectSize,
                                  ClearRecordedSlots::kNo);
      address = allocator->AllocateOrFail(kLargeObjectSize,
                                          AllocationAlignment::kWordAligned,
                                          AllocationOrigin::kRuntime);
      heap_->CreateFillerObjectAt(address, kLargeObjectSize,
                                  ClearRecordedSlots::kNo);
      if (i % 10 == 0) {
        local_heap.Safepoint();
      }
    }

    pending_->fetch_sub(1);
  }

  Heap* heap_;
  std::atomic<int>* pending_;
};

UNINITIALIZED_TEST(ConcurrentAllocationInOldSpace) {
  FLAG_max_old_space_size = 32;
  FLAG_concurrent_allocation = true;

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

  FLAG_local_heaps = true;

  std::vector<std::unique_ptr<ConcurrentAllocationThread>> threads;

  const int kThreads = 4;

  std::atomic<int> pending(kThreads);

  for (int i = 0; i < kThreads; i++) {
    auto thread = std::make_unique<ConcurrentAllocationThread>(
        i_isolate->heap(), &pending);
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  while (pending > 0) {
    v8::platform::PumpMessageLoop(i::V8::GetCurrentPlatform(), isolate);
  }

  for (auto& thread : threads) {
    thread->Join();
  }

  isolate->Dispose();
}

}  // namespace internal
}  // namespace v8

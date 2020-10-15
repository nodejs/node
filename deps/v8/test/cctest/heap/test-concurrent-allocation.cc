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
#include "src/heap/local-heap-inl.h"
#include "src/heap/safepoint.h"
#include "src/objects/heap-number.h"
#include "src/objects/heap-object.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {

void CreateFixedArray(Heap* heap, Address start, int size) {
  HeapObject object = HeapObject::FromAddress(start);
  object.set_map_after_allocation(ReadOnlyRoots(heap).fixed_array_map(),
                                  SKIP_WRITE_BARRIER);
  FixedArray array = FixedArray::cast(object);
  int length = (size - FixedArray::kHeaderSize) / kTaggedSize;
  array.set_length(length);
  MemsetTagged(array.data_start(), ReadOnlyRoots(heap).undefined_value(),
               length);
}

const int kNumIterations = 2000;
const int kSmallObjectSize = 10 * kTaggedSize;
const int kMediumObjectSize = 8 * KB;

class ConcurrentAllocationThread final : public v8::base::Thread {
 public:
  explicit ConcurrentAllocationThread(Heap* heap, std::atomic<int>* pending)
      : v8::base::Thread(base::Thread::Options("ThreadWithLocalHeap")),
        heap_(heap),
        pending_(pending) {}

  void Run() override {
    LocalHeap local_heap(heap_);

    for (int i = 0; i < kNumIterations; i++) {
      Address address = local_heap.AllocateRawOrFail(
          kSmallObjectSize, AllocationType::kOld, AllocationOrigin::kRuntime,
          AllocationAlignment::kWordAligned);
      CreateFixedArray(heap_, address, kSmallObjectSize);
      address = local_heap.AllocateRawOrFail(
          kMediumObjectSize, AllocationType::kOld, AllocationOrigin::kRuntime,
          AllocationAlignment::kWordAligned);
      CreateFixedArray(heap_, address, kMediumObjectSize);
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
  FLAG_local_heaps = true;
  FLAG_stress_concurrent_allocation = false;

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

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

class LargeObjectConcurrentAllocationThread final : public v8::base::Thread {
 public:
  explicit LargeObjectConcurrentAllocationThread(Heap* heap,
                                                 std::atomic<int>* pending)
      : v8::base::Thread(base::Thread::Options("ThreadWithLocalHeap")),
        heap_(heap),
        pending_(pending) {}

  void Run() override {
    LocalHeap local_heap(heap_);
    const size_t kLargeObjectSize = kMaxRegularHeapObjectSize * 2;

    for (int i = 0; i < kNumIterations; i++) {
      Address address = local_heap.AllocateRawOrFail(
          kLargeObjectSize, AllocationType::kOld, AllocationOrigin::kRuntime,
          AllocationAlignment::kWordAligned);
      CreateFixedArray(heap_, address, kLargeObjectSize);
      local_heap.Safepoint();
    }

    pending_->fetch_sub(1);
  }

  Heap* heap_;
  std::atomic<int>* pending_;
};

UNINITIALIZED_TEST(ConcurrentAllocationInLargeSpace) {
  FLAG_max_old_space_size = 32;
  FLAG_concurrent_allocation = true;
  FLAG_local_heaps = true;
  FLAG_stress_concurrent_allocation = false;

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);

  std::vector<std::unique_ptr<LargeObjectConcurrentAllocationThread>> threads;

  const int kThreads = 4;

  std::atomic<int> pending(kThreads);

  for (int i = 0; i < kThreads; i++) {
    auto thread = std::make_unique<LargeObjectConcurrentAllocationThread>(
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

const int kWhiteIterations = 1000;

class ConcurrentBlackAllocationThread final : public v8::base::Thread {
 public:
  explicit ConcurrentBlackAllocationThread(
      Heap* heap, std::vector<Address>* objects, base::Semaphore* sema_white,
      base::Semaphore* sema_marking_started)
      : v8::base::Thread(base::Thread::Options("ThreadWithLocalHeap")),
        heap_(heap),
        objects_(objects),
        sema_white_(sema_white),
        sema_marking_started_(sema_marking_started) {}

  void Run() override {
    LocalHeap local_heap(heap_);

    for (int i = 0; i < kNumIterations; i++) {
      if (i == kWhiteIterations) {
        ParkedScope scope(&local_heap);
        sema_white_->Signal();
        sema_marking_started_->Wait();
      }
      Address address = local_heap.AllocateRawOrFail(
          kSmallObjectSize, AllocationType::kOld, AllocationOrigin::kRuntime,
          AllocationAlignment::kWordAligned);
      objects_->push_back(address);
      CreateFixedArray(heap_, address, kSmallObjectSize);
      address = local_heap.AllocateRawOrFail(
          kMediumObjectSize, AllocationType::kOld, AllocationOrigin::kRuntime,
          AllocationAlignment::kWordAligned);
      objects_->push_back(address);
      CreateFixedArray(heap_, address, kMediumObjectSize);
    }
  }

  Heap* heap_;
  std::vector<Address>* objects_;
  base::Semaphore* sema_white_;
  base::Semaphore* sema_marking_started_;
};

UNINITIALIZED_TEST(ConcurrentBlackAllocation) {
  FLAG_concurrent_allocation = true;
  FLAG_local_heaps = true;

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  Heap* heap = i_isolate->heap();

  std::vector<Address> objects;

  base::Semaphore sema_white(0);
  base::Semaphore sema_marking_started(0);

  auto thread = std::make_unique<ConcurrentBlackAllocationThread>(
      heap, &objects, &sema_white, &sema_marking_started);
  CHECK(thread->Start());

  sema_white.Wait();
  heap->StartIncrementalMarking(i::Heap::kNoGCFlags,
                                i::GarbageCollectionReason::kTesting);
  sema_marking_started.Signal();

  thread->Join();

  const int kObjectsAllocatedPerIteration = 2;

  for (int i = 0; i < kNumIterations * kObjectsAllocatedPerIteration; i++) {
    Address address = objects[i];
    HeapObject object = HeapObject::FromAddress(address);

    if (i < kWhiteIterations * kObjectsAllocatedPerIteration) {
      CHECK(heap->incremental_marking()->marking_state()->IsWhite(object));
    } else {
      CHECK(heap->incremental_marking()->marking_state()->IsBlack(object));
    }
  }

  isolate->Dispose();
}

}  // namespace internal
}  // namespace v8

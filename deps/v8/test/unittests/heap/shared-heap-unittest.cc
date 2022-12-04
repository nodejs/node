// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/platform.h"
#include "src/heap/heap.h"
#include "src/heap/parked-scope.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if V8_CAN_CREATE_SHARED_HEAP_BOOL

namespace v8 {
namespace internal {

using SharedHeapTest = TestJSSharedMemoryWithIsolate;

class SharedHeapNoClientsTest : public TestJSSharedMemoryWithPlatform {
 public:
  SharedHeapNoClientsTest() {
    if (v8_flags.shared_space) {
      shared_space_isolate_wrapper.emplace(kNoCounters);
      shared_isolate_ = shared_space_isolate_wrapper->i_isolate();
    } else {
      bool created;
      shared_isolate_ = Isolate::GetProcessWideSharedIsolate(&created);
      CHECK(created);
    }
  }

  ~SharedHeapNoClientsTest() override {
    if (!v8_flags.shared_space) {
      Isolate::DeleteProcessWideSharedIsolate();
    }

    shared_isolate_ = nullptr;
  }

  v8::Isolate* shared_heap_isolate() {
    return reinterpret_cast<v8::Isolate*>(i_shared_heap_isolate());
  }

  Isolate* i_shared_heap_isolate() { return shared_isolate_; }

 private:
  Isolate* shared_isolate_;
  base::Optional<IsolateWrapper> shared_space_isolate_wrapper;
};

namespace {
const int kNumIterations = 2000;

template <typename Callback>
void SetupClientIsolateAndRunCallback(Callback callback) {
  IsolateWrapper isolate_wrapper(kNoCounters);
  v8::Isolate* client_isolate = isolate_wrapper.isolate();
  Isolate* i_client_isolate = reinterpret_cast<Isolate*>(client_isolate);

  callback(client_isolate, i_client_isolate);
}

class SharedOldSpaceAllocationThread final : public ParkingThread {
 public:
  SharedOldSpaceAllocationThread()
      : ParkingThread(base::Thread::Options("SharedOldSpaceAllocationThread")) {
  }

  void Run() override {
    SetupClientIsolateAndRunCallback(
        [](v8::Isolate* client_isolate, Isolate* i_client_isolate) {
          HandleScope scope(i_client_isolate);

          for (int i = 0; i < kNumIterations; i++) {
            i_client_isolate->factory()->NewFixedArray(
                10, AllocationType::kSharedOld);
          }

          CollectGarbage(OLD_SPACE, client_isolate);

          v8::platform::PumpMessageLoop(i::V8::GetCurrentPlatform(),
                                        client_isolate);
        });
  }
};
}  // namespace

TEST_F(SharedHeapTest, ConcurrentAllocationInSharedOldSpace) {
  std::vector<std::unique_ptr<SharedOldSpaceAllocationThread>> threads;
  const int kThreads = 4;

  ParkedScope parked(i_isolate()->main_thread_local_isolate());
  for (int i = 0; i < kThreads; i++) {
    auto thread = std::make_unique<SharedOldSpaceAllocationThread>();
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  for (auto& thread : threads) {
    thread->ParkedJoin(parked);
  }
}

namespace {
class SharedLargeOldSpaceAllocationThread final : public ParkingThread {
 public:
  SharedLargeOldSpaceAllocationThread()
      : ParkingThread(base::Thread::Options("SharedOldSpaceAllocationThread")) {
  }

  void Run() override {
    SetupClientIsolateAndRunCallback(
        [](v8::Isolate* client_isolate, Isolate* i_client_isolate) {
          HandleScope scope(i_client_isolate);
          const int kNumIterations = 50;

          for (int i = 0; i < kNumIterations; i++) {
            HandleScope scope(i_client_isolate);
            Handle<FixedArray> fixed_array =
                i_client_isolate->factory()->NewFixedArray(
                    kMaxRegularHeapObjectSize / kTaggedSize,
                    AllocationType::kSharedOld);
            CHECK(MemoryChunk::FromHeapObject(*fixed_array)->IsLargePage());
          }

          CollectGarbage(OLD_SPACE, client_isolate);

          v8::platform::PumpMessageLoop(i::V8::GetCurrentPlatform(),
                                        client_isolate);
        });
  }
};
}  // namespace

TEST_F(SharedHeapTest, ConcurrentAllocationInSharedLargeOldSpace) {
  std::vector<std::unique_ptr<SharedLargeOldSpaceAllocationThread>> threads;
  const int kThreads = 4;

  ParkedScope parked(i_isolate()->main_thread_local_isolate());
  for (int i = 0; i < kThreads; i++) {
    auto thread = std::make_unique<SharedLargeOldSpaceAllocationThread>();
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  for (auto& thread : threads) {
    thread->ParkedJoin(parked);
  }
}

namespace {
class SharedMapSpaceAllocationThread final : public ParkingThread {
 public:
  SharedMapSpaceAllocationThread()
      : ParkingThread(base::Thread::Options("SharedMapSpaceAllocationThread")) {
  }

  void Run() override {
    SetupClientIsolateAndRunCallback(
        [](v8::Isolate* client_isolate, Isolate* i_client_isolate) {
          HandleScope scope(i_client_isolate);

          for (int i = 0; i < kNumIterations; i++) {
            i_client_isolate->factory()->NewMap(
                NATIVE_CONTEXT_TYPE, kVariableSizeSentinel,
                TERMINAL_FAST_ELEMENTS_KIND, 0, AllocationType::kSharedMap);
          }

          CollectGarbage(OLD_SPACE, client_isolate);

          v8::platform::PumpMessageLoop(i::V8::GetCurrentPlatform(),
                                        client_isolate);
        });
  }
};
}  // namespace

TEST_F(SharedHeapTest, ConcurrentAllocationInSharedMapSpace) {
  std::vector<std::unique_ptr<SharedMapSpaceAllocationThread>> threads;
  const int kThreads = 4;

  ParkedScope parked(i_isolate()->main_thread_local_isolate());
  for (int i = 0; i < kThreads; i++) {
    auto thread = std::make_unique<SharedMapSpaceAllocationThread>();
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  for (auto& thread : threads) {
    thread->ParkedJoin(parked);
  }
}

TEST_F(SharedHeapNoClientsTest, SharedCollectionWithoutClients) {
  if (!v8_flags.shared_space) {
    DCHECK_NULL(i_shared_heap_isolate()->heap()->new_space());
    DCHECK_NULL(i_shared_heap_isolate()->heap()->new_lo_space());
  }

  ::v8::internal::CollectGarbage(OLD_SPACE, shared_heap_isolate());
}

void AllocateInSharedHeap(int iterations = 100) {
  SetupClientIsolateAndRunCallback([iterations](v8::Isolate* client_isolate,
                                                Isolate* i_client_isolate) {
    HandleScope outer_scope(i_client_isolate);
    std::vector<Handle<FixedArray>> arrays_in_handles;
    const int kKeptAliveInHandle = 1000;
    const int kKeptAliveInHeap = 100;
    Handle<FixedArray> arrays_in_heap =
        i_client_isolate->factory()->NewFixedArray(kKeptAliveInHeap,
                                                   AllocationType::kYoung);

    for (int i = 0; i < kNumIterations * iterations; i++) {
      HandleScope scope(i_client_isolate);
      Handle<FixedArray> array = i_client_isolate->factory()->NewFixedArray(
          100, AllocationType::kSharedOld);
      if (i < kKeptAliveInHandle) {
        // Keep some of those arrays alive across GCs through handles.
        arrays_in_handles.push_back(scope.CloseAndEscape(array));
      }

      if (i < kKeptAliveInHeap) {
        // Keep some of those arrays alive across GCs through client heap
        // references.
        arrays_in_heap->set(i, *array);
      }

      i_client_isolate->factory()->NewFixedArray(100, AllocationType::kYoung);
    }

    for (Handle<FixedArray> array : arrays_in_handles) {
      CHECK_EQ(array->length(), 100);
    }

    for (int i = 0; i < kKeptAliveInHeap; i++) {
      FixedArray array = FixedArray::cast(arrays_in_heap->get(i));
      CHECK_EQ(array.length(), 100);
    }
  });
}

TEST_F(SharedHeapTest, SharedCollectionWithOneClient) {
  v8_flags.max_old_space_size = 8;
  ParkedScope parked(i_isolate()->main_thread_local_isolate());
  AllocateInSharedHeap();
}

namespace {
class SharedFixedArrayAllocationThread final : public ParkingThread {
 public:
  SharedFixedArrayAllocationThread()
      : ParkingThread(
            base::Thread::Options("SharedFixedArrayAllocationThread")) {}

  void Run() override { AllocateInSharedHeap(5); }
};
}  // namespace

TEST_F(SharedHeapTest, SharedCollectionWithMultipleClients) {
  v8_flags.max_old_space_size = 8;

  std::vector<std::unique_ptr<SharedFixedArrayAllocationThread>> threads;
  const int kThreads = 4;

  ParkedScope parked(i_isolate()->main_thread_local_isolate());
  for (int i = 0; i < kThreads; i++) {
    auto thread = std::make_unique<SharedFixedArrayAllocationThread>();
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  for (auto& thread : threads) {
    thread->ParkedJoin(parked);
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_CAN_CREATE_SHARED_HEAP

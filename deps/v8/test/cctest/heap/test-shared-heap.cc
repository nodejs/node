// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-array-buffer.h"
#include "include/v8-initialization.h"
#include "include/v8-isolate.h"
#include "src/common/globals.h"
#include "src/handles/handles-inl.h"
#include "src/heap/heap.h"
#include "src/heap/read-only-spaces.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/fixed-array.h"
#include "src/objects/heap-object.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

namespace {
const int kNumIterations = 2000;

template <typename Callback>
void SetupClientIsolateAndRunCallback(Isolate* shared_isolate,
                                      Callback callback) {
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = allocator.get();
  create_params.experimental_attach_to_shared_isolate =
      reinterpret_cast<v8::Isolate*>(shared_isolate);
  v8::Isolate* client_isolate = v8::Isolate::New(create_params);
  Isolate* i_client_isolate = reinterpret_cast<Isolate*>(client_isolate);

  callback(client_isolate, i_client_isolate);

  client_isolate->Dispose();
}

class SharedOldSpaceAllocationThread final : public v8::base::Thread {
 public:
  explicit SharedOldSpaceAllocationThread(Isolate* shared)
      : v8::base::Thread(
            base::Thread::Options("SharedOldSpaceAllocationThread")),
        shared_(shared) {}

  void Run() override {
    SetupClientIsolateAndRunCallback(shared_, [](v8::Isolate* client_isolate,
                                                 Isolate* i_client_isolate) {
      HandleScope scope(i_client_isolate);

      for (int i = 0; i < kNumIterations; i++) {
        i_client_isolate->factory()->NewFixedArray(10,
                                                   AllocationType::kSharedOld);
      }

      CcTest::CollectGarbage(OLD_SPACE, i_client_isolate);

      v8::platform::PumpMessageLoop(i::V8::GetCurrentPlatform(),
                                    client_isolate);
    });
  }

  Isolate* shared_;
};
}  // namespace

UNINITIALIZED_TEST(ConcurrentAllocationInSharedOldSpace) {
  if (!ReadOnlyHeap::IsReadOnlySpaceShared()) return;
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = allocator.get();
  Isolate* shared_isolate = Isolate::NewShared(create_params);

  std::vector<std::unique_ptr<SharedOldSpaceAllocationThread>> threads;
  const int kThreads = 4;

  for (int i = 0; i < kThreads; i++) {
    auto thread =
        std::make_unique<SharedOldSpaceAllocationThread>(shared_isolate);
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  for (auto& thread : threads) {
    thread->Join();
  }

  Isolate::Delete(shared_isolate);
}

namespace {
class SharedMapSpaceAllocationThread final : public v8::base::Thread {
 public:
  explicit SharedMapSpaceAllocationThread(Isolate* shared)
      : v8::base::Thread(
            base::Thread::Options("SharedMapSpaceAllocationThread")),
        shared_(shared) {}

  void Run() override {
    SetupClientIsolateAndRunCallback(
        shared_, [](v8::Isolate* client_isolate, Isolate* i_client_isolate) {
          HandleScope scope(i_client_isolate);

          for (int i = 0; i < kNumIterations; i++) {
            i_client_isolate->factory()->NewMap(
                NATIVE_CONTEXT_TYPE, kVariableSizeSentinel,
                TERMINAL_FAST_ELEMENTS_KIND, 0, AllocationType::kSharedMap);
          }

          CcTest::CollectGarbage(OLD_SPACE, i_client_isolate);

          v8::platform::PumpMessageLoop(i::V8::GetCurrentPlatform(),
                                        client_isolate);
        });
  }

  Isolate* shared_;
};
}  // namespace

UNINITIALIZED_TEST(ConcurrentAllocationInSharedMapSpace) {
  if (!ReadOnlyHeap::IsReadOnlySpaceShared()) return;
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = allocator.get();
  Isolate* shared_isolate = Isolate::NewShared(create_params);

  std::vector<std::unique_ptr<SharedMapSpaceAllocationThread>> threads;
  const int kThreads = 4;

  for (int i = 0; i < kThreads; i++) {
    auto thread =
        std::make_unique<SharedMapSpaceAllocationThread>(shared_isolate);
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  for (auto& thread : threads) {
    thread->Join();
  }

  Isolate::Delete(shared_isolate);
}

UNINITIALIZED_TEST(SharedCollectionWithoutClients) {
  if (!ReadOnlyHeap::IsReadOnlySpaceShared()) return;
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = allocator.get();
  Isolate* shared_isolate = Isolate::NewShared(create_params);

  DCHECK_NULL(shared_isolate->heap()->new_space());
  DCHECK_NULL(shared_isolate->heap()->new_lo_space());

  CcTest::CollectGarbage(OLD_SPACE, shared_isolate);
  Isolate::Delete(shared_isolate);
}

void AllocateInSharedHeap(Isolate* shared_isolate, int iterations = 100) {
  SetupClientIsolateAndRunCallback(
      shared_isolate,
      [iterations](v8::Isolate* client_isolate, Isolate* i_client_isolate) {
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

          i_client_isolate->factory()->NewFixedArray(100,
                                                     AllocationType::kYoung);
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

UNINITIALIZED_TEST(SharedCollectionWithOneClient) {
  FLAG_max_old_space_size = 8;
  if (!ReadOnlyHeap::IsReadOnlySpaceShared()) return;
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = allocator.get();
  Isolate* shared_isolate = Isolate::NewShared(create_params);

  AllocateInSharedHeap(shared_isolate);

  Isolate::Delete(shared_isolate);
}

namespace {
class SharedFixedArrayAllocationThread final : public v8::base::Thread {
 public:
  explicit SharedFixedArrayAllocationThread(Isolate* shared)
      : v8::base::Thread(
            base::Thread::Options("SharedFixedArrayAllocationThread")),
        shared_(shared) {}

  void Run() override { AllocateInSharedHeap(shared_, 5); }

  Isolate* shared_;
};
}  // namespace

UNINITIALIZED_TEST(SharedCollectionWithMultipleClients) {
  FLAG_max_old_space_size = 8;
  if (!ReadOnlyHeap::IsReadOnlySpaceShared()) return;
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = allocator.get();
  Isolate* shared_isolate = Isolate::NewShared(create_params);

  std::vector<std::unique_ptr<SharedFixedArrayAllocationThread>> threads;
  const int kThreads = 4;

  for (int i = 0; i < kThreads; i++) {
    auto thread =
        std::make_unique<SharedFixedArrayAllocationThread>(shared_isolate);
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  for (auto& thread : threads) {
    thread->Join();
  }

  Isolate::Delete(shared_isolate);
}

}  // namespace internal
}  // namespace v8

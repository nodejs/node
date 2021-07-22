// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8.h"
#include "src/common/globals.h"
#include "src/handles/handles-inl.h"
#include "src/heap/heap.h"
#include "src/objects/heap-object.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

namespace {
const int kNumIterations = 2000;

class SharedSpaceAllocationThread final : public v8::base::Thread {
 public:
  explicit SharedSpaceAllocationThread(Isolate* shared)
      : v8::base::Thread(base::Thread::Options("SharedSpaceAllocationThread")),
        shared_(shared) {}

  void Run() override {
    v8::Isolate::CreateParams create_params;
    allocator_.reset(v8::ArrayBuffer::Allocator::NewDefaultAllocator());
    create_params.array_buffer_allocator = allocator_.get();
    v8::Isolate* client_isolate = v8::Isolate::New(create_params);
    Isolate* i_client_isolate = reinterpret_cast<Isolate*>(client_isolate);
    i_client_isolate->AttachToSharedIsolate(shared_);

    {
      HandleScope scope(i_client_isolate);

      for (int i = 0; i < kNumIterations; i++) {
        i_client_isolate->factory()->NewFixedArray(10,
                                                   AllocationType::kSharedOld);
      }

      CcTest::CollectGarbage(OLD_SPACE, i_client_isolate);

      v8::platform::PumpMessageLoop(i::V8::GetCurrentPlatform(),
                                    client_isolate);
    }

    client_isolate->Dispose();
  }

  Isolate* shared_;
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator_;
};
}  // namespace

UNINITIALIZED_TEST(ConcurrentAllocationInSharedOldSpace) {
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = allocator.get();
  Isolate* shared_isolate = Isolate::NewShared(create_params);

  std::vector<std::unique_ptr<SharedSpaceAllocationThread>> threads;
  const int kThreads = 4;

  for (int i = 0; i < kThreads; i++) {
    auto thread = std::make_unique<SharedSpaceAllocationThread>(shared_isolate);
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  for (auto& thread : threads) {
    thread->Join();
  }

  Isolate::Delete(shared_isolate);
}

UNINITIALIZED_TEST(SharedCollection) {
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = allocator.get();
  Isolate* shared_isolate = Isolate::NewShared(create_params);

  CcTest::CollectGarbage(OLD_SPACE, shared_isolate);
  Isolate::Delete(shared_isolate);
}

}  // namespace internal
}  // namespace v8

// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/local-heap.h"
#include "src/heap/heap.h"
#include "src/heap/safepoint.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using LocalHeapTest = TestWithIsolate;

TEST_F(LocalHeapTest, Initialize) {
  Heap* heap = i_isolate()->heap();
  CHECK(heap->safepoint()->ContainsAnyLocalHeap());
}

TEST_F(LocalHeapTest, Current) {
  Heap* heap = i_isolate()->heap();

  CHECK_NULL(LocalHeap::Current());

  {
    LocalHeap lh(heap, ThreadKind::kMain);
    CHECK_NULL(LocalHeap::Current());
  }

  CHECK_NULL(LocalHeap::Current());

  {
    LocalHeap lh(heap, ThreadKind::kMain);
    CHECK_NULL(LocalHeap::Current());
  }

  CHECK_NULL(LocalHeap::Current());
}

namespace {
class BackgroundThread final : public v8::base::Thread {
 public:
  explicit BackgroundThread(Heap* heap)
      : v8::base::Thread(base::Thread::Options("BackgroundThread")),
        heap_(heap) {}

  void Run() override {
    CHECK_NULL(LocalHeap::Current());
    {
      LocalHeap lh(heap_, ThreadKind::kBackground);
      CHECK_EQ(&lh, LocalHeap::Current());
    }
    CHECK_NULL(LocalHeap::Current());
  }

  Heap* heap_;
};
}  // anonymous namespace

TEST_F(LocalHeapTest, CurrentBackground) {
  Heap* heap = i_isolate()->heap();
  CHECK_NULL(LocalHeap::Current());
  {
    LocalHeap lh(heap, ThreadKind::kMain);
    auto thread = std::make_unique<BackgroundThread>(heap);
    CHECK(thread->Start());
    CHECK_NULL(LocalHeap::Current());
    thread->Join();
    CHECK_NULL(LocalHeap::Current());
  }
  CHECK_NULL(LocalHeap::Current());
}

}  // namespace internal
}  // namespace v8

// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap.h"
#include "src/heap/local-heap.h"
#include "src/heap/parked-scope-inl.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {

using DirectHandlesTest = TestWithIsolate;

TEST_F(DirectHandlesTest, CreateDirectHandleFromLocal) {
  HandleScope scope(isolate());
  Local<String> foo = String::NewFromUtf8Literal(isolate(), "foo");

  i::DirectHandle<i::String> direct = Utils::OpenDirectHandle(*foo);
  i::Handle<i::String> handle = Utils::OpenHandle(*foo);

  EXPECT_EQ(*direct, *handle);
}

TEST_F(DirectHandlesTest, CreateLocalFromDirectHandle) {
  HandleScope scope(isolate());
  i::Handle<i::String> handle =
      i_isolate()->factory()->NewStringFromAsciiChecked("foo");
  i::DirectHandle<i::String> direct = handle;

  Local<String> l1 = Utils::ToLocal(direct, i_isolate());
  Local<String> l2 = Utils::ToLocal(handle);

  EXPECT_EQ(l1, l2);
}

TEST_F(DirectHandlesTest, CreateMaybeDirectHandle) {
  HandleScope scope(isolate());
  i::Handle<i::String> handle =
      i_isolate()->factory()->NewStringFromAsciiChecked("foo");
  i::DirectHandle<i::String> direct = handle;

  i::MaybeDirectHandle<i::String> maybe_direct(direct);
  i::MaybeHandle<i::String> maybe_handle(handle);

  EXPECT_EQ(*maybe_direct.ToHandleChecked(), *maybe_handle.ToHandleChecked());
}

TEST_F(DirectHandlesTest, CreateMaybeDirectObjectHandle) {
  HandleScope scope(isolate());
  i::Handle<i::String> handle =
      i_isolate()->factory()->NewStringFromAsciiChecked("foo");
  i::DirectHandle<i::String> direct = handle;

  i::MaybeObjectDirectHandle maybe_direct(direct);
  i::MaybeObjectHandle maybe_handle(handle);

  EXPECT_EQ(*maybe_direct, *maybe_handle);
}

TEST_F(DirectHandlesTest, IsIdenticalTo) {
  i::DirectHandle<i::String> d1 =
      i_isolate()->factory()->NewStringFromAsciiChecked("foo");
  i::DirectHandle<i::String> d2(d1);

  i::DirectHandle<i::String> d3 =
      i_isolate()->factory()->NewStringFromAsciiChecked("bar");
  i::DirectHandle<i::String> d4;
  i::DirectHandle<i::String> d5;

  EXPECT_TRUE(d1.is_identical_to(d2));
  EXPECT_TRUE(d2.is_identical_to(d1));
  EXPECT_FALSE(d1.is_identical_to(d3));
  EXPECT_FALSE(d1.is_identical_to(d4));
  EXPECT_FALSE(d4.is_identical_to(d1));
  EXPECT_TRUE(d4.is_identical_to(d5));
}

TEST_F(DirectHandlesTest, MaybeObjectDirectHandleIsIdenticalTo) {
  i::DirectHandle<i::String> foo =
      i_isolate()->factory()->NewStringFromAsciiChecked("foo");
  i::DirectHandle<i::String> bar =
      i_isolate()->factory()->NewStringFromAsciiChecked("bar");

  i::MaybeObjectDirectHandle d1(foo);
  i::MaybeObjectDirectHandle d2(foo);
  i::MaybeObjectDirectHandle d3(bar);
  i::MaybeObjectDirectHandle d4;
  i::MaybeObjectDirectHandle d5;

  EXPECT_TRUE(d1.is_identical_to(d2));
  EXPECT_TRUE(d2.is_identical_to(d1));
  EXPECT_FALSE(d1.is_identical_to(d3));
  EXPECT_FALSE(d1.is_identical_to(d4));
  EXPECT_FALSE(d4.is_identical_to(d1));
  EXPECT_TRUE(d4.is_identical_to(d5));
}

// Tests to check DirectHandle usage.
// Such usage violations are only detected in debug builds, with the
// compile-time flag for enabling direct handles.

#if defined(DEBUG) && defined(V8_ENABLE_DIRECT_HANDLE)

namespace {
template <typename Callback>
void CheckDirectHandleUsage(Callback callback) {
  EXPECT_DEATH_IF_SUPPORTED(callback(), "");
}
}  // anonymous namespace

TEST_F(DirectHandlesTest, DirectHandleOutOfStackFails) {
  // Out-of-stack allocation of direct handles should fail.
  CheckDirectHandleUsage([]() {
    auto ptr = std::make_unique<i::DirectHandle<i::String>>();
    USE(ptr);
  });
}

namespace {
class BackgroundThread final : public v8::base::Thread {
 public:
  explicit BackgroundThread(i::Heap* heap)
      : v8::base::Thread(base::Thread::Options("BackgroundThread")),
        heap_(heap) {}

  void Run() override {
    i::LocalHeap lh(heap_, i::ThreadKind::kBackground);
    // Usage of direct handles in background threads should fail.
    CheckDirectHandleUsage([]() {
      i::DirectHandle<i::String> direct;
      USE(direct);
    });
  }

  i::Heap* heap_;
};
}  // anonymous namespace

TEST_F(DirectHandlesTest, DirectHandleInBackgroundThreadFails) {
  i::Heap* heap = i_isolate()->heap();
  i::LocalHeap lh(heap, i::ThreadKind::kMain);
  lh.SetUpMainThreadForTesting();
  auto thread = std::make_unique<BackgroundThread>(heap);
  CHECK(thread->Start());
  thread->Join();
}

#if V8_CAN_CREATE_SHARED_HEAP_BOOL

using DirectHandlesSharedTest = i::TestJSSharedMemoryWithIsolate;

namespace {
class ClientThread final : public i::ParkingThread {
 public:
  ClientThread() : ParkingThread(base::Thread::Options("ClientThread")) {}

  void Run() override {
    IsolateWrapper isolate_wrapper(kNoCounters);
    // Direct handles can be used in the main thread of client isolates.
    i::DirectHandle<i::String> direct;
    USE(direct);
  }
};
}  // anonymous namespace

TEST_F(DirectHandlesSharedTest, DirectHandleInClient) {
  auto thread = std::make_unique<ClientThread>();
  CHECK(thread->Start());
  thread->ParkedJoin(i_isolate()->main_thread_local_isolate());
}

namespace {
class ClientMainThread final : public i::ParkingThread {
 public:
  ClientMainThread()
      : ParkingThread(base::Thread::Options("ClientMainThread")) {}

  void Run() override {
    IsolateWrapper isolate_wrapper(kNoCounters);
    Isolate* client_isolate = isolate_wrapper.isolate();
    i::Isolate* i_client_isolate =
        reinterpret_cast<i::Isolate*>(client_isolate);

    i::Heap* heap = i_client_isolate->heap();
    i::LocalHeap lh(heap, i::ThreadKind::kMain);
    lh.SetUpMainThreadForTesting();
    auto thread = std::make_unique<BackgroundThread>(heap);
    CHECK(thread->Start());
    thread->Join();
  }
};
}  // anonymous namespace

TEST_F(DirectHandlesSharedTest, DirectHandleInClientBackgroundThreadFails) {
  auto thread = std::make_unique<ClientMainThread>();
  CHECK(thread->Start());
  thread->ParkedJoin(i_isolate()->main_thread_local_isolate());
}

#endif  // V8_CAN_CREATE_SHARED_HEAP_BOOL
#endif  // DEBUG && V8_ENABLE_DIRECT_HANDLE

}  // namespace v8

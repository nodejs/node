// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/cross-thread-persistent.h"

#include "include/cppgc/allocation.h"
#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/platform.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

struct GCed final : GarbageCollected<GCed> {
  static size_t destructor_call_count;
  GCed() { destructor_call_count = 0; }
  ~GCed() { destructor_call_count++; }
  void Trace(cppgc::Visitor*) const {}
  int a = 0;
};
size_t GCed::destructor_call_count = 0;

class Runner final : public v8::base::Thread {
 public:
  template <typename Callback>
  explicit Runner(Callback callback)
      : Thread(v8::base::Thread::Options("CrossThreadPersistent Thread")),
        callback_(callback) {}

  void Run() final { callback_(); }

 private:
  std::function<void()> callback_;
};

}  // namespace

class CrossThreadPersistentTest : public testing::TestWithHeap {};

TEST_F(CrossThreadPersistentTest, RetainStronglyOnDifferentThread) {
  subtle::CrossThreadPersistent<GCed> holder =
      MakeGarbageCollected<GCed>(GetAllocationHandle());
  {
    Runner runner([obj = std::move(holder)]() {});
    EXPECT_FALSE(holder);
    EXPECT_EQ(0u, GCed::destructor_call_count);
    PreciseGC();
    EXPECT_EQ(0u, GCed::destructor_call_count);
    runner.StartSynchronously();
    runner.Join();
  }
  EXPECT_EQ(0u, GCed::destructor_call_count);
  PreciseGC();
  EXPECT_EQ(1u, GCed::destructor_call_count);
}

TEST_F(CrossThreadPersistentTest, RetainWeaklyOnDifferentThread) {
  subtle::WeakCrossThreadPersistent<GCed> in =
      MakeGarbageCollected<GCed>(GetAllocationHandle());
  // Set up |out| with an object that is always retained to ensure that the
  // different thread indeed moves back an empty handle.
  Persistent<GCed> out_holder =
      MakeGarbageCollected<GCed>(GetAllocationHandle());
  subtle::WeakCrossThreadPersistent<GCed> out = *out_holder;
  {
    Persistent<GCed> temporary_holder = *in;
    Runner runner([obj = std::move(in), &out]() { out = std::move(obj); });
    EXPECT_FALSE(in);
    EXPECT_TRUE(out);
    EXPECT_EQ(0u, GCed::destructor_call_count);
    PreciseGC();
    EXPECT_EQ(0u, GCed::destructor_call_count);
    temporary_holder.Clear();
    PreciseGC();
    EXPECT_EQ(1u, GCed::destructor_call_count);
    runner.StartSynchronously();
    runner.Join();
  }
  EXPECT_FALSE(out);
}

TEST_F(CrossThreadPersistentTest, DestroyRacingWithGC) {
  // Destroy a handle on a different thread while at the same time invoking a
  // garbage collection on the original thread.
  subtle::CrossThreadPersistent<GCed> holder =
      MakeGarbageCollected<GCed>(GetAllocationHandle());
  Runner runner([&obj = holder]() { obj.Clear(); });
  EXPECT_TRUE(holder);
  runner.StartSynchronously();
  PreciseGC();
  runner.Join();
  EXPECT_FALSE(holder);
}

}  // namespace internal
}  // namespace cppgc

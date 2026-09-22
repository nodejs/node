// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/allocation.h"
#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/prefinalizer.h"
#include "include/v8-cppgc.h"
#include "include/v8-traced-handle.h"
#include "src/api/api-inl.h"
#include "src/base/platform/platform.h"
#include "src/handles/global-handles.h"
#include "src/heap/cppgc-internal/marker.h"
#include "src/heap/cppgc-internal/visitor.h"
#include "src/heap/cppgc-js/cpp-heap.h"
#include "src/heap/marking-state-inl.h"
#include "test/unittests/heap/cppgc-js/unified-heap-utils.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using TracedReferenceTest = TestWithHeapInternals;

TEST_F(TracedReferenceTest, ResetFromLocal) {
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  v8::TracedReference<v8::Object> ref;
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    ASSERT_TRUE(ref.IsEmpty());
    EXPECT_NE(ref, local);
    ref.Reset(v8_isolate(), local);
    EXPECT_FALSE(ref.IsEmpty());
    EXPECT_EQ(ref, local);
  }
}

TEST_F(TracedReferenceTest, ConstructFromLocal) {
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref(v8_isolate(), local);
    EXPECT_FALSE(ref.IsEmpty());
    EXPECT_EQ(ref, local);
  }
}

TEST_F(TracedReferenceTest, Reset) {
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref(v8_isolate(), local);
    EXPECT_FALSE(ref.IsEmpty());
    EXPECT_EQ(ref, local);
    ref.Reset();
    EXPECT_TRUE(ref.IsEmpty());
    EXPECT_NE(ref, local);
  }
}

TEST_F(TracedReferenceTest, Copy) {
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref(v8_isolate(), local);
    v8::TracedReference<v8::Object> ref_copy1(ref);
    v8::TracedReference<v8::Object> ref_copy2 = ref;
    EXPECT_EQ(ref, local);
    EXPECT_EQ(ref_copy1, local);
    EXPECT_EQ(ref_copy2, local);
  }
}

TEST_F(TracedReferenceTest, CopyHeterogenous) {
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref(v8_isolate(), local);
    v8::TracedReference<v8::Value> ref_copy1(ref);
    v8::TracedReference<v8::Value> ref_copy2 = ref;
    EXPECT_EQ(ref, local);
    EXPECT_EQ(ref_copy1, local);
    EXPECT_EQ(ref_copy2, local);
  }
}

TEST_F(TracedReferenceTest, Move) {
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref(v8_isolate(), local);
    v8::TracedReference<v8::Object> ref_moved1(std::move(ref));
    v8::TracedReference<v8::Object> ref_moved2 = std::move(ref_moved1);
    EXPECT_TRUE(ref.IsEmpty());
    EXPECT_TRUE(ref_moved1.IsEmpty());
    EXPECT_EQ(ref_moved2, local);
  }
}

TEST_F(TracedReferenceTest, MoveHeterogenous) {
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref1(v8_isolate(), local);
    v8::TracedReference<v8::Value> ref_moved1(std::move(ref1));
    v8::TracedReference<v8::Object> ref2(v8_isolate(), local);
    v8::TracedReference<v8::Object> ref_moved2 = std::move(ref2);
    EXPECT_TRUE(ref1.IsEmpty());
    EXPECT_EQ(ref_moved1, local);
    EXPECT_TRUE(ref2.IsEmpty());
    EXPECT_EQ(ref_moved2, local);
  }
}

TEST_F(TracedReferenceTest, Equality) {
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local1 =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref1(v8_isolate(), local1);
    v8::TracedReference<v8::Object> ref2(v8_isolate(), local1);
    EXPECT_EQ(ref1, ref2);
    EXPECT_EQ(ref2, ref1);
    v8::Local<v8::Object> local2 =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref3(v8_isolate(), local2);
    EXPECT_NE(ref2, ref3);
    EXPECT_NE(ref3, ref2);
  }
}

TEST_F(TracedReferenceTest, EqualityHeterogenous) {
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local1 =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref1(v8_isolate(), local1);
    v8::TracedReference<v8::Value> ref2(v8_isolate(), local1);
    EXPECT_EQ(ref1, ref2);
    EXPECT_EQ(ref2, ref1);
    v8::Local<v8::Object> local2 =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref3(v8_isolate(), local2);
    EXPECT_NE(ref2, ref3);
    EXPECT_NE(ref3, ref2);
  }
}

namespace {

// Must be used on stack.
class JSVisitorForTesting final : public JSVisitor {
 public:
  explicit JSVisitorForTesting(v8::Local<v8::Object> expected_object)
      : JSVisitor(cppgc::internal::VisitorFactory::CreateKey()),
        expected_object_(expected_object) {}

  void Visit(const TracedReferenceBase& ref) final {
    EXPECT_EQ(ref, expected_object_);
    visit_count_++;
  }

  size_t visit_count() const { return visit_count_; }

 private:
  v8::Local<v8::Object> expected_object_;
  size_t visit_count_ = 0;
};

}  // namespace

TEST_F(TracedReferenceTest, TracedReferenceTrace) {
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> js_member(v8_isolate(), local);
    JSVisitorForTesting visitor(local);
    // Cast to cppgc::Visitor to ensure that we dispatch through the base
    // visitor and use traits.
    static_cast<cppgc::Visitor&>(visitor).Trace(js_member);
    EXPECT_EQ(1u, visitor.visit_count());
  }
}

TEST_F(TracedReferenceTest, NoWriteBarrierOnConstruction) {
  if (!v8_flags.incremental_marking) {
    GTEST_SKIP() << "Write barrier tests require incremental marking";
  }

  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    SimulateIncrementalMarking();
    MarkingState state(i_isolate());
    ASSERT_TRUE(
        state.IsUnmarked(Cast<HeapObject>(*Utils::OpenDirectHandle(*local))));
    auto ref =
        std::make_unique<v8::TracedReference<v8::Object>>(v8_isolate(), local);
    USE(ref);
    EXPECT_TRUE(
        state.IsUnmarked(Cast<HeapObject>(*Utils::OpenDirectHandle(*local))));
  }
}

TEST_F(TracedReferenceTest, WriteBarrierForOnHeapReset) {
  if (!v8_flags.incremental_marking) {
    GTEST_SKIP() << "Write barrier tests require incremental marking";
  }

  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    auto ref = std::make_unique<v8::TracedReference<v8::Object>>();
    SimulateIncrementalMarking();
    MarkingState state(i_isolate());
    ASSERT_TRUE(
        state.IsUnmarked(Cast<HeapObject>(*Utils::OpenDirectHandle(*local))));
    ref->Reset(v8_isolate(), local);
    EXPECT_FALSE(
        state.IsUnmarked(Cast<HeapObject>(*Utils::OpenDirectHandle(*local))));
  }
}

TEST_F(TracedReferenceTest, WriteBarrierForOnStackReset) {
  if (!v8_flags.incremental_marking) {
    GTEST_SKIP() << "Write barrier tests require incremental marking";
  }

  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    v8::TracedReference<v8::Object> ref;
    SimulateIncrementalMarking();
    MarkingState state(i_isolate());
    ASSERT_TRUE(
        state.IsUnmarked(Cast<HeapObject>(*Utils::OpenDirectHandle(*local))));
    ref.Reset(v8_isolate(), local);
    EXPECT_FALSE(
        state.IsUnmarked(Cast<HeapObject>(*Utils::OpenDirectHandle(*local))));
  }
}

TEST_F(TracedReferenceTest, WriteBarrierOnHeapCopy) {
  if (!v8_flags.incremental_marking) {
    GTEST_SKIP() << "Write barrier tests require incremental marking";
  }

  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    auto ref_from =
        std::make_unique<v8::TracedReference<v8::Object>>(v8_isolate(), local);
    auto ref_to = std::make_unique<v8::TracedReference<v8::Object>>();
    SimulateIncrementalMarking();
    MarkingState state(i_isolate());
    ASSERT_TRUE(
        state.IsUnmarked(Cast<HeapObject>(*Utils::OpenDirectHandle(*local))));
    *ref_to = *ref_from;
    EXPECT_TRUE(!ref_from->IsEmpty());
    EXPECT_FALSE(
        state.IsUnmarked(Cast<HeapObject>(*Utils::OpenDirectHandle(*local))));
  }
}

TEST_F(TracedReferenceTest, WriteBarrierForOnStackCopy) {
  if (!v8_flags.incremental_marking) {
    GTEST_SKIP() << "Write barrier tests require incremental marking";
  }

  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    auto ref_from =
        std::make_unique<v8::TracedReference<v8::Object>>(v8_isolate(), local);
    v8::TracedReference<v8::Object> ref_to;
    SimulateIncrementalMarking();
    MarkingState state(i_isolate());
    ASSERT_TRUE(
        state.IsUnmarked(Cast<HeapObject>(*Utils::OpenDirectHandle(*local))));
    ref_to = *ref_from;
    EXPECT_TRUE(!ref_from->IsEmpty());
    EXPECT_FALSE(
        state.IsUnmarked(Cast<HeapObject>(*Utils::OpenDirectHandle(*local))));
  }
}

TEST_F(TracedReferenceTest, WriteBarrierForOnHeapMove) {
  if (!v8_flags.incremental_marking) {
    GTEST_SKIP() << "Write barrier tests require incremental marking";
  }

  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    auto ref_from =
        std::make_unique<v8::TracedReference<v8::Object>>(v8_isolate(), local);
    auto ref_to = std::make_unique<v8::TracedReference<v8::Object>>();
    SimulateIncrementalMarking();
    MarkingState state(i_isolate());
    ASSERT_TRUE(
        state.IsUnmarked(Cast<HeapObject>(*Utils::OpenDirectHandle(*local))));
    *ref_to = std::move(*ref_from);
    ASSERT_TRUE(ref_from->IsEmpty());
    EXPECT_FALSE(
        state.IsUnmarked(Cast<HeapObject>(*Utils::OpenDirectHandle(*local))));
  }
}

TEST_F(TracedReferenceTest, WriteBarrierForOnStackMove) {
  if (!v8_flags.incremental_marking) {
    GTEST_SKIP() << "Write barrier tests require incremental marking";
  }

  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  {
    v8::HandleScope handles(v8_isolate());
    v8::Local<v8::Object> local =
        v8::Local<v8::Object>::New(v8_isolate(), v8::Object::New(v8_isolate()));
    auto ref_from =
        std::make_unique<v8::TracedReference<v8::Object>>(v8_isolate(), local);
    v8::TracedReference<v8::Object> ref_to;
    SimulateIncrementalMarking();
    MarkingState state(i_isolate());
    ASSERT_TRUE(
        state.IsUnmarked(Cast<HeapObject>(*Utils::OpenDirectHandle(*local))));
    ref_to = std::move(*ref_from);
    ASSERT_TRUE(ref_from->IsEmpty());
    EXPECT_FALSE(
        state.IsUnmarked(Cast<HeapObject>(*Utils::OpenDirectHandle(*local))));
  }
}

namespace {

// A garbage-collected object that resets its own TracedReference from a
// pre-finalizer. Blink reaches the same shape via
// DOMTimer::Dispose() -> ScheduledAction::Dispose().
class ResettingPreFinalizer final
    : public cppgc::GarbageCollected<ResettingPreFinalizer> {
  CPPGC_USING_PRE_FINALIZER(ResettingPreFinalizer, Dispose);

 public:
  ResettingPreFinalizer(v8::Isolate* isolate, v8::Local<v8::Object> object)
      : ref_(isolate, object) {}

  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(ref_); }

  void Dispose() { ref_.Reset(); }

 private:
  v8::TracedReference<v8::Object> ref_;
};

}  // namespace

using TracedReferencePreFinalizerTest = UnifiedHeapTest;

// A pre-finalizer only runs on an object the collector already found to be
// dead, so that object was never traced, and `ResetDeadNodes()` released its
// TracedNode earlier in the same atomic pause. If the object's pre-finalizer
// resets the TracedReference, it would release its TracedNode a second time.
//
// Note that the pre-finalizer here does not misuse the API: cppgc documents
// that a pre-finalizer "may access the whole object graph, irrespective of
// whether objects are considered dead or alive".
TEST_F(TracedReferencePreFinalizerTest,
       PreFinalizerResetDoesNotDoubleFreeNode) {
  // Settle the heap first so that the node allocated below is the only traced
  // node dying in the next GC, and hence would become the free list head.
  CollectGarbageWithoutEmbedderStack(cppgc::Heap::SweepingType::kAtomic);

  {
    v8::HandleScope handles(v8_isolate());
    // Deliberately not rooted: it must be dead by the next GC.
    cppgc::MakeGarbageCollected<ResettingPreFinalizer>(
        allocation_handle(), v8_isolate(), v8::Object::New(v8_isolate()));
  }

  CollectGarbageWithoutEmbedderStack(cppgc::Heap::SweepingType::kAtomic);

  v8::HandleScope handles(v8_isolate());
  v8::Local<v8::Object> first = v8::Object::New(v8_isolate());
  v8::Local<v8::Object> second = v8::Object::New(v8_isolate());
  ASSERT_TRUE(first != second);

  v8::TracedReference<v8::Object> ref1(v8_isolate(), first);
  v8::TracedReference<v8::Object> ref2(v8_isolate(), second);

  // The two references must be backed by distinct TracedNodes. With the double
  // free, creating `ref2` reused `ref1`'s node and overwrote its object.
  EXPECT_EQ(ref1, first);
  EXPECT_NE(ref1, ref2);
}

namespace {

class ConcurrentMarkerThread final : public v8::base::Thread {
 public:
  ConcurrentMarkerThread(
      cppgc::Visitor& visitor,
      const std::vector<v8::TracedReference<v8::Object>>& refs,
      std::atomic<bool>& stop)
      : v8::base::Thread(Options("ConcurrentMarkerThread")),
        visitor_(visitor),
        refs_(refs),
        stop_(stop) {}

  void Run() override {
    while (!stop_.load(std::memory_order_relaxed)) {
      for (const auto& ref : refs_) {
        visitor_.Trace(ref);
      }
    }
  }

 private:
  cppgc::Visitor& visitor_;
  const std::vector<v8::TracedReference<v8::Object>>& refs_;
  std::atomic<bool>& stop_;
};

}  // namespace

using TracedReferenceUnifiedHeapTest = UnifiedHeapTest;

TEST_F(TracedReferenceUnifiedHeapTest, Regress40059554) {
  // Regression test: https://crbug.com/40059554
  if (!v8_flags.incremental_marking) return;

  static constexpr size_t kNumHandles = 8192;
  v8::HandleScope scope(v8_isolate());
  v8::Local<v8::Object> obj = v8::Object::New(v8_isolate());
  std::vector<v8::TracedReference<v8::Object>> refs(kNumHandles);

  // Populate TracedNodeBlock free lists with zapped nodes.
  for (auto& ref : refs) {
    ref.Reset(v8_isolate(), obj);
  }
  for (auto& ref : refs) {
    ref.Reset();
  }

  SimulateIncrementalMarking(false);

  std::atomic<bool> stop{false};
  ConcurrentMarkerThread marker_thread(cpp_heap().AsBase().marker()->Visitor(),
                                       refs, stop);
  CHECK(marker_thread.Start());

  // Create new references while the concurrent marker is already active. This
  // stress-tests the publication of the new traced handle's node.
  for (auto& ref : refs) {
    ref.Reset(v8_isolate(), obj);
  }

  // Anything below this point is just cleanup. The crash would have already
  // ocurred before the concurrent thread is stopped.
  stop.store(true, std::memory_order_relaxed);
  marker_thread.Join();

  for (auto& ref : refs) {
    ref.Reset();
  }
}

}  // namespace internal
}  // namespace v8

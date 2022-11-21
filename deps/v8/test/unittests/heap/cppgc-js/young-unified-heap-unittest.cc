// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(CPPGC_YOUNG_GENERATION)

#include <memory>

#include "include/cppgc/allocation.h"
#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/testing.h"
#include "include/v8-context.h"
#include "include/v8-cppgc.h"
#include "include/v8-local-handle.h"
#include "include/v8-object.h"
#include "include/v8-traced-handle.h"
#include "src/api/api-inl.h"
#include "src/common/globals.h"
#include "src/heap/cppgc-js/cpp-heap.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/objects/objects-inl.h"
#include "test/common/flag-utils.h"
#include "test/unittests/heap/cppgc-js/unified-heap-utils.h"
#include "test/unittests/heap/heap-utils.h"

namespace v8 {
namespace internal {

namespace {

class Wrappable final : public cppgc::GarbageCollected<Wrappable> {
 public:
  static size_t destructor_callcount;

  ~Wrappable() { destructor_callcount++; }

  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(wrapper_); }

  void SetWrapper(v8::Isolate* isolate, v8::Local<v8::Object> wrapper) {
    wrapper_.Reset(isolate, wrapper);
  }

  TracedReference<v8::Object>& wrapper() { return wrapper_; }

 private:
  TracedReference<v8::Object> wrapper_;
};

size_t Wrappable::destructor_callcount = 0;

class MinorMCEnabler {
 public:
  MinorMCEnabler()
      : minor_mc_(&v8_flags.minor_mc, true),
        cppgc_young_generation_(&v8_flags.cppgc_young_generation, true) {}

 private:
  FlagScope<bool> minor_mc_;
  FlagScope<bool> cppgc_young_generation_;
};

}  // namespace

class YoungUnifiedHeapTest : public MinorMCEnabler, public UnifiedHeapTest {
 public:
  YoungUnifiedHeapTest() {
    // Enable young generation flag and run GC. After the first run the heap
    // will enable minor GC.
    CollectGarbageWithoutEmbedderStack();
  }
};

TEST_F(YoungUnifiedHeapTest, OnlyGC) { CollectYoungGarbageWithEmbedderStack(); }

TEST_F(YoungUnifiedHeapTest, CollectUnreachableCppGCObject) {
  v8::HandleScope scope(v8_isolate());
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);

  cppgc::MakeGarbageCollected<Wrappable>(allocation_handle());
  v8::Local<v8::Object> api_object =
      WrapperHelper::CreateWrapper(context, nullptr, nullptr);
  EXPECT_FALSE(api_object.IsEmpty());

  Wrappable::destructor_callcount = 0;
  CollectYoungGarbageWithoutEmbedderStack(cppgc::Heap::SweepingType::kAtomic);
  EXPECT_EQ(1u, Wrappable::destructor_callcount);
}

TEST_F(YoungUnifiedHeapTest, FindingV8ToCppGCReference) {
  v8::HandleScope scope(v8_isolate());
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);

  uint16_t wrappable_type = WrapperHelper::kTracedEmbedderId;
  auto* wrappable_object =
      cppgc::MakeGarbageCollected<Wrappable>(allocation_handle());
  v8::Local<v8::Object> api_object =
      WrapperHelper::CreateWrapper(context, &wrappable_type, wrappable_object);
  EXPECT_FALSE(api_object.IsEmpty());

  Wrappable::destructor_callcount = 0;
  CollectYoungGarbageWithoutEmbedderStack(cppgc::Heap::SweepingType::kAtomic);
  EXPECT_EQ(0u, Wrappable::destructor_callcount);

  WrapperHelper::ResetWrappableConnection(api_object);
  CollectGarbageWithoutEmbedderStack(cppgc::Heap::SweepingType::kAtomic);
  EXPECT_EQ(1u, Wrappable::destructor_callcount);
}

TEST_F(YoungUnifiedHeapTest, FindingCppGCToV8Reference) {
  v8::HandleScope handle_scope(v8_isolate());
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);

  auto* wrappable_object =
      cppgc::MakeGarbageCollected<Wrappable>(allocation_handle());

  {
    v8::HandleScope inner_handle_scope(v8_isolate());
    auto local = v8::Object::New(v8_isolate());
    EXPECT_TRUE(local->IsObject());
    wrappable_object->SetWrapper(v8_isolate(), local);
  }

  CollectYoungGarbageWithEmbedderStack(cppgc::Heap::SweepingType::kAtomic);
  auto local = wrappable_object->wrapper().Get(v8_isolate());
  EXPECT_TRUE(local->IsObject());
}

TEST_F(YoungUnifiedHeapTest, GenerationalBarrierV8ToCppGCReference) {
  if (i::v8_flags.single_generation) return;

  v8::HandleScope scope(v8_isolate());
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> api_object =
      WrapperHelper::CreateWrapper(context, nullptr, nullptr);
  auto handle_api_object =
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(api_object));

  EXPECT_TRUE(Heap::InYoungGeneration(*handle_api_object));
  CollectAllAvailableGarbage();
  EXPECT_EQ(0u, Wrappable::destructor_callcount);
  EXPECT_FALSE(Heap::InYoungGeneration(*handle_api_object));

  auto* wrappable = cppgc::MakeGarbageCollected<Wrappable>(allocation_handle());
  uint16_t type_info = WrapperHelper::kTracedEmbedderId;
  WrapperHelper::SetWrappableConnection(api_object, &type_info, wrappable);

  Wrappable::destructor_callcount = 0;
  CollectYoungGarbageWithoutEmbedderStack(cppgc::Heap::SweepingType::kAtomic);
  EXPECT_EQ(0u, Wrappable::destructor_callcount);
}

}  // namespace internal
}  // namespace v8

#endif  // defined(CPPGC_YOUNG_GENERATION)

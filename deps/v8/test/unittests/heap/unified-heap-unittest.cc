// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/allocation.h"
#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/platform.h"
#include "src/api/api-inl.h"
#include "src/heap/cppgc-js/cpp-heap.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/heap/unified-heap-utils.h"

namespace v8 {
namespace internal {

namespace {

class Wrappable final : public cppgc::GarbageCollected<Wrappable> {
 public:
  static size_t destructor_callcount;

  ~Wrappable() { destructor_callcount++; }

  void Trace(cppgc::Visitor* visitor) const {}
};

size_t Wrappable::destructor_callcount = 0;

}  // namespace

TEST_F(UnifiedHeapTest, OnlyGC) { CollectGarbageWithEmbedderStack(); }

TEST_F(UnifiedHeapTest, FindingV8ToBlinkReference) {
  v8::HandleScope scope(v8_isolate());
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Object> api_object = WrapperHelper::CreateWrapper(
      context, cppgc::MakeGarbageCollected<Wrappable>(allocation_handle()));
  EXPECT_FALSE(api_object.IsEmpty());
  EXPECT_EQ(0u, Wrappable::destructor_callcount);
  CollectGarbageWithoutEmbedderStack();
  EXPECT_EQ(0u, Wrappable::destructor_callcount);
  WrapperHelper::ResetWrappableConnection(api_object);
  CollectGarbageWithoutEmbedderStack();
  // Calling CollectGarbage twice to force the first GC to finish sweeping.
  CollectGarbageWithoutEmbedderStack();
  EXPECT_EQ(1u, Wrappable::destructor_callcount);
}

}  // namespace internal
}  // namespace v8

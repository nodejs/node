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

namespace v8 {
namespace internal {

namespace {

v8::Local<v8::Object> ConstructTraceableJSApiObject(
    v8::Local<v8::Context> context, void* object) {
  v8::EscapableHandleScope scope(context->GetIsolate());
  v8::Local<v8::FunctionTemplate> function_t =
      v8::FunctionTemplate::New(context->GetIsolate());
  v8::Local<v8::ObjectTemplate> instance_t = function_t->InstanceTemplate();
  instance_t->SetInternalFieldCount(2);
  v8::Local<v8::Function> function =
      function_t->GetFunction(context).ToLocalChecked();
  v8::Local<v8::Object> instance =
      function->NewInstance(context).ToLocalChecked();
  instance->SetAlignedPointerInInternalField(0, object);
  instance->SetAlignedPointerInInternalField(1, object);
  CHECK(!instance.IsEmpty());
  i::Handle<i::JSReceiver> js_obj = v8::Utils::OpenHandle(*instance);
  CHECK_EQ(i::JS_API_OBJECT_TYPE, js_obj->map().instance_type());
  return scope.Escape(instance);
}

void ResetWrappableConnection(v8::Local<v8::Object> api_object) {
  api_object->SetAlignedPointerInInternalField(0, nullptr);
  api_object->SetAlignedPointerInInternalField(1, nullptr);
}

class UnifiedHeapTest : public TestWithHeapInternals {
 public:
  UnifiedHeapTest()
      : saved_incremental_marking_wrappers_(FLAG_incremental_marking_wrappers) {
    FLAG_incremental_marking_wrappers = false;
    cppgc::InitializeProcess(V8::GetCurrentPlatform()->GetPageAllocator());
    cpp_heap_ = std::make_unique<CppHeap>(v8_isolate(), 0);
    heap()->SetEmbedderHeapTracer(&cpp_heap());
  }

  ~UnifiedHeapTest() {
    heap()->SetEmbedderHeapTracer(nullptr);
    FLAG_incremental_marking_wrappers = saved_incremental_marking_wrappers_;
    cppgc::ShutdownProcess();
  }

  CppHeap& cpp_heap() const { return *cpp_heap_.get(); }

  cppgc::AllocationHandle& allocation_handle() {
    return cpp_heap().object_allocator();
  }

 private:
  std::unique_ptr<CppHeap> cpp_heap_;
  bool saved_incremental_marking_wrappers_;
};

class Wrappable final : public cppgc::GarbageCollected<Wrappable> {
 public:
  static size_t destructor_callcount;

  ~Wrappable() { destructor_callcount++; }

  void Trace(cppgc::Visitor* visitor) const {}
};

size_t Wrappable::destructor_callcount = 0;

}  // namespace

TEST_F(UnifiedHeapTest, OnlyGC) { CollectGarbage(OLD_SPACE); }

TEST_F(UnifiedHeapTest, FindingV8ToBlinkReference) {
  v8::HandleScope scope(v8_isolate());
  v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Object> api_object = ConstructTraceableJSApiObject(
      context, cppgc::MakeGarbageCollected<Wrappable>(allocation_handle()));
  EXPECT_FALSE(api_object.IsEmpty());
  EXPECT_EQ(0u, Wrappable::destructor_callcount);
  CollectGarbage(OLD_SPACE);
  EXPECT_EQ(0u, Wrappable::destructor_callcount);
  ResetWrappableConnection(api_object);
  CollectGarbage(OLD_SPACE);
  EXPECT_EQ(1u, Wrappable::destructor_callcount);
}

}  // namespace internal
}  // namespace v8

// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8.h"
#include "src/api.h"
#include "src/objects-inl.h"
#include "src/objects/module.h"
#include "src/objects/script.h"
#include "src/objects/shared-function-info.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace heap {

namespace {

v8::Local<v8::Object> ConstructTraceableJSApiObject(
    v8::Local<v8::Context> context, void* first_field, void* second_field) {
  v8::EscapableHandleScope scope(context->GetIsolate());
  v8::Local<v8::FunctionTemplate> function_t =
      v8::FunctionTemplate::New(context->GetIsolate());
  v8::Local<v8::ObjectTemplate> instance_t = function_t->InstanceTemplate();
  instance_t->SetInternalFieldCount(2);
  v8::Local<v8::Function> function =
      function_t->GetFunction(context).ToLocalChecked();
  v8::Local<v8::Object> instance =
      function->NewInstance(context).ToLocalChecked();
  instance->SetAlignedPointerInInternalField(0, first_field);
  instance->SetAlignedPointerInInternalField(1, second_field);
  CHECK(!instance.IsEmpty());
  i::Handle<i::JSReceiver> js_obj = v8::Utils::OpenHandle(*instance);
  CHECK_EQ(i::JS_API_OBJECT_TYPE, js_obj->map()->instance_type());
  return scope.Escape(instance);
}

class TestEmbedderHeapTracer final : public v8::EmbedderHeapTracer {
 public:
  explicit TestEmbedderHeapTracer(v8::Isolate* isolate) : isolate_(isolate) {}

  void RegisterV8References(
      const std::vector<std::pair<void*, void*>>& embedder_fields) final {
    registered_from_v8_.insert(registered_from_v8_.end(),
                               embedder_fields.begin(), embedder_fields.end());
  }

  void AddReferenceForTracing(v8::Persistent<v8::Object>* persistent) {
    to_register_with_v8_.push_back(persistent);
  }

  bool AdvanceTracing(double deadline_in_ms,
                      AdvanceTracingActions actions) final {
    for (auto persistent : to_register_with_v8_) {
      persistent->RegisterExternalReference(isolate_);
    }
    to_register_with_v8_.clear();
    return false;
  }

  void TracePrologue() final {}
  void TraceEpilogue() final {}
  void AbortTracing() final {}
  void EnterFinalPause() final {}

  bool IsRegisteredFromV8(void* first_field) const {
    for (auto pair : registered_from_v8_) {
      if (pair.first == first_field) return true;
    }
    return false;
  }

 private:
  v8::Isolate* const isolate_;
  std::vector<std::pair<void*, void*>> registered_from_v8_;
  std::vector<v8::Persistent<v8::Object>*> to_register_with_v8_;
};

}  // namespace

TEST(V8RegisteringEmbedderReference) {
  // Tests that wrappers are properly registered with the embedder heap
  // tracer.
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  TestEmbedderHeapTracer tracer(isolate);
  isolate->SetEmbedderHeapTracer(&tracer);
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  void* first_field = reinterpret_cast<void*>(0x2);
  v8::Local<v8::Object> api_object =
      ConstructTraceableJSApiObject(context, first_field, nullptr);
  CHECK(!api_object.IsEmpty());
  CcTest::CollectGarbage(i::OLD_SPACE);
  CHECK(tracer.IsRegisteredFromV8(first_field));
}

TEST(EmbedderRegisteringV8Reference) {
  // Tests that references that are registered by the embedder heap tracer are
  // considered live by V8.
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  TestEmbedderHeapTracer tracer(isolate);
  isolate->SetEmbedderHeapTracer(&tracer);
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  v8::Persistent<v8::Object> g;
  {
    v8::HandleScope inner_scope(isolate);
    v8::Local<v8::Object> o =
        v8::Local<v8::Object>::New(isolate, v8::Object::New(isolate));
    g.Reset(isolate, o);
    g.SetWeak();
  }
  tracer.AddReferenceForTracing(&g);
  CcTest::CollectGarbage(i::OLD_SPACE);
  CHECK(!g.IsEmpty());
}

namespace {

void ResurrectingFinalizer(
    const v8::WeakCallbackInfo<v8::Global<v8::Object>>& data) {
  data.GetParameter()->ClearWeak();
}

}  // namespace

TEST(TracingInRevivedSubgraph) {
  // Tests that wrappers are traced when they are contained with in a subgraph
  // that is revived by a finalizer.
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  TestEmbedderHeapTracer tracer(isolate);
  isolate->SetEmbedderHeapTracer(&tracer);
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  v8::Global<v8::Object> g;
  void* first_field = reinterpret_cast<void*>(0x4);
  {
    v8::HandleScope inner_scope(isolate);
    v8::Local<v8::Object> api_object =
        ConstructTraceableJSApiObject(context, first_field, nullptr);
    CHECK(!api_object.IsEmpty());
    v8::Local<v8::Object> o =
        v8::Local<v8::Object>::New(isolate, v8::Object::New(isolate));
    o->Set(context, v8_str("link"), api_object).FromJust();
    g.Reset(isolate, o);
    g.SetWeak(&g, ResurrectingFinalizer, v8::WeakCallbackType::kFinalizer);
  }
  CcTest::CollectGarbage(i::OLD_SPACE);
  CHECK(tracer.IsRegisteredFromV8(first_field));
}

TEST(TracingInEphemerons) {
  // Tests that wrappers that are part of ephemerons are traced.
  ManualGCScope manual_gc;
  CcTest::InitializeVM();
  v8::Isolate* isolate = CcTest::isolate();
  TestEmbedderHeapTracer tracer(isolate);
  isolate->SetEmbedderHeapTracer(&tracer);
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> key =
      v8::Local<v8::Object>::New(isolate, v8::Object::New(isolate));
  void* first_field = reinterpret_cast<void*>(0x8);
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  Handle<JSWeakMap> weak_map = i_isolate->factory()->NewJSWeakMap();
  {
    v8::HandleScope inner_scope(isolate);
    v8::Local<v8::Object> api_object =
        ConstructTraceableJSApiObject(context, first_field, nullptr);
    CHECK(!api_object.IsEmpty());
    Handle<JSObject> js_key =
        handle(JSObject::cast(*v8::Utils::OpenHandle(*key)));
    Handle<JSReceiver> js_api_object = v8::Utils::OpenHandle(*api_object);
    int32_t hash = js_key->GetOrCreateHash(i_isolate)->value();
    JSWeakCollection::Set(weak_map, js_key, js_api_object, hash);
  }
  CcTest::CollectGarbage(i::OLD_SPACE);
  CHECK(tracer.IsRegisteredFromV8(first_field));
}

}  // namespace heap
}  // namespace internal
}  // namespace v8

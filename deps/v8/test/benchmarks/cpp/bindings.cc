// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/allocation.h"
#include "include/v8-context.h"
#include "include/v8-internal.h"
#include "include/v8-local-handle.h"
#include "include/v8-persistent-handle.h"
#include "include/v8-sandbox.h"
#include "include/v8-template.h"
#include "src/api/api-inl.h"
#include "src/base/macros.h"
#include "src/objects/js-objects-inl.h"
#include "test/benchmarks/cpp/benchmark-utils.h"
#include "third_party/google_benchmark_chrome/src/include/benchmark/benchmark.h"

namespace {

v8::Local<v8::String> v8_str(const char* x) {
  return v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), x).ToLocalChecked();
}

struct WrapperTypeInfo {
  uint16_t embedder_id;
};

struct PerContextData {
  cppgc::AllocationHandle& allocation_handle;
  std::map<WrapperTypeInfo*, v8::Global<v8::ObjectTemplate>> object_templates;
};

class WrappableBase : public cppgc::GarbageCollected<WrappableBase> {
 public:
  virtual WrapperTypeInfo* GetWrapperTypeInfo() = 0;

  void SetWrapper(v8::Isolate* isolate, v8::Local<v8::Value> value) {
    wrapper_.Reset(isolate, value);
  }

  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(wrapper_); }

 private:
  v8::TracedReference<v8::Value> wrapper_;
};

class WrappableValue : public WrappableBase {
 public:
  static WrapperTypeInfo wrapper_type_info;

  WrapperTypeInfo* GetWrapperTypeInfo() override { return &wrapper_type_info; }
};
WrapperTypeInfo WrappableValue::wrapper_type_info{
    v8::benchmarking::kEmbedderId};

class GlobalWrappable : public WrappableBase {
 public:
  static WrapperTypeInfo wrapper_type_info;

  WrapperTypeInfo* GetWrapperTypeInfo() override { return &wrapper_type_info; }

  WrappableValue* GetWrappableValue(
      cppgc::AllocationHandle& allocation_Handle) {
    return cppgc::MakeGarbageCollected<WrappableValue>(allocation_Handle);
  }

  uint16_t GetSmiNumber() { return 17; }
};
WrapperTypeInfo GlobalWrappable::wrapper_type_info{
    v8::benchmarking::kEmbedderId};

v8::Local<v8::ObjectTemplate> GetInstanceTemplateForContext(
    v8::Isolate* isolate, PerContextData* data,
    WrapperTypeInfo* wrapper_type_info, int number_of_internal_fields) {
  auto it = data->object_templates.find((&WrappableValue::wrapper_type_info));
  v8::Local<v8::ObjectTemplate> instance_tpl;
  if (it == data->object_templates.end()) {
    v8::Local<v8::FunctionTemplate> function_template =
        v8::FunctionTemplate::New(isolate);
    auto object_template = function_template->InstanceTemplate();
    object_template->SetInternalFieldCount(number_of_internal_fields);
    data->object_templates.emplace(
        &WrappableValue::wrapper_type_info,
        v8::Global<v8::ObjectTemplate>(isolate, object_template));
    instance_tpl = object_template;
  } else {
    instance_tpl = it->second.Get(isolate);
  }
  return instance_tpl;
}

template <typename ConcreteBindings>
class BindingsBenchmarkBase : public v8::benchmarking::BenchmarkWithIsolate {
 public:
  static void AccessorReturningWrapper(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    // Preamble.
    auto* isolate = info.GetIsolate();
    auto ctx = isolate->GetCurrentContext();
    auto* data = reinterpret_cast<PerContextData*>(
        ctx->GetAlignedPointerFromEmbedderData(v8::benchmarking::kEmbedderId));

    // Unwrap: Get the C++ instance pointer.
    GlobalWrappable* receiver =
        ConcreteBindings::template Unwrap<GlobalWrappable>(isolate,
                                                           info.This());
    // Invoke the actual operation.
    WrappableValue* return_value =
        receiver->GetWrappableValue(data->allocation_handle);
    // Wrap the C++ value with a JS value.
    auto v8_wrapper = ConcreteBindings::Wrap(
        isolate, ctx, data, return_value, &WrappableValue::wrapper_type_info);
    // Return the JS value back to V8.
    info.GetReturnValue().SetNonEmpty(v8_wrapper);
  }

  static void AccessorReturningSmi(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    // Preamble.
    auto* isolate = info.GetIsolate();

    // Unwrap: Get the C++ instance pointer.
    GlobalWrappable* receiver =
        ConcreteBindings::template Unwrap<GlobalWrappable>(isolate,
                                                           info.This());
    // Invoke the actual operation.
    uint16_t return_value = receiver->GetSmiNumber();

    // Return Smi.
    info.GetReturnValue().Set(return_value);
  }

  void SetUp(::benchmark::State& state) override {
    auto* isolate = v8_isolate();
    v8::HandleScope handle_scope(isolate);

    auto proxy_template_function = v8::FunctionTemplate::New(isolate);
    auto object_template = proxy_template_function->InstanceTemplate();
    ConcreteBindings::SetupContextTemplate(object_template);

    object_template->SetAccessorProperty(
        v8_str("accessorReturningWrapper"),
        v8::FunctionTemplate::New(isolate, &AccessorReturningWrapper));
    object_template->SetAccessorProperty(
        v8_str("accessorReturningSmi"),
        v8::FunctionTemplate::New(isolate, &AccessorReturningSmi));

    v8::Local<v8::Context> context =
        v8::Context::New(isolate, nullptr, object_template);

    context->SetAlignedPointerInEmbedderData(
        0, new PerContextData{allocation_handle(), {}});

    auto* global_wrappable =
        cppgc::MakeGarbageCollected<GlobalWrappable>(allocation_handle());
    CHECK(context->Global()->IsApiWrapper());

    ConcreteBindings::AssociateWithWrapper(isolate, context->Global(),
                                           &GlobalWrappable::wrapper_type_info,
                                           global_wrappable);
    context_.Reset(isolate, context);
    context->Enter();
  }

  void TearDown(::benchmark::State& state) override {
    auto* isolate = v8_isolate();
    v8::HandleScope handle_scope(isolate);
    auto context = context_.Get(isolate);
    delete reinterpret_cast<PerContextData*>(
        context->GetAlignedPointerFromEmbedderData(
            v8::benchmarking::kEmbedderId));
    context->Exit();
    context_.Reset();
  }

  v8::Local<v8::Script> CompileBenchmarkScript(const char* source) {
    v8::EscapableHandleScope handle_scope(v8_isolate());
    v8::Local<v8::Context> context = v8_context();
    v8::Local<v8::String> v8_source = v8_str(source);
    v8::Local<v8::Script> script =
        v8::Script::Compile(context, v8_source).ToLocalChecked();
    return handle_scope.Escape(script);
  }

 protected:
  v8::Local<v8::Context> v8_context() { return context_.Get(v8_isolate()); }

  v8::Global<v8::Context> context_;
};

class OldBindings : public BindingsBenchmarkBase<OldBindings> {
 public:
  static V8_INLINE v8::Local<v8::Object> Wrap(v8::Isolate* isolate,
                                              v8::Local<v8::Context>& context,
                                              PerContextData* data,
                                              WrappableBase* wrappable,
                                              WrapperTypeInfo* info) {
    // Allocate a new JS wrapper.
    v8::Local<v8::ObjectTemplate> wrapper_instance_tpl =
        GetInstanceTemplateForContext(isolate, data,
                                      &WrappableValue::wrapper_type_info, 2);
    auto v8_wrapper =
        wrapper_instance_tpl->NewInstance(context).ToLocalChecked();
    AssociateWithWrapper(isolate, v8_wrapper, info, wrappable);
    return v8_wrapper;
  }

  static V8_INLINE void AssociateWithWrapper(v8::Isolate* isolate,
                                             v8::Local<v8::Object> v8_wrapper,
                                             WrapperTypeInfo* info,
                                             WrappableBase* wrappable) {
    // Set V8 to C++ reference.
    int indices[] = {v8::benchmarking::kTypeOffset,
                     v8::benchmarking::kInstanceOffset};
    void* values[] = {info, wrappable};
    v8_wrapper->SetAlignedPointerInInternalFields(2, indices, values);
    // Set C++ to V8 reference.
    wrappable->SetWrapper(isolate, v8_wrapper);
  }

  template <typename T>
  static V8_INLINE T* Unwrap(v8::Isolate* isolate, v8::Local<v8::Object> thiz) {
    return reinterpret_cast<T*>(thiz->GetAlignedPointerFromInternalField(
        v8::benchmarking::kInstanceOffset));
  }

  static void SetupContextTemplate(
      v8::Local<v8::ObjectTemplate>& object_template) {
    object_template->SetInternalFieldCount(2);
  }
};

class NewBindings : public BindingsBenchmarkBase<NewBindings> {
 public:
  static V8_INLINE v8::Local<v8::Object> Wrap(v8::Isolate* isolate,
                                              v8::Local<v8::Context>& context,
                                              PerContextData* data,
                                              WrappableBase* wrappable,
                                              WrapperTypeInfo* info) {
    // Allocate a new JS wrapper.
    v8::Local<v8::ObjectTemplate> wrapper_instance_tpl =
        GetInstanceTemplateForContext(isolate, data,
                                      &WrappableValue::wrapper_type_info, 0);
    auto v8_wrapper =
        wrapper_instance_tpl->NewInstance(context).ToLocalChecked();
    AssociateWithWrapper(isolate, v8_wrapper, info, wrappable);
    return v8_wrapper;
  }

  static V8_INLINE void AssociateWithWrapper(v8::Isolate* isolate,
                                             v8::Local<v8::Object> v8_wrapper,
                                             WrapperTypeInfo* info,
                                             WrappableBase* wrappable) {
    // Set V8 to C++ reference.
    v8::Object::Wrap<v8::CppHeapPointerTag::kDefaultTag>(isolate, v8_wrapper,
                                                         wrappable);
    // Set C++ to V8 reference.
    wrappable->SetWrapper(isolate, v8_wrapper);
  }

  template <typename T>
  static V8_INLINE T* Unwrap(v8::Isolate* isolate, v8::Local<v8::Object> thiz) {
    return v8::Object::Unwrap<v8::CppHeapPointerTag::kDefaultTag, T>(isolate,
                                                                     thiz);
  }

  static void SetupContextTemplate(
      v8::Local<v8::ObjectTemplate>& object_template) {}
};

}  // namespace

const char* kScriptInvocingAccessorReturingWrapper =
    "function invoke() { globalThis.accessorReturningWrapper; }"
    "for (var i =0; i < 1_000; i++) invoke();";

BENCHMARK_F(OldBindings, AccessorReturningWrapper)(benchmark::State& st) {
  v8::HandleScope handle_scope(v8_isolate());
  v8::Local<v8::Context> context = v8_context();
  v8::Local<v8::Script> script =
      CompileBenchmarkScript(kScriptInvocingAccessorReturingWrapper);
  v8::HandleScope benchmark_handle_scope(v8_isolate());
  for (auto _ : st) {
    USE(_);
    v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK_F(NewBindings, AccessorReturningWrapper)(benchmark::State& st) {
  v8::HandleScope handle_scope(v8_isolate());
  v8::Local<v8::Context> context = v8_context();
  v8::Local<v8::Script> script =
      CompileBenchmarkScript(kScriptInvocingAccessorReturingWrapper);
  v8::HandleScope benchmark_handle_scope(v8_isolate());
  for (auto _ : st) {
    USE(_);
    v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();
    benchmark::DoNotOptimize(result);
  }
}

const char* kScriptInvocingAccessorReturingSmi =
    "function invoke() { globalThis.accessorReturningSmi; }"
    "for (var i =0; i < 1_000; i++) invoke();";

BENCHMARK_F(OldBindings, AccessorReturningSmi)(benchmark::State& st) {
  v8::HandleScope handle_scope(v8_isolate());
  v8::Local<v8::Context> context = v8_context();
  v8::Local<v8::Script> script =
      CompileBenchmarkScript(kScriptInvocingAccessorReturingSmi);
  v8::HandleScope benchmark_handle_scope(v8_isolate());
  for (auto _ : st) {
    USE(_);
    v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK_F(NewBindings, AccessorReturningSmi)(benchmark::State& st) {
  v8::HandleScope handle_scope(v8_isolate());
  v8::Local<v8::Context> context = v8_context();
  v8::Local<v8::Script> script =
      CompileBenchmarkScript(kScriptInvocingAccessorReturingSmi);
  v8::HandleScope benchmark_handle_scope(v8_isolate());
  for (auto _ : st) {
    USE(_);
    v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();
    benchmark::DoNotOptimize(result);
  }
}

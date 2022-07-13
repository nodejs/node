// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/extensions/gc-extension.h"

#include "include/v8-isolate.h"
#include "include/v8-object.h"
#include "include/v8-persistent-handle.h"
#include "include/v8-primitive.h"
#include "include/v8-template.h"
#include "src/base/optional.h"
#include "src/base/platform/platform.h"
#include "src/execution/isolate.h"
#include "src/heap/heap.h"
#include "src/tasks/cancelable-task.h"

namespace v8 {
namespace internal {

namespace {

enum class ExecutionType { kAsync, kSync };

struct GCOptions {
  v8::Isolate::GarbageCollectionType type;
  ExecutionType execution;
};

Maybe<bool> IsProperty(v8::Isolate* isolate, v8::Local<v8::Context> ctx,
                       v8::Local<v8::Object> object, const char* key,
                       const char* value) {
  auto k = v8::String::NewFromUtf8(isolate, key).ToLocalChecked();
  // Get will return undefined for non-existing keys which will make
  // StrictEquals fail.
  auto maybe_property = object->Get(ctx, k);
  if (maybe_property.IsEmpty()) return Nothing<bool>();
  return Just<bool>(maybe_property.ToLocalChecked()->StrictEquals(
      v8::String::NewFromUtf8(isolate, value).ToLocalChecked()));
}

Maybe<GCOptions> Parse(v8::Isolate* isolate,
                       const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Default values.
  auto options =
      GCOptions{v8::Isolate::GarbageCollectionType::kFullGarbageCollection,
                ExecutionType::kSync};
  bool found_options_object = false;

  if (args.Length() > 0 && args[0]->IsObject()) {
    v8::HandleScope scope(isolate);
    auto ctx = isolate->GetCurrentContext();
    auto param = v8::Local<v8::Object>::Cast(args[0]);
    auto maybe_type = IsProperty(isolate, ctx, param, "type", "minor");
    if (maybe_type.IsNothing()) return Nothing<GCOptions>();
    if (maybe_type.ToChecked()) {
      found_options_object = true;
      options.type =
          v8::Isolate::GarbageCollectionType::kMinorGarbageCollection;
    }
    auto maybe_execution =
        IsProperty(isolate, ctx, param, "execution", "async");
    if (maybe_execution.IsNothing()) return Nothing<GCOptions>();
    if (maybe_execution.ToChecked()) {
      found_options_object = true;
      options.execution = ExecutionType::kAsync;
    }
  }

  // If no options object is present default to legacy behavior.
  if (!found_options_object) {
    options.type =
        args[0]->BooleanValue(isolate)
            ? v8::Isolate::GarbageCollectionType::kMinorGarbageCollection
            : v8::Isolate::GarbageCollectionType::kFullGarbageCollection;
  }

  return Just<GCOptions>(options);
}

void InvokeGC(v8::Isolate* isolate, ExecutionType execution_type,
              v8::Isolate::GarbageCollectionType type) {
  Heap* heap = reinterpret_cast<Isolate*>(isolate)->heap();
  switch (type) {
    case v8::Isolate::GarbageCollectionType::kMinorGarbageCollection:
      heap->CollectGarbage(i::NEW_SPACE, i::GarbageCollectionReason::kTesting,
                           kGCCallbackFlagForced);
      break;
    case v8::Isolate::GarbageCollectionType::kFullGarbageCollection:
      EmbedderStackStateScope stack_scope(
          heap,
          execution_type == ExecutionType::kAsync
              ? EmbedderStackStateScope::kImplicitThroughTask
              : EmbedderStackStateScope::kExplicitInvocation,
          execution_type == ExecutionType::kAsync
              ? v8::EmbedderHeapTracer::EmbedderStackState::kNoHeapPointers
              : v8::EmbedderHeapTracer::EmbedderStackState::
                    kMayContainHeapPointers);
      heap->PreciseCollectAllGarbage(i::Heap::kNoGCFlags,
                                     i::GarbageCollectionReason::kTesting,
                                     kGCCallbackFlagForced);
      break;
  }
}

class AsyncGC final : public CancelableTask {
 public:
  ~AsyncGC() final = default;

  AsyncGC(v8::Isolate* isolate, v8::Local<v8::Promise::Resolver> resolver,
          v8::Isolate::GarbageCollectionType type)
      : CancelableTask(reinterpret_cast<Isolate*>(isolate)),
        isolate_(isolate),
        ctx_(isolate, isolate->GetCurrentContext()),
        resolver_(isolate, resolver),
        type_(type) {}
  AsyncGC(const AsyncGC&) = delete;
  AsyncGC& operator=(const AsyncGC&) = delete;

  void RunInternal() final {
    v8::HandleScope scope(isolate_);
    InvokeGC(isolate_, ExecutionType::kAsync, type_);
    auto resolver = v8::Local<v8::Promise::Resolver>::New(isolate_, resolver_);
    auto ctx = Local<v8::Context>::New(isolate_, ctx_);
    resolver->Resolve(ctx, v8::Undefined(isolate_)).ToChecked();
  }

 private:
  v8::Isolate* isolate_;
  v8::Persistent<v8::Context> ctx_;
  v8::Persistent<v8::Promise::Resolver> resolver_;
  v8::Isolate::GarbageCollectionType type_;
};

}  // namespace

v8::Local<v8::FunctionTemplate> GCExtension::GetNativeFunctionTemplate(
    v8::Isolate* isolate, v8::Local<v8::String> str) {
  return v8::FunctionTemplate::New(isolate, GCExtension::GC);
}

void GCExtension::GC(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();

  // Immediate bailout if no arguments are provided.
  if (args.Length() == 0) {
    InvokeGC(isolate, ExecutionType::kSync,
             v8::Isolate::GarbageCollectionType::kFullGarbageCollection);
    return;
  }

  auto maybe_options = Parse(isolate, args);
  if (maybe_options.IsNothing()) return;
  GCOptions options = maybe_options.ToChecked();
  switch (options.execution) {
    case ExecutionType::kSync:
      InvokeGC(isolate, ExecutionType::kSync, options.type);
      break;
    case ExecutionType::kAsync: {
      v8::HandleScope scope(isolate);
      auto resolver = v8::Promise::Resolver::New(isolate->GetCurrentContext())
                          .ToLocalChecked();
      args.GetReturnValue().Set(resolver->GetPromise());
      auto task_runner =
          V8::GetCurrentPlatform()->GetForegroundTaskRunner(isolate);
      CHECK(task_runner->NonNestableTasksEnabled());
      task_runner->PostNonNestableTask(
          std::make_unique<AsyncGC>(isolate, resolver, options.type));
    } break;
  }
}

}  // namespace internal
}  // namespace v8

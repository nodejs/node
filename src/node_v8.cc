// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "node.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"
#include "v8.h"

namespace node {

using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::GCCallbackFlags;
using v8::GCType;
using v8::Handle;
using v8::HandleScope;
using v8::HeapStatistics;
using v8::Isolate;
using v8::Local;
using v8::Null;
using v8::Number;
using v8::Object;
using v8::Uint32;
using v8::Value;
using v8::kGCTypeAll;
using v8::kGCTypeMarkSweepCompact;
using v8::kGCTypeScavenge;


void Environment::IsolateData::BeforeGarbageCollection(Isolate* isolate,
                                                       GCType type,
                                                       GCCallbackFlags flags) {
  Get(isolate)->BeforeGarbageCollection(type, flags);
}


void Environment::IsolateData::AfterGarbageCollection(Isolate* isolate,
                                                      GCType type,
                                                      GCCallbackFlags flags) {
  Get(isolate)->AfterGarbageCollection(type, flags);
}


void Environment::IsolateData::BeforeGarbageCollection(GCType type,
                                                       GCCallbackFlags flags) {
  gc_info_before_ = GCInfo(isolate(), type, flags, uv_hrtime());
}


void Environment::IsolateData::AfterGarbageCollection(GCType type,
                                                      GCCallbackFlags flags) {
  gc_info_after_ = GCInfo(isolate(), type, flags, uv_hrtime());

  // The copy upfront and the remove-then-insert is to avoid corrupting the
  // list when the callback removes itself from it.  QUEUE_FOREACH() is unsafe
  // when the list is mutated while being walked.
  ASSERT(QUEUE_EMPTY(&gc_tracker_queue_) == false);
  QUEUE queue;
  QUEUE* q = QUEUE_HEAD(&gc_tracker_queue_);
  QUEUE_SPLIT(&gc_tracker_queue_, q, &queue);
  while (QUEUE_EMPTY(&queue) == false) {
    q = QUEUE_HEAD(&queue);
    QUEUE_REMOVE(q);
    QUEUE_INSERT_TAIL(&gc_tracker_queue_, q);
    Environment* env = ContainerOf(&Environment::gc_tracker_queue_, q);
    env->AfterGarbageCollectionCallback(&gc_info_before_, &gc_info_after_);
  }
}


void Environment::IsolateData::StartGarbageCollectionTracking(
    Environment* env) {
  if (QUEUE_EMPTY(&gc_tracker_queue_)) {
    isolate()->AddGCPrologueCallback(BeforeGarbageCollection, v8::kGCTypeAll);
    isolate()->AddGCEpilogueCallback(AfterGarbageCollection, v8::kGCTypeAll);
  }
  ASSERT(QUEUE_EMPTY(&env->gc_tracker_queue_) == true);
  QUEUE_INSERT_TAIL(&gc_tracker_queue_, &env->gc_tracker_queue_);
}


void Environment::IsolateData::StopGarbageCollectionTracking(Environment* env) {
  ASSERT(QUEUE_EMPTY(&env->gc_tracker_queue_) == false);
  QUEUE_REMOVE(&env->gc_tracker_queue_);
  QUEUE_INIT(&env->gc_tracker_queue_);
  if (QUEUE_EMPTY(&gc_tracker_queue_)) {
    isolate()->RemoveGCPrologueCallback(BeforeGarbageCollection);
    isolate()->RemoveGCEpilogueCallback(AfterGarbageCollection);
  }
}


// Considering a memory constrained environment, creating more objects is less
// than ideal
void Environment::AfterGarbageCollectionCallback(const GCInfo* before,
                                                 const GCInfo* after) {
  HandleScope handle_scope(isolate());
  Context::Scope context_scope(context());
  Local<Value> argv[] = { Object::New(isolate()), Object::New(isolate()) };
  const GCInfo* infov[] = { before, after };
  for (unsigned i = 0; i < ARRAY_SIZE(argv); i += 1) {
    Local<Object> obj = argv[i].As<Object>();
    const GCInfo* info = infov[i];
    switch (info->type()) {
      case kGCTypeScavenge:
        obj->Set(type_string(), scavenge_string());
        break;
      case kGCTypeMarkSweepCompact:
        obj->Set(type_string(), mark_sweep_compact_string());
        break;
      default:
        UNREACHABLE();
    }
    obj->Set(flags_string(), Uint32::NewFromUnsigned(isolate(), info->flags()));
    obj->Set(timestamp_string(), Number::New(isolate(), info->timestamp()));
    // TODO(trevnorris): Setting many object properties in C++ is a significant
    // performance hit. Redo this to pass the results to JS and create/set the
    // properties there.
#define V(name)                                                               \
    do {                                                                      \
      obj->Set(name ## _string(),                                             \
               Uint32::NewFromUnsigned(isolate(), info->stats()->name()));    \
    } while (0)
    V(total_heap_size);
    V(total_heap_size_executable);
    V(total_physical_size);
    V(used_heap_size);
    V(heap_size_limit);
#undef V
  }
  MakeCallback(this,
               Null(isolate()),
               gc_info_callback_function(),
               ARRAY_SIZE(argv),
               argv);
}


void Environment::StartGarbageCollectionTracking(Local<Function> callback) {
  ASSERT(gc_info_callback_function().IsEmpty() == true);
  set_gc_info_callback_function(callback);
  isolate_data()->StartGarbageCollectionTracking(this);
}


void Environment::StopGarbageCollectionTracking() {
  ASSERT(gc_info_callback_function().IsEmpty() == false);
  isolate_data()->StopGarbageCollectionTracking(this);
  set_gc_info_callback_function(Local<Function>());
}


void StartGarbageCollectionTracking(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsFunction() == true);
  HandleScope handle_scope(args.GetIsolate());
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  env->StartGarbageCollectionTracking(args[0].As<Function>());
}


void GetHeapStatistics(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope handle_scope(isolate);
  Environment* env = Environment::GetCurrent(isolate);
  HeapStatistics s;
  isolate->GetHeapStatistics(&s);
  Local<Object> info = Object::New(isolate);
  // TODO(trevnorris): Setting many object properties in C++ is a significant
  // performance hit. Redo this to pass the results to JS and create/set the
  // properties there.
#define V(name)                                                               \
  info->Set(env->name ## _string(), Uint32::NewFromUnsigned(isolate, s.name()))
  V(total_heap_size);
  V(total_heap_size_executable);
  V(total_physical_size);
  V(used_heap_size);
  V(heap_size_limit);
#undef V
  args.GetReturnValue().Set(info);
}


void StopGarbageCollectionTracking(const FunctionCallbackInfo<Value>& args) {
  HandleScope handle_scope(args.GetIsolate());
  Environment::GetCurrent(args.GetIsolate())->StopGarbageCollectionTracking();
}


void InitializeV8Bindings(Handle<Object> target,
                          Handle<Value> unused,
                          Handle<Context> context) {
  NODE_SET_METHOD(target,
                  "startGarbageCollectionTracking",
                  StartGarbageCollectionTracking);
  NODE_SET_METHOD(target,
                  "stopGarbageCollectionTracking",
                  StopGarbageCollectionTracking);
  NODE_SET_METHOD(target, "getHeapStatistics", GetHeapStatistics);
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(v8, node::InitializeV8Bindings)

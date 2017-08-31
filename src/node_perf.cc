#include "node.h"
#include "v8.h"
#include "env.h"
#include "env-inl.h"
#include "node_perf.h"
#include "uv.h"

#include <vector>

namespace node {
namespace performance {

using v8::Array;
using v8::ArrayBuffer;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Name;
using v8::Object;
using v8::ObjectTemplate;
using v8::PropertyCallbackInfo;
using v8::String;
using v8::Value;

const uint64_t timeOrigin = PERFORMANCE_NOW();
uint64_t performance_node_start;
uint64_t performance_v8_start;

uint64_t performance_last_gc_start_mark_ = 0;
v8::GCType performance_last_gc_type_ = v8::GCType::kGCTypeAll;

void PerformanceEntry::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Utf8Value name(isolate, args[0]);
  Utf8Value type(isolate, args[1]);
  uint64_t now = PERFORMANCE_NOW();
  new PerformanceEntry(env, args.This(), *name, *type, now, now);
}

void PerformanceEntry::NotifyObservers(Environment* env,
                                       PerformanceEntry* entry) {
  uint32_t* observers = env->performance_state()->observers;
  PerformanceEntryType type = ToPerformanceEntryTypeEnum(entry->type().c_str());
  if (observers == nullptr ||
      type == NODE_PERFORMANCE_ENTRY_TYPE_INVALID ||
      !observers[type]) {
    return;
  }
  Local<Context> context = env->context();
  Isolate* isolate = env->isolate();
  Local<Value> argv = entry->object();
  env->performance_entry_callback()->Call(context,
                                          v8::Undefined(isolate),
                                          1, &argv).ToLocalChecked();
}

void Mark(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  Isolate* isolate = env->isolate();
  Utf8Value name(isolate, args[0]);
  uint64_t now = PERFORMANCE_NOW();
  auto marks = env->performance_marks();
  (*marks)[*name] = now;

  // TODO(jasnell): Once Tracing API is fully implemented, this should
  // record a trace event also.

  Local<Function> fn = env->performance_entry_template();
  Local<Object> obj = fn->NewInstance(context).ToLocalChecked();
  new PerformanceEntry(env, obj, *name, "mark", now, now);
  args.GetReturnValue().Set(obj);
}

inline uint64_t GetPerformanceMark(Environment* env, std::string name) {
  auto marks = env->performance_marks();
  auto res = marks->find(name);
  return res != marks->end() ? res->second : 0;
}

void Measure(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  Isolate* isolate = env->isolate();
  Utf8Value name(isolate, args[0]);
  Utf8Value startMark(isolate, args[1]);
  Utf8Value endMark(isolate, args[2]);

  double* milestones = env->performance_state()->milestones;

  uint64_t startTimestamp = timeOrigin;
  uint64_t start = GetPerformanceMark(env, *startMark);
  if (start != 0) {
    startTimestamp = start;
  } else {
    PerformanceMilestone milestone = ToPerformanceMilestoneEnum(*startMark);
    if (milestone != NODE_PERFORMANCE_MILESTONE_INVALID)
      startTimestamp = milestones[milestone];
  }

  uint64_t endTimestamp = GetPerformanceMark(env, *endMark);
  if (endTimestamp == 0) {
    PerformanceMilestone milestone = ToPerformanceMilestoneEnum(*endMark);
    if (milestone != NODE_PERFORMANCE_MILESTONE_INVALID)
      endTimestamp = milestones[milestone];
  }

  if (endTimestamp < startTimestamp)
    endTimestamp = startTimestamp;

  // TODO(jasnell): Once Tracing API is fully implemented, this should
  // record a trace event also.

  Local<Function> fn = env->performance_entry_template();
  Local<Object> obj = fn->NewInstance(context).ToLocalChecked();
  new PerformanceEntry(env, obj, *name, "measure",
                       startTimestamp, endTimestamp);
  args.GetReturnValue().Set(obj);
}

void GetPerformanceEntryName(const Local<String> prop,
                             const PropertyCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  PerformanceEntry* entry;
  ASSIGN_OR_RETURN_UNWRAP(&entry, info.Holder());
  info.GetReturnValue().Set(
    String::NewFromUtf8(isolate, entry->name().c_str(), String::kNormalString));
}

void GetPerformanceEntryType(const Local<String> prop,
                             const PropertyCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
  PerformanceEntry* entry;
  ASSIGN_OR_RETURN_UNWRAP(&entry, info.Holder());
  info.GetReturnValue().Set(
    String::NewFromUtf8(isolate, entry->type().c_str(), String::kNormalString));
}

void GetPerformanceEntryStartTime(const Local<String> prop,
                                  const PropertyCallbackInfo<Value>& info) {
  PerformanceEntry* entry;
  ASSIGN_OR_RETURN_UNWRAP(&entry, info.Holder());
  info.GetReturnValue().Set(entry->startTime());
}

void GetPerformanceEntryDuration(const Local<String> prop,
                                 const PropertyCallbackInfo<Value>& info) {
  PerformanceEntry* entry;
  ASSIGN_OR_RETURN_UNWRAP(&entry, info.Holder());
  info.GetReturnValue().Set(entry->duration());
}

void MarkMilestone(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  double* milestones = env->performance_state()->milestones;
  PerformanceMilestone milestone =
      static_cast<PerformanceMilestone>(
          args[0]->Int32Value(context).ToChecked());
  if (milestone != NODE_PERFORMANCE_MILESTONE_INVALID) {
    milestones[milestone] = PERFORMANCE_NOW();
  }
}

void SetupPerformanceObservers(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsFunction());
  env->set_performance_entry_callback(args[0].As<Function>());
}

inline void PerformanceGCCallback(uv_async_t* handle) {
  PerformanceEntry::Data* data =
      static_cast<PerformanceEntry::Data*>(handle->data);
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);
  Environment* env = Environment::GetCurrent(isolate);
  Local<Context> context = env->context();
  Local<Function> fn;
  Local<Object> obj;
  PerformanceGCKind kind = static_cast<PerformanceGCKind>(data->data());

  uint32_t* observers = env->performance_state()->observers;
  if (!observers[NODE_PERFORMANCE_ENTRY_TYPE_GC]) {
    goto cleanup;
  }

  fn = env->performance_entry_template();
  obj = fn->NewInstance(context).ToLocalChecked();
  obj->Set(context,
           FIXED_ONE_BYTE_STRING(isolate, "kind"),
           Integer::New(isolate, kind)).FromJust();
  new PerformanceEntry(env, obj, data);

 cleanup:
  delete data;
  auto closeCB = [](uv_handle_t* handle) { delete handle; };
  uv_close(reinterpret_cast<uv_handle_t*>(handle), closeCB);
}

inline void MarkGarbageCollectionStart(Isolate* isolate,
                                       v8::GCType type,
                                       v8::GCCallbackFlags flags) {
  performance_last_gc_start_mark_ = PERFORMANCE_NOW();
  performance_last_gc_type_ = type;
}

inline void MarkGarbageCollectionEnd(Isolate* isolate,
                                     v8::GCType type,
                                     v8::GCCallbackFlags flags) {
  uv_async_t *async = new uv_async_t;
  async->data =
      new PerformanceEntry::Data("gc", "gc",
                                 performance_last_gc_start_mark_,
                                 PERFORMANCE_NOW(), type);
  uv_async_init(uv_default_loop(), async, PerformanceGCCallback);
  uv_async_send(async);
}

inline void SetupGarbageCollectionTracking(Isolate* isolate) {
  isolate->AddGCPrologueCallback(MarkGarbageCollectionStart);
  isolate->AddGCEpilogueCallback(MarkGarbageCollectionEnd);
}

inline Local<Value> GetName(Local<Function> fn) {
  Local<Value> val = fn->GetDebugName();
  if (val.IsEmpty() || val->IsUndefined()) {
    Local<Value> boundFunction = fn->GetBoundFunction();
    if (!boundFunction.IsEmpty() && !boundFunction->IsUndefined()) {
      val = GetName(boundFunction.As<Function>());
    }
  }
  return val;
}

void TimerFunctionCall(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  Environment* env = Environment::GetCurrent(isolate);
  Local<Context> context = env->context();
  Local<Function> fn = args.Data().As<Function>();
  size_t count = args.Length();
  size_t idx;
  std::vector<Local<Value>> call_args;
  for (size_t i = 0; i < count; ++i) {
    call_args.push_back(args[i]);
  }

  Utf8Value name(isolate, GetName(fn));

  uint64_t start;
  uint64_t end;
  v8::TryCatch try_catch(isolate);
  if (args.IsConstructCall()) {
    start = PERFORMANCE_NOW();
    v8::MaybeLocal<Object> ret = fn->NewInstance(context,
                                                 call_args.size(),
                                                 call_args.data());
    end = PERFORMANCE_NOW();
    if (ret.IsEmpty()) {
      try_catch.ReThrow();
      return;
    }
    args.GetReturnValue().Set(ret.ToLocalChecked());
  } else {
    start = PERFORMANCE_NOW();
    v8::MaybeLocal<Value> ret = fn->Call(context,
                                         args.This(),
                                         call_args.size(),
                                         call_args.data());
    end = PERFORMANCE_NOW();
    if (ret.IsEmpty()) {
      try_catch.ReThrow();
      return;
    }
    args.GetReturnValue().Set(ret.ToLocalChecked());
  }


  uint32_t* observers = env->performance_state()->observers;
  if (!observers[NODE_PERFORMANCE_ENTRY_TYPE_FUNCTION])
    return;

  Local<Function> ctor = env->performance_entry_template();
  v8::MaybeLocal<Object> instance = ctor->NewInstance(context);
  Local<Object> obj = instance.ToLocalChecked();
  for (idx = 0; idx < count; idx++) {
    obj->Set(context, idx, args[idx]).ToChecked();
  }
  new PerformanceEntry(env, obj, *name, "function", start, end);
}

void Timerify(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  CHECK(args[0]->IsFunction());
  CHECK(args[1]->IsNumber());
  Local<Function> fn = args[0].As<Function>();
  int length = args[1]->IntegerValue(context).ToChecked();
  Local<Function> wrap =
      Function::New(context, TimerFunctionCall, fn, length).ToLocalChecked();
  args.GetReturnValue().Set(wrap);
}

void Init(Local<Object> target,
          Local<Value> unused,
          Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();
  performance_state* state = env->performance_state();
  auto state_ab = ArrayBuffer::New(isolate, state, sizeof(*state));

  #define SET_STATE_TYPEDARRAY(name, type, field)                         \
    target->Set(context,                                                  \
                FIXED_ONE_BYTE_STRING(isolate, (name)),                   \
                type::New(state_ab,                                       \
                          offsetof(performance_state, field),             \
                          arraysize(state->field)))                       \
                                    .FromJust()
    SET_STATE_TYPEDARRAY("observerCounts", v8::Uint32Array, observers);
    SET_STATE_TYPEDARRAY("milestones", v8::Float64Array, milestones);
  #undef SET_STATE_TYPEDARRAY

  Local<String> performanceEntryString =
      FIXED_ONE_BYTE_STRING(isolate, "PerformanceEntry");

  Local<FunctionTemplate> pe = env->NewFunctionTemplate(PerformanceEntry::New);
  pe->InstanceTemplate()->SetInternalFieldCount(1);
  pe->SetClassName(performanceEntryString);
  Local<ObjectTemplate> ot = pe->InstanceTemplate();
  ot->SetAccessor(env->name_string(), GetPerformanceEntryName);
  ot->SetAccessor(FIXED_ONE_BYTE_STRING(isolate, "entryType"),
                  GetPerformanceEntryType);
  ot->SetAccessor(FIXED_ONE_BYTE_STRING(isolate, "startTime"),
                  GetPerformanceEntryStartTime);
  ot->SetAccessor(FIXED_ONE_BYTE_STRING(isolate, "duration"),
                  GetPerformanceEntryDuration);
  Local<Function> fn = pe->GetFunction();
  target->Set(performanceEntryString, fn);
  env->set_performance_entry_template(fn);

  env->SetMethod(target, "mark", Mark);
  env->SetMethod(target, "measure", Measure);
  env->SetMethod(target, "markMilestone", MarkMilestone);
  env->SetMethod(target, "setupObservers", SetupPerformanceObservers);
  env->SetMethod(target, "timerify", Timerify);

  Local<Object> constants = Object::New(isolate);

  NODE_DEFINE_CONSTANT(constants, NODE_PERFORMANCE_GC_MAJOR);
  NODE_DEFINE_CONSTANT(constants, NODE_PERFORMANCE_GC_MINOR);
  NODE_DEFINE_CONSTANT(constants, NODE_PERFORMANCE_GC_INCREMENTAL);
  NODE_DEFINE_CONSTANT(constants, NODE_PERFORMANCE_GC_WEAKCB);

#define V(name, _)                                                            \
  NODE_DEFINE_HIDDEN_CONSTANT(constants, NODE_PERFORMANCE_ENTRY_TYPE_##name);
  NODE_PERFORMANCE_ENTRY_TYPES(V)
#undef V

#define V(name, _)                                                            \
  NODE_DEFINE_HIDDEN_CONSTANT(constants, NODE_PERFORMANCE_MILESTONE_##name);
  NODE_PERFORMANCE_MILESTONES(V)
#undef V

  v8::PropertyAttribute attr =
      static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete);

  target->DefineOwnProperty(context,
                            FIXED_ONE_BYTE_STRING(isolate, "timeOrigin"),
                            v8::Number::New(isolate, timeOrigin / 1e6),
                            attr).ToChecked();

  target->DefineOwnProperty(context,
                            env->constants_string(),
                            constants,
                            attr).ToChecked();

  SetupGarbageCollectionTracking(isolate);
}

}  // namespace performance
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(performance, node::performance::Init)

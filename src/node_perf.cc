#include "node_internals.h"
#include "node_perf.h"

#include <vector>

namespace node {
namespace performance {

using v8::Array;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Name;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;

const uint64_t timeOrigin = PERFORMANCE_NOW();
uint64_t performance_node_start;
uint64_t performance_v8_start;

uint64_t performance_last_gc_start_mark_ = 0;
v8::GCType performance_last_gc_type_ = v8::GCType::kGCTypeAll;

// Initialize the performance entry object properties
inline void InitObject(const PerformanceEntry& entry, Local<Object> obj) {
  Environment* env = entry.env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  v8::PropertyAttribute attr =
      static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
  obj->DefineOwnProperty(context,
                         env->name_string(),
                         String::NewFromUtf8(isolate,
                                             entry.name().c_str(),
                                             String::kNormalString),
                         attr).FromJust();
  obj->DefineOwnProperty(context,
                         FIXED_ONE_BYTE_STRING(isolate, "entryType"),
                         String::NewFromUtf8(isolate,
                                             entry.type().c_str(),
                                             String::kNormalString),
                         attr).FromJust();
  obj->DefineOwnProperty(context,
                         FIXED_ONE_BYTE_STRING(isolate, "startTime"),
                         Number::New(isolate, entry.startTime()),
                         attr).FromJust();
  obj->DefineOwnProperty(context,
                         FIXED_ONE_BYTE_STRING(isolate, "duration"),
                         Number::New(isolate, entry.duration()),
                         attr).FromJust();
}

// Create a new PerformanceEntry object
const Local<Object> PerformanceEntry::ToObject() const {
  Local<Object> obj =
      env_->performance_entry_template()
          ->NewInstance(env_->context()).ToLocalChecked();
  InitObject(*this, obj);
  return obj;
}

// Allow creating a PerformanceEntry object from JavaScript
void PerformanceEntry::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Utf8Value name(isolate, args[0]);
  Utf8Value type(isolate, args[1]);
  uint64_t now = PERFORMANCE_NOW();
  PerformanceEntry entry(env, *name, *type, now, now);
  Local<Object> obj = args.This();
  InitObject(entry, obj);
  PerformanceEntry::Notify(env, entry.kind(), obj);
}

// Pass the PerformanceEntry object to the PerformanceObservers
void PerformanceEntry::Notify(Environment* env,
                              PerformanceEntryType type,
                              Local<Value> object) {
  Context::Scope scope(env->context());
  AliasedBuffer<uint32_t, v8::Uint32Array>& observers =
      env->performance_state()->observers;
  if (type != NODE_PERFORMANCE_ENTRY_TYPE_INVALID &&
      observers[type]) {
    node::MakeCallback(env->isolate(),
                       env->process_object(),
                       env->performance_entry_callback(),
                       1, &object);
  }
}

// Create a User Timing Mark
void Mark(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HandleScope scope(env->isolate());
  Utf8Value name(env->isolate(), args[0]);
  uint64_t now = PERFORMANCE_NOW();
  auto marks = env->performance_marks();
  (*marks)[*name] = now;

  // TODO(jasnell): Once Tracing API is fully implemented, this should
  // record a trace event also.

  PerformanceEntry entry(env, *name, "mark", now, now);
  Local<Object> obj = entry.ToObject();
  PerformanceEntry::Notify(env, entry.kind(), obj);
  args.GetReturnValue().Set(obj);
}


inline uint64_t GetPerformanceMark(Environment* env, std::string name) {
  auto marks = env->performance_marks();
  auto res = marks->find(name);
  return res != marks->end() ? res->second : 0;
}

// Create a User Timing Measure. A Measure is a PerformanceEntry that
// measures the duration between two distinct user timing marks
void Measure(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HandleScope scope(env->isolate());
  Utf8Value name(env->isolate(), args[0]);
  Utf8Value startMark(env->isolate(), args[1]);
  Utf8Value endMark(env->isolate(), args[2]);

  AliasedBuffer<double, v8::Float64Array>& milestones =
      env->performance_state()->milestones;

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

  PerformanceEntry entry(env, *name, "measure", startTimestamp, endTimestamp);
  Local<Object> obj = entry.ToObject();
  PerformanceEntry::Notify(env, entry.kind(), obj);
  args.GetReturnValue().Set(obj);
}

// Allows specific Node.js lifecycle milestones to be set from JavaScript
void MarkMilestone(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  AliasedBuffer<double, v8::Float64Array>& milestones =
      env->performance_state()->milestones;
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

// Creates a GC Performance Entry and passes it to observers
void PerformanceGCCallback(Environment* env, void* ptr) {
  GCPerformanceEntry* entry = static_cast<GCPerformanceEntry*>(ptr);
  HandleScope scope(env->isolate());
  Local<Context> context = env->context();

  AliasedBuffer<uint32_t, v8::Uint32Array>& observers =
      env->performance_state()->observers;
  if (observers[NODE_PERFORMANCE_ENTRY_TYPE_GC]) {
    Local<Object> obj = entry->ToObject();
    v8::PropertyAttribute attr =
        static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete);
    obj->DefineOwnProperty(context,
                           FIXED_ONE_BYTE_STRING(env->isolate(), "kind"),
                           Integer::New(env->isolate(), entry->gckind()),
                           attr).FromJust();
    PerformanceEntry::Notify(env, entry->kind(), obj);
  }

  delete entry;
}

// Marks the start of a GC cycle
void MarkGarbageCollectionStart(Isolate* isolate,
                                v8::GCType type,
                                v8::GCCallbackFlags flags) {
  performance_last_gc_start_mark_ = PERFORMANCE_NOW();
  performance_last_gc_type_ = type;
}

// Marks the end of a GC cycle
void MarkGarbageCollectionEnd(Isolate* isolate,
                              v8::GCType type,
                              v8::GCCallbackFlags flags,
                              void* data) {
  Environment* env = static_cast<Environment*>(data);
  GCPerformanceEntry* entry =
      new GCPerformanceEntry(env,
                             static_cast<PerformanceGCKind>(type),
                             performance_last_gc_start_mark_,
                             PERFORMANCE_NOW());
  env->SetUnrefImmediate(PerformanceGCCallback,
                         entry);
}


inline void SetupGarbageCollectionTracking(Environment* env) {
  env->isolate()->AddGCPrologueCallback(MarkGarbageCollectionStart);
  env->isolate()->AddGCEpilogueCallback(MarkGarbageCollectionEnd,
                                        static_cast<void*>(env));
}

// Gets the name of a function
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

// Executes a wrapped Function and captures timing information, causing a
// Function PerformanceEntry to be emitted to PerformanceObservers after
// execution.
void TimerFunctionCall(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  Environment* env = Environment::GetCurrent(isolate);
  Local<Context> context = env->context();
  Local<Function> fn = args.Data().As<Function>();
  size_t count = args.Length();
  size_t idx;
  std::vector<Local<Value>> call_args;
  for (size_t i = 0; i < count; ++i)
    call_args.push_back(args[i]);

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

  AliasedBuffer<uint32_t, v8::Uint32Array>& observers =
      env->performance_state()->observers;
  if (!observers[NODE_PERFORMANCE_ENTRY_TYPE_FUNCTION])
    return;

  PerformanceEntry entry(env, *name, "function", start, end);
  Local<Object> obj = entry.ToObject();
  for (idx = 0; idx < count; idx++)
    obj->Set(context, idx, args[idx]).FromJust();
  PerformanceEntry::Notify(env, entry.kind(), obj);
}

// Wraps a Function in a TimerFunctionCall
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

  target->Set(context,
              FIXED_ONE_BYTE_STRING(isolate, "observerCounts"),
              state->observers.GetJSArray()).FromJust();
  target->Set(context,
              FIXED_ONE_BYTE_STRING(isolate, "milestones"),
              state->milestones.GetJSArray()).FromJust();

  Local<String> performanceEntryString =
      FIXED_ONE_BYTE_STRING(isolate, "PerformanceEntry");

  Local<FunctionTemplate> pe = FunctionTemplate::New(isolate);
  pe->SetClassName(performanceEntryString);
  Local<Function> fn = pe->GetFunction();
  target->Set(context, performanceEntryString, fn).FromJust();
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

  SetupGarbageCollectionTracking(env);
}

}  // namespace performance
}  // namespace node

NODE_BUILTIN_MODULE_CONTEXT_AWARE(performance, node::performance::Init)

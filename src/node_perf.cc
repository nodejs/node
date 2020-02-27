#include "aliased_buffer.h"
#include "memory_tracker-inl.h"
#include "node_internals.h"
#include "node_perf.h"
#include "node_buffer.h"
#include "node_process.h"
#include "util-inl.h"

#include <cinttypes>

namespace node {
namespace performance {

using v8::Array;
using v8::Context;
using v8::DontDelete;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::GCCallbackFlags;
using v8::GCType;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Map;
using v8::MaybeLocal;
using v8::Name;
using v8::NewStringType;
using v8::Number;
using v8::Object;
using v8::PropertyAttribute;
using v8::ReadOnly;
using v8::String;
using v8::Uint32Array;
using v8::Value;

// Microseconds in a millisecond, as a float.
#define MICROS_PER_MILLIS 1e3

// https://w3c.github.io/hr-time/#dfn-time-origin
const uint64_t timeOrigin = PERFORMANCE_NOW();
// https://w3c.github.io/hr-time/#dfn-time-origin-timestamp
const double timeOriginTimestamp = GetCurrentTimeInMicroseconds();
uint64_t performance_v8_start;

void performance_state::Mark(enum PerformanceMilestone milestone,
                             uint64_t ts) {
  this->milestones[milestone] = ts;
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP0(
      TRACING_CATEGORY_NODE1(bootstrap),
      GetPerformanceMilestoneName(milestone),
      TRACE_EVENT_SCOPE_THREAD, ts / 1000);
}

// Initialize the performance entry object properties
inline void InitObject(const PerformanceEntry& entry, Local<Object> obj) {
  Environment* env = entry.env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  PropertyAttribute attr =
      static_cast<PropertyAttribute>(ReadOnly | DontDelete);
  obj->DefineOwnProperty(context,
                         env->name_string(),
                         String::NewFromUtf8(isolate,
                                             entry.name().c_str(),
                                             NewStringType::kNormal)
                             .ToLocalChecked(),
                         attr)
      .Check();
  obj->DefineOwnProperty(context,
                         env->entry_type_string(),
                         String::NewFromUtf8(isolate,
                                             entry.type().c_str(),
                                             NewStringType::kNormal)
                             .ToLocalChecked(),
                         attr)
      .Check();
  obj->DefineOwnProperty(context,
                         env->start_time_string(),
                         Number::New(isolate, entry.startTime()),
                         attr).Check();
  obj->DefineOwnProperty(context,
                         env->duration_string(),
                         Number::New(isolate, entry.duration()),
                         attr).Check();
}

// Create a new PerformanceEntry object
MaybeLocal<Object> PerformanceEntry::ToObject() const {
  Local<Object> obj;
  if (!env_->performance_entry_template()
           ->NewInstance(env_->context())
           .ToLocal(&obj)) {
    return MaybeLocal<Object>();
  }
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
  AliasedUint32Array& observers = env->performance_state()->observers;
  if (type != NODE_PERFORMANCE_ENTRY_TYPE_INVALID &&
      observers[type]) {
    node::MakeCallback(env->isolate(),
                       object.As<Object>(),
                       env->performance_entry_callback(),
                       1, &object,
                       node::async_context{0, 0});
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

  TRACE_EVENT_COPY_MARK_WITH_TIMESTAMP(
      TRACING_CATEGORY_NODE2(perf, usertiming),
      *name, now / 1000);

  PerformanceEntry entry(env, *name, "mark", now, now);
  Local<Object> obj;
  if (!entry.ToObject().ToLocal(&obj)) return;
  PerformanceEntry::Notify(env, entry.kind(), obj);
  args.GetReturnValue().Set(obj);
}

void ClearMark(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto marks = env->performance_marks();

  if (args.Length() == 0) {
    marks->clear();
  } else {
    Utf8Value name(env->isolate(), args[0]);
    marks->erase(*name);
  }
}

inline uint64_t GetPerformanceMark(Environment* env, const std::string& name) {
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

  AliasedFloat64Array& milestones = env->performance_state()->milestones;

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

  TRACE_EVENT_COPY_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      TRACING_CATEGORY_NODE2(perf, usertiming),
      *name, *name, startTimestamp / 1000);
  TRACE_EVENT_COPY_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      TRACING_CATEGORY_NODE2(perf, usertiming),
      *name, *name, endTimestamp / 1000);

  PerformanceEntry entry(env, *name, "measure", startTimestamp, endTimestamp);
  Local<Object> obj;
  if (!entry.ToObject().ToLocal(&obj)) return;
  PerformanceEntry::Notify(env, entry.kind(), obj);
  args.GetReturnValue().Set(obj);
}

// Allows specific Node.js lifecycle milestones to be set from JavaScript
void MarkMilestone(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  PerformanceMilestone milestone =
      static_cast<PerformanceMilestone>(
          args[0]->Int32Value(context).ToChecked());
  if (milestone != NODE_PERFORMANCE_MILESTONE_INVALID)
    env->performance_state()->Mark(milestone);
}


void SetupPerformanceObservers(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsFunction());
  env->set_performance_entry_callback(args[0].As<Function>());
}

// Creates a GC Performance Entry and passes it to observers
void PerformanceGCCallback(Environment* env,
                           std::unique_ptr<GCPerformanceEntry> entry) {
  HandleScope scope(env->isolate());
  Local<Context> context = env->context();

  AliasedUint32Array& observers = env->performance_state()->observers;
  if (observers[NODE_PERFORMANCE_ENTRY_TYPE_GC]) {
    Local<Object> obj;
    if (!entry->ToObject().ToLocal(&obj)) return;
    PropertyAttribute attr =
        static_cast<PropertyAttribute>(ReadOnly | DontDelete);
    obj->DefineOwnProperty(context,
                           env->kind_string(),
                           Integer::New(env->isolate(), entry->gckind()),
                           attr).Check();
    obj->DefineOwnProperty(context,
                           env->flags_string(),
                           Integer::New(env->isolate(), entry->gcflags()),
                           attr).Check();
    PerformanceEntry::Notify(env, entry->kind(), obj);
  }
}

// Marks the start of a GC cycle
void MarkGarbageCollectionStart(Isolate* isolate,
                                GCType type,
                                GCCallbackFlags flags,
                                void* data) {
  Environment* env = static_cast<Environment*>(data);
  env->performance_state()->performance_last_gc_start_mark = PERFORMANCE_NOW();
}

// Marks the end of a GC cycle
void MarkGarbageCollectionEnd(Isolate* isolate,
                              GCType type,
                              GCCallbackFlags flags,
                              void* data) {
  Environment* env = static_cast<Environment*>(data);
  performance_state* state = env->performance_state();
  // If no one is listening to gc performance entries, do not create them.
  if (!state->observers[NODE_PERFORMANCE_ENTRY_TYPE_GC])
    return;
  auto entry = std::make_unique<GCPerformanceEntry>(
      env,
      static_cast<PerformanceGCKind>(type),
      static_cast<PerformanceGCFlags>(flags),
      state->performance_last_gc_start_mark,
      PERFORMANCE_NOW());
  env->SetUnrefImmediate([entry = std::move(entry)](Environment* env) mutable {
    PerformanceGCCallback(env, std::move(entry));
  });
}

void GarbageCollectionCleanupHook(void* data) {
  Environment* env = static_cast<Environment*>(data);
  env->isolate()->RemoveGCPrologueCallback(MarkGarbageCollectionStart, data);
  env->isolate()->RemoveGCEpilogueCallback(MarkGarbageCollectionEnd, data);
}

static void InstallGarbageCollectionTracking(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  env->isolate()->AddGCPrologueCallback(MarkGarbageCollectionStart,
                                        static_cast<void*>(env));
  env->isolate()->AddGCEpilogueCallback(MarkGarbageCollectionEnd,
                                        static_cast<void*>(env));
  env->AddCleanupHook(GarbageCollectionCleanupHook, env);
}

static void RemoveGarbageCollectionTracking(
  const FunctionCallbackInfo<Value> &args) {
  Environment* env = Environment::GetCurrent(args);

  env->RemoveCleanupHook(GarbageCollectionCleanupHook, env);
  GarbageCollectionCleanupHook(env);
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
  Local<Context> context = isolate->GetCurrentContext();
  Environment* env = Environment::GetCurrent(context);
  CHECK_NOT_NULL(env);
  Local<Function> fn = args.Data().As<Function>();
  size_t count = args.Length();
  size_t idx;
  SlicedArguments call_args(args);
  Utf8Value name(isolate, GetName(fn));
  bool is_construct_call = args.IsConstructCall();

  uint64_t start = PERFORMANCE_NOW();
  TRACE_EVENT_COPY_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      TRACING_CATEGORY_NODE2(perf, timerify),
      *name, *name, start / 1000);
  v8::MaybeLocal<Value> ret;

  if (is_construct_call) {
    ret = fn->NewInstance(context, call_args.length(), call_args.out())
        .FromMaybe(Local<Object>());
  } else {
    ret = fn->Call(context, args.This(), call_args.length(), call_args.out());
  }

  uint64_t end = PERFORMANCE_NOW();
  TRACE_EVENT_COPY_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      TRACING_CATEGORY_NODE2(perf, timerify),
      *name, *name, end / 1000);

  if (ret.IsEmpty())
    return;
  args.GetReturnValue().Set(ret.ToLocalChecked());

  AliasedUint32Array& observers = env->performance_state()->observers;
  if (!observers[NODE_PERFORMANCE_ENTRY_TYPE_FUNCTION])
    return;

  PerformanceEntry entry(env, *name, "function", start, end);
  Local<Object> obj;
  if (!entry.ToObject().ToLocal(&obj)) return;
  for (idx = 0; idx < count; idx++)
    obj->Set(context, idx, args[idx]).Check();
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

// Notify a custom PerformanceEntry to observers
void Notify(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Utf8Value type(env->isolate(), args[0]);
  Local<Value> entry = args[1];
  PerformanceEntryType entry_type = ToPerformanceEntryTypeEnum(*type);
  AliasedUint32Array& observers = env->performance_state()->observers;
  if (entry_type != NODE_PERFORMANCE_ENTRY_TYPE_INVALID &&
      observers[entry_type]) {
    USE(env->performance_entry_callback()->
      Call(env->context(), Undefined(env->isolate()), 1, &entry));
  }
}


template <class T>
class EventLoopHistogramBinding {
 public:
  static void Declare(
      Environment* env,
      Local<Context> context,
      Local<Object> target,
      Local<String> classname) {
    Local<FunctionTemplate> t = env->NewFunctionTemplate(New);
    t->SetClassName(classname);
    t->InstanceTemplate()->SetInternalFieldCount(1);
    env->SetProtoMethod(t, "exceeds", Exceeds);
    env->SetProtoMethod(t, "min", Min);
    env->SetProtoMethod(t, "max", Max);
    env->SetProtoMethod(t, "mean", Mean);
    env->SetProtoMethod(t, "stddev", Stddev);
    env->SetProtoMethod(t, "percentile", Percentile);
    env->SetProtoMethod(t, "percentiles", Percentiles);
    env->SetProtoMethod(t, "enable", Enable);
    env->SetProtoMethod(t, "disable", Disable);
    env->SetProtoMethod(t, "reset", Reset);
    target->Set(context, classname,
                t->GetFunction(env->context()).ToLocalChecked()).Check();
  }

 protected:
  static void Min(const FunctionCallbackInfo<Value>& args) {
    T* histogram;
    ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
    double value = static_cast<double>(histogram->Min());
    args.GetReturnValue().Set(value);
  }

  static void Max(const FunctionCallbackInfo<Value>& args) {
    T* histogram;
    ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
    double value = static_cast<double>(histogram->Max());
    args.GetReturnValue().Set(value);
  }

  static void Mean(const FunctionCallbackInfo<Value>& args) {
    T* histogram;
    ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
    args.GetReturnValue().Set(histogram->Mean());
  }

  static void Exceeds(const FunctionCallbackInfo<Value>& args) {
    T* histogram;
    ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
    double value = static_cast<double>(histogram->Exceeds());
    args.GetReturnValue().Set(value);
  }

  static void Stddev(const FunctionCallbackInfo<Value>& args) {
    T* histogram;
    ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
    args.GetReturnValue().Set(histogram->Stddev());
  }

  static void Percentile(const FunctionCallbackInfo<Value>& args) {
    T* histogram;
    ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
    CHECK(args[0]->IsNumber());
    double percentile = args[0].As<Number>()->Value();
    args.GetReturnValue().Set(histogram->Percentile(percentile));
  }

  static void Percentiles(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);
    T* histogram;
    ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
    CHECK(args[0]->IsMap());
    Local<Map> map = args[0].As<Map>();
    histogram->Percentiles([&](double key, double value) {
      map->Set(env->context(),
               Number::New(env->isolate(), key),
               Number::New(env->isolate(), value)).IsEmpty();
    });
  }

  static void Enable(const FunctionCallbackInfo<Value>& args) {
    T* histogram;
    ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
    args.GetReturnValue().Set(histogram->Enable());
  }

  static void Disable(const FunctionCallbackInfo<Value>& args) {
    T* histogram;
    ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
    args.GetReturnValue().Set(histogram->Disable());
  }

  static void Reset(const FunctionCallbackInfo<Value>& args) {
    T* histogram;
    ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
    histogram->ResetState();
  }

  static void New(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);
    CHECK(args.IsConstructCall());
    int32_t resolution = args[0]->IntegerValue(env->context()).FromJust();
    CHECK_GT(resolution, 0);
    new T(env, args.This(), resolution);
  }
};

EventLoopHistogram::EventLoopHistogram(int32_t resolution)
  : Histogram(1, 3.6e12),
    resolution_(resolution) {
}

bool EventLoopHistogram::Enable() {
  if (enabled_)
    return false;
  enabled_ = true;
  prev_ = 0;
  return true;
}

bool EventLoopHistogram::Disable() {
  if (!enabled_)
    return false;
  enabled_ = false;
  return true;
}

ELDHistogram::ELDHistogram(
    Environment* env,
    Local<Object> wrap,
    int32_t resolution)
  : HandleWrap(
      env,
      wrap,
      reinterpret_cast<uv_handle_t*>(&timer_),
      AsyncWrap::PROVIDER_ELDHISTOGRAM),
    EventLoopHistogram(resolution) {
  MakeWeak();
  CHECK_EQ(uv_timer_init(env->event_loop(), &timer_), 0);
}

void ELDHistogram::DelayIntervalCallback(uv_timer_t* req) {
  ELDHistogram* histogram = ContainerOf(&ELDHistogram::timer_, req);
  histogram->RecordDelta();
  TRACE_COUNTER1(TRACING_CATEGORY_NODE2(perf, event_loop),
                 "delay_min", histogram->Min());
  TRACE_COUNTER1(TRACING_CATEGORY_NODE2(perf, event_loop),
                 "delay_max", histogram->Max());
  TRACE_COUNTER1(TRACING_CATEGORY_NODE2(perf, event_loop),
                 "delay_mean", histogram->Mean());
  TRACE_COUNTER1(TRACING_CATEGORY_NODE2(perf, event_loop),
                 "delay_stddev", histogram->Stddev());
}

bool ELDHistogram::RecordDelta() {
  uint64_t time = uv_hrtime();
  bool ret = true;
  if (prev_ > 0) {
    int64_t delta = time - prev_;
    if (delta > 0) {
      ret = Record(delta);
      TRACE_COUNTER1(TRACING_CATEGORY_NODE2(perf, event_loop),
                     "delay", delta);
      if (!ret) {
        if (exceeds_ < 0xFFFFFFFF)
          exceeds_++;
        ProcessEmitWarning(
            env(),
            "Event loop delay exceeded 1 hour: %" PRId64 " nanoseconds",
            delta);
      }
    }
  }
  prev_ = time;
  return ret;
}

bool ELDHistogram::Enable() {
  if (IsHandleClosing()) {
    return false;
  }
  if (!EventLoopHistogram::Enable()) {
    return false;
  }
  CHECK_EQ(uv_timer_start(
        &timer_,
        DelayIntervalCallback,
        resolution_,
        resolution_),
      0);
  uv_unref(reinterpret_cast<uv_handle_t*>(&timer_));
  return true;
}

bool ELDHistogram::Disable() {
  if (IsHandleClosing()) {
    return false;
  }
  if (!EventLoopHistogram::Disable()) {
    return false;
  }
  CHECK_EQ(uv_timer_stop(&timer_), 0);
  return true;
}

ELIHistogram::ELIHistogram(
    Environment* env,
    Local<Object> wrap,
    int32_t resolution)
  : HandleWrap(
      env,
      wrap,
      reinterpret_cast<uv_handle_t*>(&prepare_),
      AsyncWrap::PROVIDER_ELIHISTOGRAM),
    EventLoopHistogram(resolution) {
  MakeWeak();
  CHECK_EQ(uv_prepare_init(env->event_loop(), &prepare_), 0);
  CHECK_EQ(uv_check_init(env->event_loop(), &check_), 0);
}

void ELIHistogram::Close(Local<Value> close_callback) {
  // HandleWrap::Close will call `uv_close()` on `prepare_` and both
  // structures will be freed in `HandleWrap::OnClose`.
  // Thus it is safe to call `uv_close` with `nullptr` here.
  if (!IsHandleClosing())
    uv_close(reinterpret_cast<uv_handle_t*>(&check_), nullptr);

  HandleWrap::Close(close_callback);
}

void ELIHistogram::PrepareCallback(uv_prepare_t* handle) {
  ELIHistogram* histogram = ContainerOf(&ELIHistogram::prepare_, handle);
  histogram->RecordPollStart();
}

void ELIHistogram::CheckCallback(uv_check_t* handle) {
  ELIHistogram* histogram = ContainerOf(&ELIHistogram::check_, handle);
  histogram->RecordPollEnd();
  TRACE_COUNTER1(TRACING_CATEGORY_NODE2(perf, event_loop),
                 "idle_min", histogram->Min());
  TRACE_COUNTER1(TRACING_CATEGORY_NODE2(perf, event_loop),
                 "idle_max", histogram->Max());
  TRACE_COUNTER1(TRACING_CATEGORY_NODE2(perf, event_loop),
                 "idle_mean", histogram->Mean());
  TRACE_COUNTER1(TRACING_CATEGORY_NODE2(perf, event_loop),
                 "idle_stddev", histogram->Stddev());
}

void ELIHistogram::RecordPollStart() {
  prev_ = uv_hrtime();
}

void ELIHistogram::RecordPollEnd() {
  if (prev_ == 0) {
    return;
  }

  int64_t delta = uv_hrtime() - prev_;
  if (delta <= 0) {
    return;
  }
  bool ret = Record(delta);
  TRACE_COUNTER1(TRACING_CATEGORY_NODE2(perf, event_loop),
                 "idle", delta);
  if (ret) {
    return;
  }
  if (exceeds_ < 0xFFFFFFFF) {
    exceeds_++;
  }
  ProcessEmitWarning(
      env(),
      "Event loop idleness exceeded 1 hour: %" PRId64 " nanoseconds",
      delta);
}

bool ELIHistogram::Enable() {
  if (!EventLoopHistogram::Enable()) {
    return false;
  }
  CHECK_EQ(uv_prepare_start(&prepare_, PrepareCallback), 0);
  CHECK_EQ(uv_check_start(&check_, CheckCallback), 0);

  uv_unref(reinterpret_cast<uv_handle_t*>(&prepare_));
  uv_unref(reinterpret_cast<uv_handle_t*>(&check_));
  return true;
}

bool ELIHistogram::Disable() {
  if (!EventLoopHistogram::Disable()) {
    return false;
  }
  CHECK_EQ(uv_prepare_stop(&prepare_), 0);
  CHECK_EQ(uv_check_stop(&check_), 0);
  return true;
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();
  performance_state* state = env->performance_state();

  target->Set(context,
              FIXED_ONE_BYTE_STRING(isolate, "observerCounts"),
              state->observers.GetJSArray()).Check();
  target->Set(context,
              FIXED_ONE_BYTE_STRING(isolate, "milestones"),
              state->milestones.GetJSArray()).Check();

  Local<String> performanceEntryString =
      FIXED_ONE_BYTE_STRING(isolate, "PerformanceEntry");

  Local<FunctionTemplate> pe = FunctionTemplate::New(isolate);
  pe->SetClassName(performanceEntryString);
  Local<Function> fn = pe->GetFunction(context).ToLocalChecked();
  target->Set(context, performanceEntryString, fn).Check();
  env->set_performance_entry_template(fn);

  env->SetMethod(target, "clearMark", ClearMark);
  env->SetMethod(target, "mark", Mark);
  env->SetMethod(target, "measure", Measure);
  env->SetMethod(target, "markMilestone", MarkMilestone);
  env->SetMethod(target, "setupObservers", SetupPerformanceObservers);
  env->SetMethod(target, "timerify", Timerify);
  env->SetMethod(target,
                 "installGarbageCollectionTracking",
                 InstallGarbageCollectionTracking);
  env->SetMethod(target,
                 "removeGarbageCollectionTracking",
                 RemoveGarbageCollectionTracking);
  env->SetMethod(target, "notify", Notify);

  Local<Object> constants = Object::New(isolate);

  NODE_DEFINE_CONSTANT(constants, NODE_PERFORMANCE_GC_MAJOR);
  NODE_DEFINE_CONSTANT(constants, NODE_PERFORMANCE_GC_MINOR);
  NODE_DEFINE_CONSTANT(constants, NODE_PERFORMANCE_GC_INCREMENTAL);
  NODE_DEFINE_CONSTANT(constants, NODE_PERFORMANCE_GC_WEAKCB);

  NODE_DEFINE_CONSTANT(
    constants, NODE_PERFORMANCE_GC_FLAGS_NO);
  NODE_DEFINE_CONSTANT(
    constants, NODE_PERFORMANCE_GC_FLAGS_CONSTRUCT_RETAINED);
  NODE_DEFINE_CONSTANT(
    constants, NODE_PERFORMANCE_GC_FLAGS_FORCED);
  NODE_DEFINE_CONSTANT(
    constants, NODE_PERFORMANCE_GC_FLAGS_SYNCHRONOUS_PHANTOM_PROCESSING);
  NODE_DEFINE_CONSTANT(
    constants, NODE_PERFORMANCE_GC_FLAGS_ALL_AVAILABLE_GARBAGE);
  NODE_DEFINE_CONSTANT(
    constants, NODE_PERFORMANCE_GC_FLAGS_ALL_EXTERNAL_MEMORY);
  NODE_DEFINE_CONSTANT(
    constants, NODE_PERFORMANCE_GC_FLAGS_SCHEDULE_IDLE);

#define V(name, _)                                                            \
  NODE_DEFINE_HIDDEN_CONSTANT(constants, NODE_PERFORMANCE_ENTRY_TYPE_##name);
  NODE_PERFORMANCE_ENTRY_TYPES(V)
#undef V

#define V(name, _)                                                            \
  NODE_DEFINE_HIDDEN_CONSTANT(constants, NODE_PERFORMANCE_MILESTONE_##name);
  NODE_PERFORMANCE_MILESTONES(V)
#undef V

  PropertyAttribute attr =
      static_cast<PropertyAttribute>(ReadOnly | DontDelete);

  target->DefineOwnProperty(context,
                            FIXED_ONE_BYTE_STRING(isolate, "timeOrigin"),
                            Number::New(isolate, timeOrigin / 1e6),
                            attr).ToChecked();

  target->DefineOwnProperty(
      context,
      FIXED_ONE_BYTE_STRING(isolate, "timeOriginTimestamp"),
      Number::New(isolate, timeOriginTimestamp / MICROS_PER_MILLIS),
      attr).ToChecked();

  target->DefineOwnProperty(context,
                            env->constants_string(),
                            constants,
                            attr).ToChecked();

  EventLoopHistogramBinding<ELDHistogram>::Declare(
      env,
      context,
      target,
      FIXED_ONE_BYTE_STRING(env->isolate(), "ELDHistogram"));
  EventLoopHistogramBinding<ELIHistogram>::Declare(
      env,
      context,
      target,
      FIXED_ONE_BYTE_STRING(env->isolate(), "ELIHistogram"));
}

}  // namespace performance
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(performance, node::performance::Initialize)

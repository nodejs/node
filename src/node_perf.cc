#include "aliased_buffer.h"
#include "node_internals.h"
#include "node_perf.h"
#include "node_buffer.h"
#include "node_process.h"
#include "async_wrap-inl.h"

#include <cinttypes>

#ifdef __POSIX__
#include <sys/time.h>  // gettimeofday
#endif

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

// Microseconds in a second, as a float.
#define MICROS_PER_SEC 1e6
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

double GetCurrentTimeInMicroseconds() {
#ifdef _WIN32
// The difference between the Unix Epoch and the Windows Epoch in 100-ns ticks.
#define TICKS_TO_UNIX_EPOCH 116444736000000000LL
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  uint64_t filetime_int = static_cast<uint64_t>(ft.dwHighDateTime) << 32 |
                          ft.dwLowDateTime;
  // FILETIME is measured in terms of 100 ns. Convert that to 1 us (1000 ns).
  return (filetime_int - TICKS_TO_UNIX_EPOCH) / 10.;
#else
  struct timeval tp;
  gettimeofday(&tp, nullptr);
  return MICROS_PER_SEC * tp.tv_sec + tp.tv_usec;
#endif
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
      .FromJust();
  obj->DefineOwnProperty(context,
                         env->entry_type_string(),
                         String::NewFromUtf8(isolate,
                                             entry.type().c_str(),
                                             NewStringType::kNormal)
                             .ToLocalChecked(),
                         attr)
      .FromJust();
  obj->DefineOwnProperty(context,
                         env->start_time_string(),
                         Number::New(isolate, entry.startTime()),
                         attr).FromJust();
  obj->DefineOwnProperty(context,
                         env->duration_string(),
                         Number::New(isolate, entry.duration()),
                         attr).FromJust();
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
  AliasedBuffer<uint32_t, Uint32Array>& observers =
      env->performance_state()->observers;
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
void PerformanceGCCallback(Environment* env, void* ptr) {
  std::unique_ptr<GCPerformanceEntry> entry{
      static_cast<GCPerformanceEntry*>(ptr)};
  HandleScope scope(env->isolate());
  Local<Context> context = env->context();

  AliasedBuffer<uint32_t, Uint32Array>& observers =
      env->performance_state()->observers;
  if (observers[NODE_PERFORMANCE_ENTRY_TYPE_GC]) {
    Local<Object> obj;
    if (!entry->ToObject().ToLocal(&obj)) return;
    PropertyAttribute attr =
        static_cast<PropertyAttribute>(ReadOnly | DontDelete);
    obj->DefineOwnProperty(context,
                           env->kind_string(),
                           Integer::New(env->isolate(), entry->gckind()),
                           attr).FromJust();
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
  GCPerformanceEntry* entry =
      new GCPerformanceEntry(env,
                             static_cast<PerformanceGCKind>(type),
                             state->performance_last_gc_start_mark,
                             PERFORMANCE_NOW());
  env->SetUnrefImmediate(PerformanceGCCallback,
                         entry);
}

static void SetupGarbageCollectionTracking(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  env->isolate()->AddGCPrologueCallback(MarkGarbageCollectionStart,
                                        static_cast<void*>(env));
  env->isolate()->AddGCEpilogueCallback(MarkGarbageCollectionEnd,
                                        static_cast<void*>(env));
  env->AddCleanupHook([](void* data) {
    Environment* env = static_cast<Environment*>(data);
    env->isolate()->RemoveGCPrologueCallback(MarkGarbageCollectionStart, data);
    env->isolate()->RemoveGCEpilogueCallback(MarkGarbageCollectionEnd, data);
  }, env);
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

  AliasedBuffer<uint32_t, Uint32Array>& observers =
      env->performance_state()->observers;
  if (!observers[NODE_PERFORMANCE_ENTRY_TYPE_FUNCTION])
    return;

  PerformanceEntry entry(env, *name, "function", start, end);
  Local<Object> obj;
  if (!entry.ToObject().ToLocal(&obj)) return;
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


namespace {
void HistogramMin(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  Histogram::GetMin(histogram, args);
}

void HistogramMax(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  Histogram::GetMax(histogram, args);
}

void HistogramMean(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  Histogram::GetMean(histogram, args);
}

void HistogramCount(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  Histogram::GetCount(histogram, args);
}

void HistogramExceeds(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  Histogram::GetExceeds(histogram, args);
}

void HistogramStddev(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  Histogram::GetStddev(histogram, args);
}

void HistogramPercentile(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  Histogram::GetPercentile(histogram, args);
}

void HistogramPercentiles(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  Histogram::GetPercentiles(histogram, args);
}

void HistogramEnable(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  args.GetReturnValue().Set(histogram->Enable());
}

void HistogramDisable(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  args.GetReturnValue().Set(histogram->Disable());
}

void HistogramReset(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  histogram->ResetState();
}

void ELDHistogramNew(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args.IsConstructCall());
  int32_t resolution = args[0]->IntegerValue(env->context()).FromJust();
  CHECK_GT(resolution, 0);
  new ELDHistogram(env, args.This(), resolution);
}

void RWLHistogramNew(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args.IsConstructCall());
  RWLHistogram* ret = new RWLHistogram(env, args.This());
  ret->SetTypes(args[0]);
}

void ELDHistogramDelayInterval(uv_timer_t* req) {
  ELDHistogram* histogram =
    reinterpret_cast<ELDHistogram*>(req->data);
  histogram->RecordDelta();
  if (*TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACING_CATEGORY_NODE2(perf, event_loop)) != 0) {
    auto value = tracing::TracedValue::Create();
    histogram->DumpToTracedValue(value.get());
    TRACE_EVENT_INSTANT1(TRACING_CATEGORY_NODE2(perf, event_loop),
                         "EventLoopDelay",
                         TRACE_EVENT_SCOPE_THREAD,
                         "histogram",
                         std::move(value));
  }
}

}  // namespace

HistogramBase::HistogramBase(
    Environment* env,
    Local<Object> wrap,
    int64_t lowest,
    int64_t highest,
    int figures) :
    BaseObject(env, wrap),
    Histogram(lowest, highest, figures) {
  MakeWeak();
}

ELDHistogram::ELDHistogram(
    Environment* env,
    Local<Object> wrap,
    int32_t resolution) : HistogramBase(env, wrap, 1, 3.6e12),
                          resolution_(resolution),
                          timer_(nullptr) {}

bool ELDHistogram::RecordDelta() {
  uint64_t time = uv_hrtime();
  bool ret = true;
  if (prev_ > 0) {
    int64_t delta = time - prev_;
    if (delta > 0) {
      ret = Record(delta);
      TRACE_COUNTER1(TRACING_CATEGORY_NODE2(perf, event_loop),
                     "EventLoopDelay",
                     delta);
      if (!ret) {
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

void ELDHistogram::Cleanup() {
  if (timer_ == nullptr)
    return;
  env()->CloseHandle(timer_, [](uv_timer_t* handle) { delete handle; });
  timer_ = nullptr;
}

bool ELDHistogram::Enable() {
  if (timer_ == nullptr) {
    timer_ = new uv_timer_t();
    timer_->data = static_cast<void*>(this);
    uv_timer_init(env()->event_loop(), timer_);
  } else if (uv_is_active(reinterpret_cast<uv_handle_t*>(timer_))) {
    // Timer is already running
    return false;
  }
  uv_timer_start(timer_,
                 ELDHistogramDelayInterval,
                 resolution_,
                 resolution_);
  uv_unref(reinterpret_cast<uv_handle_t*>(timer_));
  return true;
}

bool ELDHistogram::Disable() {
  if (timer_ == nullptr)
    return false;
  Cleanup();
  return true;
}


RWLHistogram::RWLHistogram(
    Environment* env,
    Local<Object> wrap) : HistogramBase(env, wrap, 1, 3.6e12),
    providers_(0) {}

bool RWLHistogram::Enable() {
  if (!enabled_) {
    enabled_ = true;
    env()->add_req_wrap_listener(this);
    return true;
  }
  return false;
}

bool RWLHistogram::Disable() {
  if (enabled_) {
    enabled_ = false;
    env()->remove_req_wrap_listener(this);
    return true;
  }
  return false;
}

void RWLHistogram::Record(int type, int64_t delta) {
  if (!providers_[type])
    return;
  Histogram::Record(delta);
  if (*TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACING_CATEGORY_NODE2(perf, reqwrap)) != 0) {
    auto value = tracing::TracedValue::Create();
    DumpToTracedValue(value.get());
    TRACE_EVENT_INSTANT1(TRACING_CATEGORY_NODE2(perf, reqwrap),
                        "ReqWrapLatency",
                        TRACE_EVENT_SCOPE_THREAD,
                        "histogram",
                        std::move(value));
  }
}

void RWLHistogram::SetTypes(Local<Value> names) {
  if (names->IsArray()) {
    Local<Array> array = names.As<Array>();
    uint32_t length = array->Length();
    for (uint32_t i = 0; i < length; ++i) {
      Utf8Value name(env()->isolate(),
                     array->Get(env()->context(), i).ToLocalChecked());
#define V(n)                                                                   \
  if (strcmp(*name, #n) == 0) {                                                \
    providers_.set(AsyncWrap::PROVIDER_ ## n);                                 \
  }
      NODE_ASYNC_PROVIDER_TYPES(V)
#undef V
    }
  } else if (names.IsEmpty() || names->IsUndefined()) {
    for (uint32_t i = 0; i < AsyncWrap::PROVIDERS_LENGTH; i++)
      providers_.set(i);
  }
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
              state->observers.GetJSArray()).FromJust();
  target->Set(context,
              FIXED_ONE_BYTE_STRING(isolate, "milestones"),
              state->milestones.GetJSArray()).FromJust();

  Local<String> performanceEntryString =
      FIXED_ONE_BYTE_STRING(isolate, "PerformanceEntry");

  Local<FunctionTemplate> pe = FunctionTemplate::New(isolate);
  pe->SetClassName(performanceEntryString);
  Local<Function> fn = pe->GetFunction(context).ToLocalChecked();
  target->Set(context, performanceEntryString, fn).FromJust();
  env->set_performance_entry_template(fn);

  env->SetMethod(target, "clearMark", ClearMark);
  env->SetMethod(target, "mark", Mark);
  env->SetMethod(target, "measure", Measure);
  env->SetMethod(target, "markMilestone", MarkMilestone);
  env->SetMethod(target, "setupObservers", SetupPerformanceObservers);
  env->SetMethod(target, "timerify", Timerify);
  env->SetMethod(
      target, "setupGarbageCollectionTracking", SetupGarbageCollectionTracking);

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

  Local<String> eldh_classname = FIXED_ONE_BYTE_STRING(isolate, "ELDHistogram");
  Local<FunctionTemplate> eldh =
      env->NewFunctionTemplate(ELDHistogramNew);
  eldh->SetClassName(eldh_classname);
  eldh->InstanceTemplate()->SetInternalFieldCount(1);
#define SET_ELDH_FNS(name, fn)                                                 \
  env->SetProtoMethod(eldh, #name, Histogram##fn);
  HISTOGRAM_FNS(SET_ELDH_FNS)
#undef SET_ELDH_FNS
  target->Set(context, eldh_classname,
              eldh->GetFunction(env->context()).ToLocalChecked()).FromJust();

  Local<String> rwlh_classname = FIXED_ONE_BYTE_STRING(isolate, "RWLHistogram");
  Local<FunctionTemplate> rwlh =
      env->NewFunctionTemplate(RWLHistogramNew);
  rwlh->SetClassName(rwlh_classname);
  rwlh->InstanceTemplate()->SetInternalFieldCount(1);
#define SET_RWL_FNS(name, fn)                                                 \
  env->SetProtoMethod(rwlh, #name, Histogram##fn);
  HISTOGRAM_FNS(SET_RWL_FNS)
#undef SET_RWL_FNS
  target->Set(context, rwlh_classname,
              rwlh->GetFunction(env->context()).ToLocalChecked()).FromJust();
}

}  // namespace performance
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(performance, node::performance::Initialize)

#include "aliased_buffer.h"
#include "node_internals.h"
#include "node_perf.h"
#include "node_buffer.h"
#include "node_process.h"

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
using v8::Map;
using v8::MaybeLocal;
using v8::Name;
using v8::NewStringType;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
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

// Heap Histograms
namespace {
static void HistogramMin(const FunctionCallbackInfo<Value>& args) {
  HistogramWrap* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  double value = static_cast<double>((**histogram)->Min());
  args.GetReturnValue().Set(value);
}

static void HistogramMax(const FunctionCallbackInfo<Value>& args) {
  HistogramWrap* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  double value = static_cast<double>((**histogram)->Max());
  args.GetReturnValue().Set(value);
}

static void HistogramMean(const FunctionCallbackInfo<Value>& args) {
  HistogramWrap* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  args.GetReturnValue().Set((**histogram)->Mean());
}

static void HistogramExceeds(const FunctionCallbackInfo<Value>& args) {
  HistogramWrap* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  double value = static_cast<double>(histogram->Exceeds());
  args.GetReturnValue().Set(value);
}

static void HistogramStddev(const FunctionCallbackInfo<Value>& args) {
  HistogramWrap* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  args.GetReturnValue().Set((**histogram)->Stddev());
}

static void HistogramPercentile(const FunctionCallbackInfo<Value>& args) {
  HistogramWrap* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  CHECK(args[0]->IsNumber());
  double percentile = args[0].As<Number>()->Value();
  args.GetReturnValue().Set((**histogram)->Percentile(percentile));
}

static void HistogramPercentiles(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HistogramWrap* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  CHECK(args[0]->IsMap());
  Local<Map> map = args[0].As<Map>();
  (**histogram)->Percentiles([&](double key, double value) {
    map->Set(env->context(),
             Number::New(env->isolate(), key),
             Number::New(env->isolate(), value)).IsEmpty();
  });
}

static void HeapHistogramsEnable(const FunctionCallbackInfo<Value>& args) {
  HeapHistograms* histograms;
  ASSIGN_OR_RETURN_UNWRAP(&histograms, args.Holder());
  args.GetReturnValue().Set(histograms->Enable());
}

static void HeapHistogramsDisable(const FunctionCallbackInfo<Value>& args) {
  HeapHistograms* histograms;
  ASSIGN_OR_RETURN_UNWRAP(&histograms, args.Holder());
  args.GetReturnValue().Set(histograms->Disable());
}

static void HeapHistogramsReset(const FunctionCallbackInfo<Value>& args) {
  ELDHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  histogram->env()->performance_state()->ResetHeapStatistics();
}

static void HeapHistogramsNew(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args.IsConstructCall());
  int32_t resolution = args[0]->IntegerValue(env->context()).FromJust();
  CHECK_GT(resolution, 0);
  new HeapHistograms(env, args.This(), resolution);

#define SET_HISTOGRAM(name, str)                                               \
  HistogramWrap* name##_wrap =                                                 \
      HistogramWrap::New(                                                      \
          env,                                                                 \
          env->performance_state()->name##_histogram(),                        \
          [env]() { return env->performance_state()->name##_exceeds(); });     \
  CHECK_NE(name##_wrap, nullptr);                                              \
  args.This()->Set(env->context(), str, name##_wrap->object()).FromJust();
  SET_HISTOGRAM(rss, env->rss_string())
  SET_HISTOGRAM(total_heap_size, env->heap_total_string())
  SET_HISTOGRAM(used_heap_size, env->heap_used_string())
  SET_HISTOGRAM(external_memory, env->external_string())
#undef SET_HISTOGRAM
}

void HeapHistogramsInterval(uv_timer_t* req) {
  HeapHistograms* histogram =
    reinterpret_cast<HeapHistograms*>(req->data);
  histogram->env()->performance_state()->RecordHeapStatistics(
      histogram->env()->isolate());
}

} // namespace

HeapHistograms::HeapHistograms(Environment* env,
                               Local<Object> wrap,
                               int32_t resolution) :
      BaseObject(env, wrap),
      resolution_(resolution) {
  MakeWeak();
  timer_ = new uv_timer_t();
  uv_timer_init(env->event_loop(), timer_);
  timer_->data = this;
}

bool HeapHistograms::Enable() {
  if (enabled_) return false;
  enabled_ = true;
  uv_timer_start(timer_,
                 HeapHistogramsInterval,
                 resolution_,
                 resolution_);
  uv_unref(reinterpret_cast<uv_handle_t*>(timer_));
  return true;
}

bool HeapHistograms::Disable() {
  if (!enabled_) return false;
  enabled_ = false;
  uv_timer_stop(timer_);
  return true;
}

void HeapHistograms::CloseTimer() {
  if (timer_ == nullptr)
    return;

  env()->CloseHandle(timer_, [](uv_timer_t* handle) { delete handle; });
  timer_ = nullptr;
}

HeapHistograms::~HeapHistograms() {
  Disable();
  CloseTimer();
}


HistogramWrap* HistogramWrap::New(Environment* env,
                                  Histogram* histogram,
                                  std::function<int64_t()> exceeds_fn) {
  Local<Object> obj;
  if (!env->histogram_constructor_template()
          ->NewInstance(env->context())
          .ToLocal(&obj)) {
    return nullptr;
  }
  return new HistogramWrap(env, obj, histogram, std::move(exceeds_fn));
}


// Event Loop Timing Histogram
namespace {
static void ELDHistogramMin(const FunctionCallbackInfo<Value>& args) {
  ELDHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  double value = static_cast<double>(histogram->Min());
  args.GetReturnValue().Set(value);
}

static void ELDHistogramMax(const FunctionCallbackInfo<Value>& args) {
  ELDHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  double value = static_cast<double>(histogram->Max());
  args.GetReturnValue().Set(value);
}

static void ELDHistogramMean(const FunctionCallbackInfo<Value>& args) {
  ELDHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  args.GetReturnValue().Set(histogram->Mean());
}

static void ELDHistogramExceeds(const FunctionCallbackInfo<Value>& args) {
  ELDHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  double value = static_cast<double>(histogram->Exceeds());
  args.GetReturnValue().Set(value);
}

static void ELDHistogramStddev(const FunctionCallbackInfo<Value>& args) {
  ELDHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  args.GetReturnValue().Set(histogram->Stddev());
}

static void ELDHistogramPercentile(const FunctionCallbackInfo<Value>& args) {
  ELDHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  CHECK(args[0]->IsNumber());
  double percentile = args[0].As<Number>()->Value();
  args.GetReturnValue().Set(histogram->Percentile(percentile));
}

static void ELDHistogramPercentiles(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  ELDHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  CHECK(args[0]->IsMap());
  Local<Map> map = args[0].As<Map>();
  histogram->Percentiles([&](double key, double value) {
    map->Set(env->context(),
             Number::New(env->isolate(), key),
             Number::New(env->isolate(), value)).IsEmpty();
  });
}

static void ELDHistogramEnable(const FunctionCallbackInfo<Value>& args) {
  ELDHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  args.GetReturnValue().Set(histogram->Enable());
}

static void ELDHistogramDisable(const FunctionCallbackInfo<Value>& args) {
  ELDHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  args.GetReturnValue().Set(histogram->Disable());
}

static void ELDHistogramReset(const FunctionCallbackInfo<Value>& args) {
  ELDHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  histogram->ResetState();
}

static void ELDHistogramNew(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args.IsConstructCall());
  int32_t resolution = args[0]->IntegerValue(env->context()).FromJust();
  CHECK_GT(resolution, 0);
  new ELDHistogram(env, args.This(), resolution);
}
}  // namespace

ELDHistogram::ELDHistogram(
    Environment* env,
    Local<Object> wrap,
    int32_t resolution) : BaseObject(env, wrap),
                          Histogram(1, 3.6e12),
                          resolution_(resolution) {
  MakeWeak();
  timer_ = new uv_timer_t();
  uv_timer_init(env->event_loop(), timer_);
  timer_->data = this;
}

void ELDHistogram::CloseTimer() {
  if (timer_ == nullptr)
    return;

  env()->CloseHandle(timer_, [](uv_timer_t* handle) { delete handle; });
  timer_ = nullptr;
}

ELDHistogram::~ELDHistogram() {
  Disable();
  CloseTimer();
}

void ELDHistogramDelayInterval(uv_timer_t* req) {
  ELDHistogram* histogram =
    reinterpret_cast<ELDHistogram*>(req->data);
  histogram->RecordDelta();
  TRACE_COUNTER1(TRACING_CATEGORY_NODE2(perf, event_loop),
                 "min", histogram->Min());
  TRACE_COUNTER1(TRACING_CATEGORY_NODE2(perf, event_loop),
                 "max", histogram->Max());
  TRACE_COUNTER1(TRACING_CATEGORY_NODE2(perf, event_loop),
                 "mean", histogram->Mean());
  TRACE_COUNTER1(TRACING_CATEGORY_NODE2(perf, event_loop),
                 "stddev", histogram->Stddev());
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
  if (enabled_) return false;
  enabled_ = true;
  uv_timer_start(timer_,
                 ELDHistogramDelayInterval,
                 resolution_,
                 resolution_);
  uv_unref(reinterpret_cast<uv_handle_t*>(timer_));
  return true;
}

bool ELDHistogram::Disable() {
  if (!enabled_) return false;
  enabled_ = false;
  uv_timer_stop(timer_);
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
  env->SetProtoMethod(eldh, "exceeds", ELDHistogramExceeds);
  env->SetProtoMethod(eldh, "min", ELDHistogramMin);
  env->SetProtoMethod(eldh, "max", ELDHistogramMax);
  env->SetProtoMethod(eldh, "mean", ELDHistogramMean);
  env->SetProtoMethod(eldh, "stddev", ELDHistogramStddev);
  env->SetProtoMethod(eldh, "percentile", ELDHistogramPercentile);
  env->SetProtoMethod(eldh, "percentiles", ELDHistogramPercentiles);
  env->SetProtoMethod(eldh, "enable", ELDHistogramEnable);
  env->SetProtoMethod(eldh, "disable", ELDHistogramDisable);
  env->SetProtoMethod(eldh, "reset", ELDHistogramReset);
  target->Set(context, eldh_classname,
              eldh->GetFunction(env->context()).ToLocalChecked()).FromJust();

  Local<String> histogram_name =
      FIXED_ONE_BYTE_STRING(isolate, "Histogram");
  Local<FunctionTemplate> histogram =
      FunctionTemplate::New(env->isolate());
  histogram->SetClassName(histogram_name);
  env->SetProtoMethod(histogram, "exceeds", HistogramExceeds);
  env->SetProtoMethod(histogram, "min", HistogramMin);
  env->SetProtoMethod(histogram, "max", HistogramMax);
  env->SetProtoMethod(histogram, "mean", HistogramMean);
  env->SetProtoMethod(histogram, "stddev", HistogramStddev);
  env->SetProtoMethod(histogram, "percentile", HistogramPercentile);
  env->SetProtoMethod(histogram, "percentiles", HistogramPercentiles);
  Local<ObjectTemplate> histogramt = histogram->InstanceTemplate();
  histogramt->SetInternalFieldCount(1);
  env->set_histogram_constructor_template(histogramt);

  Local<String> heap_histograms_name =
      FIXED_ONE_BYTE_STRING(isolate, "HeapHistograms");
  Local<FunctionTemplate> heap_histograms =
      env->NewFunctionTemplate(HeapHistogramsNew);
  heap_histograms->SetClassName(heap_histograms_name);
  env->SetProtoMethod(heap_histograms, "enable", HeapHistogramsEnable);
  env->SetProtoMethod(heap_histograms, "disable", HeapHistogramsDisable);
  env->SetProtoMethod(heap_histograms, "reset", HeapHistogramsReset);
  Local<ObjectTemplate> heap_histogramst = heap_histograms->InstanceTemplate();
  heap_histogramst->Set(env->rss_string(), Null(env->isolate()));
  heap_histogramst->Set(env->heap_total_string(), Null(env->isolate()));
  heap_histogramst->Set(env->heap_used_string(), Null(env->isolate()));
  heap_histogramst->Set(env->external_string(), Null(env->isolate()));
  heap_histogramst->SetInternalFieldCount(1);
  target->Set(context, heap_histograms_name,
              heap_histograms->GetFunction(
                  env->context()).ToLocalChecked()).FromJust();
}

}  // namespace performance
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(performance, node::performance::Initialize)

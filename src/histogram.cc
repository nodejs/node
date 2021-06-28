#include "histogram.h"  // NOLINT(build/include_inline)
#include "base_object-inl.h"
#include "histogram-inl.h"
#include "memory_tracker-inl.h"
#include "node_errors.h"
#include "node_external_reference.h"

namespace node {

using v8::BigInt;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Local;
using v8::Map;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;

Histogram::Histogram(int64_t lowest, int64_t highest, int figures) {
  hdr_histogram* histogram;
  CHECK_EQ(0, hdr_init(lowest, highest, figures, &histogram));
  histogram_.reset(histogram);
}

void Histogram::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("histogram", GetMemorySize());
}

HistogramImpl::HistogramImpl(int64_t lowest, int64_t highest, int figures)
    : histogram_(new Histogram(lowest, highest, figures)) {}

HistogramImpl::HistogramImpl(std::shared_ptr<Histogram> histogram)
    : histogram_(std::move(histogram)) {}

HistogramBase::HistogramBase(
    Environment* env,
    Local<Object> wrap,
    int64_t lowest,
    int64_t highest,
    int figures)
    : BaseObject(env, wrap),
      HistogramImpl(lowest, highest, figures) {
  MakeWeak();
}

HistogramBase::HistogramBase(
    Environment* env,
    Local<Object> wrap,
    std::shared_ptr<Histogram> histogram)
    : BaseObject(env, wrap),
      HistogramImpl(std::move(histogram)) {
  MakeWeak();
}

void HistogramBase::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("histogram", histogram());
}

void HistogramBase::GetMin(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  double value = static_cast<double>((*histogram)->Min());
  args.GetReturnValue().Set(value);
}

void HistogramBase::GetMax(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  double value = static_cast<double>((*histogram)->Max());
  args.GetReturnValue().Set(value);
}

void HistogramBase::GetMean(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  args.GetReturnValue().Set((*histogram)->Mean());
}

void HistogramBase::GetExceeds(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  double value = static_cast<double>((*histogram)->Exceeds());
  args.GetReturnValue().Set(value);
}

void HistogramBase::GetStddev(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  args.GetReturnValue().Set((*histogram)->Stddev());
}

void HistogramBase::GetPercentile(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  CHECK(args[0]->IsNumber());
  double percentile = args[0].As<Number>()->Value();
  args.GetReturnValue().Set((*histogram)->Percentile(percentile));
}

void HistogramBase::GetPercentiles(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  CHECK(args[0]->IsMap());
  Local<Map> map = args[0].As<Map>();
  (*histogram)->Percentiles([map, env](double key, double value) {
    map->Set(
        env->context(),
        Number::New(env->isolate(), key),
        Number::New(env->isolate(), value)).IsEmpty();
  });
}

void HistogramBase::DoReset(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  (*histogram)->Reset();
}

void HistogramBase::RecordDelta(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  (*histogram)->RecordDelta();
}

void HistogramBase::Record(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_IMPLIES(!args[0]->IsNumber(), args[0]->IsBigInt());
  bool lossless = true;
  int64_t value = args[0]->IsBigInt()
      ? args[0].As<BigInt>()->Int64Value(&lossless)
      : static_cast<int64_t>(args[0].As<Number>()->Value());
  if (!lossless || value < 1)
    return THROW_ERR_OUT_OF_RANGE(env, "value is out of range");
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  (*histogram)->Record(value);
}

BaseObjectPtr<HistogramBase> HistogramBase::Create(
    Environment* env,
    int64_t lowest,
    int64_t highest,
    int figures) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env)
          ->InstanceTemplate()
          ->NewInstance(env->context()).ToLocal(&obj)) {
    return BaseObjectPtr<HistogramBase>();
  }

  return MakeBaseObject<HistogramBase>(
      env, obj, lowest, highest, figures);
}

BaseObjectPtr<HistogramBase> HistogramBase::Create(
    Environment* env,
    std::shared_ptr<Histogram> histogram) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env)
          ->InstanceTemplate()
          ->NewInstance(env->context()).ToLocal(&obj)) {
    return BaseObjectPtr<HistogramBase>();
  }
  return MakeBaseObject<HistogramBase>(env, obj, std::move(histogram));
}

void HistogramBase::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  new HistogramBase(env, args.This());
}

Local<FunctionTemplate> HistogramBase::GetConstructorTemplate(
    Environment* env) {
  Local<FunctionTemplate> tmpl = env->histogram_ctor_template();
  if (tmpl.IsEmpty()) {
    tmpl = env->NewFunctionTemplate(New);
    Local<String> classname =
        FIXED_ONE_BYTE_STRING(env->isolate(), "Histogram");
    tmpl->SetClassName(classname);
    tmpl->Inherit(BaseObject::GetConstructorTemplate(env));

    tmpl->InstanceTemplate()->SetInternalFieldCount(
        HistogramBase::kInternalFieldCount);
    env->SetProtoMethodNoSideEffect(tmpl, "exceeds", GetExceeds);
    env->SetProtoMethodNoSideEffect(tmpl, "min", GetMin);
    env->SetProtoMethodNoSideEffect(tmpl, "max", GetMax);
    env->SetProtoMethodNoSideEffect(tmpl, "mean", GetMean);
    env->SetProtoMethodNoSideEffect(tmpl, "stddev", GetStddev);
    env->SetProtoMethodNoSideEffect(tmpl, "percentile", GetPercentile);
    env->SetProtoMethodNoSideEffect(tmpl, "percentiles", GetPercentiles);
    env->SetProtoMethod(tmpl, "reset", DoReset);
    env->SetProtoMethod(tmpl, "record", Record);
    env->SetProtoMethod(tmpl, "recordDelta", RecordDelta);
    env->set_histogram_ctor_template(tmpl);
  }
  return tmpl;
}

void HistogramBase::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(GetExceeds);
  registry->Register(GetMin);
  registry->Register(GetMax);
  registry->Register(GetMean);
  registry->Register(GetStddev);
  registry->Register(GetPercentile);
  registry->Register(GetPercentiles);
  registry->Register(DoReset);
  registry->Register(Record);
  registry->Register(RecordDelta);
}

void HistogramBase::Initialize(Environment* env, Local<Object> target) {
  env->SetConstructorFunction(target, "Histogram", GetConstructorTemplate(env));
}

BaseObjectPtr<BaseObject> HistogramBase::HistogramTransferData::Deserialize(
    Environment* env,
    v8::Local<v8::Context> context,
    std::unique_ptr<worker::TransferData> self) {
  return Create(env, std::move(histogram_));
}

std::unique_ptr<worker::TransferData> HistogramBase::CloneForMessaging() const {
  return std::make_unique<HistogramTransferData>(this);
}

void HistogramBase::HistogramTransferData::MemoryInfo(
    MemoryTracker* tracker) const {
  tracker->TrackField("histogram", histogram_);
}

Local<FunctionTemplate> IntervalHistogram::GetConstructorTemplate(
    Environment* env) {
  Local<FunctionTemplate> tmpl = env->intervalhistogram_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = FunctionTemplate::New(env->isolate());
    tmpl->Inherit(HandleWrap::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        HistogramBase::kInternalFieldCount);
    env->SetProtoMethodNoSideEffect(tmpl, "exceeds", GetExceeds);
    env->SetProtoMethodNoSideEffect(tmpl, "min", GetMin);
    env->SetProtoMethodNoSideEffect(tmpl, "max", GetMax);
    env->SetProtoMethodNoSideEffect(tmpl, "mean", GetMean);
    env->SetProtoMethodNoSideEffect(tmpl, "stddev", GetStddev);
    env->SetProtoMethodNoSideEffect(tmpl, "percentile", GetPercentile);
    env->SetProtoMethodNoSideEffect(tmpl, "percentiles", GetPercentiles);
    env->SetProtoMethod(tmpl, "reset", DoReset);
    env->SetProtoMethod(tmpl, "start", Start);
    env->SetProtoMethod(tmpl, "stop", Stop);
    env->set_intervalhistogram_constructor_template(tmpl);
  }
  return tmpl;
}

void IntervalHistogram::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(GetExceeds);
  registry->Register(GetMin);
  registry->Register(GetMax);
  registry->Register(GetMean);
  registry->Register(GetStddev);
  registry->Register(GetPercentile);
  registry->Register(GetPercentiles);
  registry->Register(DoReset);
  registry->Register(Start);
  registry->Register(Stop);
}

IntervalHistogram::IntervalHistogram(
    Environment* env,
    Local<Object> wrap,
    AsyncWrap::ProviderType type,
    int32_t interval,
    int64_t lowest,
    int64_t highest,
    int figures)
    : HandleWrap(
          env,
          wrap,
          reinterpret_cast<uv_handle_t*>(&timer_),
          type),
      HistogramImpl(lowest, highest, figures),
      interval_(interval) {
  MakeWeak();
  uv_timer_init(env->event_loop(), &timer_);
}

void IntervalHistogram::TimerCB(uv_timer_t* handle) {
  IntervalHistogram* histogram =
      ContainerOf(&IntervalHistogram::timer_, handle);
  histogram->OnInterval();
}

void IntervalHistogram::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("histogram", histogram());
}

void IntervalHistogram::OnStart(StartFlags flags) {
  if (enabled_ || IsHandleClosing()) return;
  enabled_ = true;
  if (flags == StartFlags::RESET)
    histogram()->Reset();
  uv_timer_start(&timer_, TimerCB, interval_, interval_);
  uv_unref(reinterpret_cast<uv_handle_t*>(&timer_));
}

void IntervalHistogram::OnStop() {
  if (!enabled_ || IsHandleClosing()) return;
  enabled_ = false;
  uv_timer_stop(&timer_);
}

void IntervalHistogram::Start(const FunctionCallbackInfo<Value>& args) {
  IntervalHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  histogram->OnStart(args[0]->IsTrue() ? StartFlags::RESET : StartFlags::NONE);
}

void IntervalHistogram::Stop(const FunctionCallbackInfo<Value>& args) {
  IntervalHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  histogram->OnStop();
}

void IntervalHistogram::GetMin(const FunctionCallbackInfo<Value>& args) {
  IntervalHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  double value = static_cast<double>((*histogram)->Min());
  args.GetReturnValue().Set(value);
}

void IntervalHistogram::GetMax(const FunctionCallbackInfo<Value>& args) {
  IntervalHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  double value = static_cast<double>((*histogram)->Max());
  args.GetReturnValue().Set(value);
}

void IntervalHistogram::GetMean(const FunctionCallbackInfo<Value>& args) {
  IntervalHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  args.GetReturnValue().Set((*histogram)->Mean());
}

void IntervalHistogram::GetExceeds(const FunctionCallbackInfo<Value>& args) {
  IntervalHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  double value = static_cast<double>((*histogram)->Exceeds());
  args.GetReturnValue().Set(value);
}

void IntervalHistogram::GetStddev(const FunctionCallbackInfo<Value>& args) {
  IntervalHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  args.GetReturnValue().Set((*histogram)->Stddev());
}

void IntervalHistogram::GetPercentile(const FunctionCallbackInfo<Value>& args) {
  IntervalHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  CHECK(args[0]->IsNumber());
  double percentile = args[0].As<Number>()->Value();
  args.GetReturnValue().Set((*histogram)->Percentile(percentile));
}

void IntervalHistogram::GetPercentiles(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  IntervalHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  CHECK(args[0]->IsMap());
  Local<Map> map = args[0].As<Map>();
  (*histogram)->Percentiles([map, env](double key, double value) {
    map->Set(
        env->context(),
        Number::New(env->isolate(), key),
        Number::New(env->isolate(), value)).IsEmpty();
  });
}

void IntervalHistogram::DoReset(const FunctionCallbackInfo<Value>& args) {
  IntervalHistogram* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  (*histogram)->Reset();
}

std::unique_ptr<worker::TransferData>
IntervalHistogram::CloneForMessaging() const {
  return std::make_unique<HistogramBase::HistogramTransferData>(histogram());
}

}  // namespace node

#include "histogram.h"  // NOLINT(build/include_inline)
#include "histogram-inl.h"
#include "memory_tracker-inl.h"

namespace node {

using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Local;
using v8::Map;
using v8::Number;
using v8::ObjectTemplate;
using v8::String;
using v8::Value;

Histogram::Histogram(int64_t lowest, int64_t highest, int figures) {
  hdr_histogram* histogram;
  CHECK_EQ(0, hdr_init(lowest, highest, figures, &histogram));
  histogram_.reset(histogram);
}

HistogramBase::HistogramBase(
    Environment* env,
    v8::Local<v8::Object> wrap,
    int64_t lowest,
    int64_t highest,
    int figures)
    : BaseObject(env, wrap),
      Histogram(lowest, highest, figures) {
  MakeWeak();
}

void HistogramBase::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("histogram", GetMemorySize());
}

void HistogramBase::GetMin(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  double value = static_cast<double>(histogram->Min());
  args.GetReturnValue().Set(value);
}

void HistogramBase::GetMax(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  double value = static_cast<double>(histogram->Max());
  args.GetReturnValue().Set(value);
}

void HistogramBase::GetMean(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  args.GetReturnValue().Set(histogram->Mean());
}

void HistogramBase::GetExceeds(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  double value = static_cast<double>(histogram->Exceeds());
  args.GetReturnValue().Set(value);
}

void HistogramBase::GetStddev(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  args.GetReturnValue().Set(histogram->Stddev());
}

void HistogramBase::GetPercentile(
    const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  CHECK(args[0]->IsNumber());
  double percentile = args[0].As<Number>()->Value();
  args.GetReturnValue().Set(histogram->Percentile(percentile));
}

void HistogramBase::GetPercentiles(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  CHECK(args[0]->IsMap());
  Local<Map> map = args[0].As<Map>();
  histogram->Percentiles([map, env](double key, double value) {
    map->Set(
        env->context(),
        Number::New(env->isolate(), key),
        Number::New(env->isolate(), value)).IsEmpty();
  });
}

void HistogramBase::DoReset(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  histogram->ResetState();
}

BaseObjectPtr<HistogramBase> HistogramBase::New(
    Environment* env,
    int64_t lowest,
    int64_t highest,
    int figures) {
  CHECK_LE(lowest, highest);
  CHECK_GT(figures, 0);
  v8::Local<v8::Object> obj;
  auto tmpl = env->histogram_instance_template();
  if (!tmpl->NewInstance(env->context()).ToLocal(&obj))
    return {};

  return MakeDetachedBaseObject<HistogramBase>(
      env, obj, lowest, highest, figures);
}

void HistogramBase::Initialize(Environment* env) {
  // Guard against multiple initializations
  if (!env->histogram_instance_template().IsEmpty())
    return;

  Local<FunctionTemplate> histogram = FunctionTemplate::New(env->isolate());
  Local<String> classname = FIXED_ONE_BYTE_STRING(env->isolate(), "Histogram");
  histogram->SetClassName(classname);

  Local<ObjectTemplate> histogramt =
    histogram->InstanceTemplate();

  histogramt->SetInternalFieldCount(1);
  env->SetProtoMethod(histogram, "exceeds", HistogramBase::GetExceeds);
  env->SetProtoMethod(histogram, "min", HistogramBase::GetMin);
  env->SetProtoMethod(histogram, "max", HistogramBase::GetMax);
  env->SetProtoMethod(histogram, "mean", HistogramBase::GetMean);
  env->SetProtoMethod(histogram, "stddev", HistogramBase::GetStddev);
  env->SetProtoMethod(histogram, "percentile", HistogramBase::GetPercentile);
  env->SetProtoMethod(histogram, "percentiles", HistogramBase::GetPercentiles);
  env->SetProtoMethod(histogram, "reset", HistogramBase::DoReset);

  env->set_histogram_instance_template(histogramt);
}

}  // namespace node

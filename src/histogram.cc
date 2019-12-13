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

void HistogramBase::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("histogram", GetMemorySize());
}

void HistogramBase::HistogramMin(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  double value = static_cast<double>(histogram->Min());
  args.GetReturnValue().Set(value);
}

void HistogramBase::HistogramMax(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  double value = static_cast<double>(histogram->Max());
  args.GetReturnValue().Set(value);
}

void HistogramBase::HistogramMean(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  args.GetReturnValue().Set(histogram->Mean());
}

void HistogramBase::HistogramExceeds(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  double value = static_cast<double>(histogram->Exceeds());
  args.GetReturnValue().Set(value);
}

void HistogramBase::HistogramStddev(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  args.GetReturnValue().Set(histogram->Stddev());
}

void HistogramBase::HistogramPercentile(
    const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  CHECK(args[0]->IsNumber());
  double percentile = args[0].As<Number>()->Value();
  args.GetReturnValue().Set(histogram->Percentile(percentile));
}

void HistogramBase::HistogramPercentiles(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  CHECK(args[0]->IsMap());
  Local<Map> map = args[0].As<Map>();
  histogram->Percentiles([&](double key, double value) {
    map->Set(
        env->context(),
        Number::New(env->isolate(), key),
        Number::New(env->isolate(), value)).IsEmpty();
  });
}

void HistogramBase::HistogramReset(const FunctionCallbackInfo<Value>& args) {
  HistogramBase* histogram;
  ASSIGN_OR_RETURN_UNWRAP(&histogram, args.Holder());
  histogram->ResetState();
}

void HistogramBase::Initialize(Environment* env) {
  // Guard against multiple initializations
  if (!env->histogram_ctor_template().IsEmpty())
    return;

  Local<String> classname =
      FIXED_ONE_BYTE_STRING(env->isolate(), "Histogram");

  Local<FunctionTemplate> histogram =
    FunctionTemplate::New(env->isolate());
  histogram->SetClassName(classname);

  Local<ObjectTemplate> histogramt =
    histogram->InstanceTemplate();

  histogramt->SetInternalFieldCount(1);
  env->SetProtoMethod(histogram, "exceeds", HistogramExceeds);
  env->SetProtoMethod(histogram, "min", HistogramMin);
  env->SetProtoMethod(histogram, "max", HistogramMax);
  env->SetProtoMethod(histogram, "mean", HistogramMean);
  env->SetProtoMethod(histogram, "stddev", HistogramStddev);
  env->SetProtoMethod(histogram, "percentile", HistogramPercentile);
  env->SetProtoMethod(histogram, "percentiles", HistogramPercentiles);
  env->SetProtoMethod(histogram, "reset", HistogramReset);

  env->set_histogram_ctor_template(histogramt);
}

}  // namespace node

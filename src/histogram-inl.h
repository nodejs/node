#ifndef SRC_HISTOGRAM_INL_H_
#define SRC_HISTOGRAM_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "histogram.h"
#include "node_internals.h"
#include "env-inl.h"
#include "v8.h"

namespace node {

using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Map;
using v8::Number;
using v8::Value;

inline void Histogram::GetMin(
    Histogram* histogram,
    const FunctionCallbackInfo<Value>& args) {
  double value = static_cast<double>(histogram->Min());
  args.GetReturnValue().Set(value);
}

inline void Histogram::GetMax(
    Histogram* histogram,
    const FunctionCallbackInfo<Value>& args) {
  double value = static_cast<double>(histogram->Max());
  args.GetReturnValue().Set(value);
}

inline void Histogram::GetMean(
    Histogram* histogram,
    const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(histogram->Mean());
}

inline void Histogram::GetExceeds(
    Histogram* histogram,
    const FunctionCallbackInfo<Value>& args) {
  double value = static_cast<double>(histogram->Exceeds());
  args.GetReturnValue().Set(value);
}

inline void Histogram::GetCount(
    Histogram* histogram,
    const FunctionCallbackInfo<Value>& args) {
  double value = static_cast<double>(histogram->Count());
  args.GetReturnValue().Set(value);
}

inline void Histogram::GetStddev(
    Histogram* histogram,
    const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(histogram->Stddev());
}

inline void Histogram::GetPercentile(
    Histogram* histogram,
    const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsNumber());
  double percentile = args[0].As<Number>()->Value();
  args.GetReturnValue().Set(histogram->Percentile(percentile));
}

inline void Histogram::GetPercentiles(
    Histogram* histogram,
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsMap());
  Local<Map> map = args[0].As<Map>();
  histogram->Percentiles([&](double key, double value) {
    map->Set(env->context(),
             Number::New(env->isolate(), key),
             Number::New(env->isolate(), value)).IsEmpty();
  });
}

inline Histogram::Histogram(int64_t lowest, int64_t highest, int figures) :
    exceeds_(0), count_(0) {
  CHECK_EQ(0, hdr_init(lowest, highest, figures, &histogram_));
}

inline Histogram::~Histogram() {
  hdr_close(histogram_);
}

inline void Histogram::Reset() {
  hdr_reset(histogram_);
  exceeds_ = 0;
  count_ = 0;
}

inline bool Histogram::Record(int64_t value) {
  bool ret = hdr_record_value(histogram_, value);
  if (!ret && exceeds_ < 0xFFFFFFFF)
    exceeds_++;
  if (count_ < 0xFFFFFFFF)
    count_++;
  return ret;
}

inline int64_t Histogram::Exceeds() {
  return exceeds_;
}

inline int64_t Histogram::Count() {
  return count_;
}

inline int64_t Histogram::Min() {
  return hdr_min(histogram_);
}

inline int64_t Histogram::Max() {
  return hdr_max(histogram_);
}

inline double Histogram::Mean() {
  return hdr_mean(histogram_);
}

inline double Histogram::Stddev() {
  return hdr_stddev(histogram_);
}

inline double Histogram::Percentile(double percentile) {
  CHECK_GT(percentile, 0);
  CHECK_LE(percentile, 100);
  return hdr_value_at_percentile(histogram_, percentile);
}

inline void Histogram::Percentiles(std::function<void(double, double)> fn) {
  hdr_iter iter;
  hdr_iter_percentile_init(&iter, histogram_, 1);
  while (hdr_iter_next(&iter)) {
    double key = iter.specifics.percentiles.percentile;
    double value = iter.value;
    fn(key, value);
  }
}

inline void Histogram::DumpToTracedValue(tracing::TracedValue* value) {
  value->SetDouble("min", static_cast<double>(Min()));
  value->SetDouble("max", static_cast<double>(Max()));
  value->SetDouble("mean", static_cast<double>(Mean()));
  value->SetDouble("stddev", static_cast<double>(Stddev()));
  value->SetDouble("exceeds", static_cast<double>(Exceeds()));
  value->SetDouble("count", static_cast<double>(Count()));
  value->BeginArray("percentiles");
  Percentiles([&](double key, double val) {
    value->BeginArray();
    value->AppendDouble(key);
    value->AppendDouble(val);
    value->EndArray();
  });
  value->EndArray();
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_HISTOGRAM_INL_H_

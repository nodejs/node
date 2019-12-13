#ifndef SRC_HISTOGRAM_INL_H_
#define SRC_HISTOGRAM_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "histogram.h"
#include "base_object-inl.h"
#include "node_internals.h"

namespace node {

Histogram::Histogram(int64_t lowest, int64_t highest, int figures) {
  CHECK_EQ(0, hdr_init(lowest, highest, figures, &histogram_));
}

Histogram::~Histogram() {
  hdr_close(histogram_);
}

void Histogram::Reset() {
  hdr_reset(histogram_);
}

bool Histogram::Record(int64_t value) {
  return hdr_record_value(histogram_, value);
}

int64_t Histogram::Min() {
  return hdr_min(histogram_);
}

int64_t Histogram::Max() {
  return hdr_max(histogram_);
}

double Histogram::Mean() {
  return hdr_mean(histogram_);
}

double Histogram::Stddev() {
  return hdr_stddev(histogram_);
}

double Histogram::Percentile(double percentile) {
  CHECK_GT(percentile, 0);
  CHECK_LE(percentile, 100);
  return static_cast<double>(hdr_value_at_percentile(histogram_, percentile));
}

template <typename Iterator>
void Histogram::Percentiles(Iterator&& fn) {
  hdr_iter iter;
  hdr_iter_percentile_init(&iter, histogram_, 1);
  while (hdr_iter_next(&iter)) {
    double key = iter.specifics.percentiles.percentile;
    double value = static_cast<double>(iter.value);
    fn(key, value);
  }
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

bool HistogramBase::RecordDelta() {
  uint64_t time = uv_hrtime();
  bool ret = true;
  if (prev_ > 0) {
    int64_t delta = time - prev_;
    if (delta > 0) {
      ret = Record(delta);
      TraceDelta(delta);
      if (!ret) {
        if (exceeds_ < 0xFFFFFFFF)
          exceeds_++;
        TraceExceeds(delta);
      }
    }
  }
  prev_ = time;
  return ret;
}

void HistogramBase::ResetState() {
  Reset();
  exceeds_ = 0;
  prev_ = 0;
}

BaseObjectPtr<HistogramBase> HistogramBase::New(
    Environment* env,
    int64_t lowest,
    int64_t highest,
    int figures) {
  CHECK_LE(lowest, highest);
  CHECK_GT(figures, 0);
  v8::Local<v8::Object> obj;
  auto tmpl = env->histogram_ctor_template();
  if (!tmpl->NewInstance(env->context()).ToLocal(&obj))
    return {};

  return MakeDetachedBaseObject<HistogramBase>(
      env, obj, lowest, highest, figures);
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_HISTOGRAM_INL_H_

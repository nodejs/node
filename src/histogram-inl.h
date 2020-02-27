#ifndef SRC_HISTOGRAM_INL_H_
#define SRC_HISTOGRAM_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "histogram.h"
#include "base_object-inl.h"
#include "node_internals.h"

namespace node {

void Histogram::Reset() {
  hdr_reset(histogram_.get());
}

bool Histogram::Record(int64_t value) {
  return hdr_record_value(histogram_.get(), value);
}

int64_t Histogram::Min() {
  return hdr_min(histogram_.get());
}

int64_t Histogram::Max() {
  return hdr_max(histogram_.get());
}

double Histogram::Mean() {
  return hdr_mean(histogram_.get());
}

double Histogram::Stddev() {
  return hdr_stddev(histogram_.get());
}

double Histogram::Percentile(double percentile) {
  CHECK_GT(percentile, 0);
  CHECK_LE(percentile, 100);
  return static_cast<double>(
      hdr_value_at_percentile(histogram_.get(), percentile));
}

template <typename Iterator>
void Histogram::Percentiles(Iterator&& fn) {
  hdr_iter iter;
  hdr_iter_percentile_init(&iter, histogram_.get(), 1);
  while (hdr_iter_next(&iter)) {
    double key = iter.specifics.percentiles.percentile;
    double value = static_cast<double>(iter.value);
    fn(key, value);
  }
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

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_HISTOGRAM_INL_H_

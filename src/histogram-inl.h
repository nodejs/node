#ifndef SRC_HISTOGRAM_INL_H_
#define SRC_HISTOGRAM_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "histogram.h"
#include "node_internals.h"

namespace node {

inline Histogram::Histogram(int64_t lowest, int64_t highest, int figures) {
  CHECK_EQ(0, hdr_init(lowest, highest, figures, &histogram_));
}

inline Histogram::~Histogram() {
  hdr_close(histogram_);
}

inline void Histogram::Reset() {
  hdr_reset(histogram_);
}

inline bool Histogram::Record(int64_t value) {
  return hdr_record_value(histogram_, value);
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

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_HISTOGRAM_INL_H_

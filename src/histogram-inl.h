#ifndef SRC_HISTOGRAM_INL_H_
#define SRC_HISTOGRAM_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "histogram.h"
#include "base_object-inl.h"
#include "node_internals.h"

namespace node {

void Histogram::Reset() {
  Mutex::ScopedLock lock(mutex_);
  hdr_reset(histogram_.get());
  exceeds_ = 0;
  prev_ = 0;
}

int64_t Histogram::Min() {
  Mutex::ScopedLock lock(mutex_);
  return hdr_min(histogram_.get());
}

int64_t Histogram::Max() {
  Mutex::ScopedLock lock(mutex_);
  return hdr_max(histogram_.get());
}

double Histogram::Mean() {
  Mutex::ScopedLock lock(mutex_);
  return hdr_mean(histogram_.get());
}

double Histogram::Stddev() {
  Mutex::ScopedLock lock(mutex_);
  return hdr_stddev(histogram_.get());
}

double Histogram::Percentile(double percentile) {
  Mutex::ScopedLock lock(mutex_);
  CHECK_GT(percentile, 0);
  CHECK_LE(percentile, 100);
  return static_cast<double>(
      hdr_value_at_percentile(histogram_.get(), percentile));
}

template <typename Iterator>
void Histogram::Percentiles(Iterator&& fn) {
  Mutex::ScopedLock lock(mutex_);
  hdr_iter iter;
  hdr_iter_percentile_init(&iter, histogram_.get(), 1);
  while (hdr_iter_next(&iter)) {
    double key = iter.specifics.percentiles.percentile;
    double value = static_cast<double>(iter.value);
    fn(key, value);
  }
}

bool Histogram::Record(int64_t value) {
  Mutex::ScopedLock lock(mutex_);
  return hdr_record_value(histogram_.get(), value);
}

uint64_t Histogram::RecordDelta() {
  Mutex::ScopedLock lock(mutex_);
  uint64_t time = uv_hrtime();
  uint64_t delta = 0;
  if (prev_ > 0) {
    delta = time - prev_;
    if (delta > 0) {
      if (!hdr_record_value(histogram_.get(), delta) && exceeds_ < 0xFFFFFFFF)
        exceeds_++;
    }
  }
  prev_ = time;
  return delta;
}

size_t Histogram::GetMemorySize() const {
  Mutex::ScopedLock lock(mutex_);
  return hdr_get_memory_size(histogram_.get());
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_HISTOGRAM_INL_H_

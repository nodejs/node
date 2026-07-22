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
  count_ = 0;
  prev_ = 0;
}

double Histogram::Add(const Histogram& other) {
  Mutex::ScopedLock lock(mutex_);
  count_ += other.count_;
  exceeds_ += other.exceeds_;
  if (other.prev_ > prev_)
    prev_ = other.prev_;
  return static_cast<double>(hdr_add(histogram_.get(), other.histogram_.get()));
}

size_t Histogram::Count() const {
  Mutex::ScopedLock lock(mutex_);
  return count_;
}

int64_t Histogram::Min() const {
  Mutex::ScopedLock lock(mutex_);
  return hdr_min(histogram_.get());
}

int64_t Histogram::Max() const {
  Mutex::ScopedLock lock(mutex_);
  return hdr_max(histogram_.get());
}

double Histogram::Mean() const {
  Mutex::ScopedLock lock(mutex_);
  return hdr_mean(histogram_.get());
}

double Histogram::Stddev() const {
  Mutex::ScopedLock lock(mutex_);
  return hdr_stddev(histogram_.get());
}

int64_t Histogram::Percentile(double percentile) const {
  Mutex::ScopedLock lock(mutex_);
  CHECK_GT(percentile, 0);
  CHECK_LE(percentile, 100);
  return hdr_value_at_percentile(histogram_.get(), percentile);
}

template <typename Iterator>
void Histogram::Percentiles(Iterator&& fn) {
  Mutex::ScopedLock lock(mutex_);
  hdr_iter iter;
  hdr_iter_percentile_init(&iter, histogram_.get(), 1);
  while (hdr_iter_next(&iter)) {
    double key = iter.specifics.percentiles.percentile;
    fn(key, iter.value);
  }
}

bool Histogram::Record(int64_t value) {
  Mutex::ScopedLock lock(mutex_);
  bool recorded = hdr_record_value(histogram_.get(), value);
  if (!recorded)
    exceeds_++;
  else
    count_++;
  return recorded;
}

uint64_t Histogram::RecordDelta() {
  Mutex::ScopedLock lock(mutex_);
  uint64_t time = uv_hrtime();
  int64_t delta = 0;
  if (prev_ > 0) {
    CHECK_GE(time, prev_);
    delta = time - prev_;
    if (hdr_record_value(histogram_.get(), delta))
      count_++;
    else
      exceeds_++;
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

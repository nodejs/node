#ifndef SRC_HISTOGRAM_INL_H_
#define SRC_HISTOGRAM_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "histogram.h"
#include "base_object-inl.h"
#include "node_internals.h"

namespace node {

void Histogram::Reset() {
  RwLock::ScopedWriteLock lock(mutex_);
  hdr_reset(histogram_.get());
  exceeds_ = 0;
  prev_ = 0;
}

double Histogram::Add(const Histogram& other) {
  auto do_add = [&]() {
    exceeds_ += other.exceeds_;
    if (other.prev_ > prev_) prev_ = other.prev_;
    // hdr_add merges all bucket counts and total_count internally.
    return static_cast<double>(
        hdr_add(histogram_.get(), other.histogram_.get()));
  };

  // When adding a histogram to itself, a single write lock suffices.
  if (this == &other) {
    RwLock::ScopedWriteLock lock(mutex_);
    return do_add();
  }

  // Write-lock this (modified), read-lock other (only read).
  // Lock in pointer order to prevent deadlock.
  if (this < &other) {
    RwLock::ScopedWriteLock lock1(mutex_);
    RwLock::ScopedReadLock lock2(other.mutex_);
    return do_add();
  }

  RwLock::ScopedReadLock lock1(other.mutex_);
  RwLock::ScopedWriteLock lock2(mutex_);
  return do_add();
}

size_t Histogram::Count() const {
  RwLock::ScopedReadLock lock(mutex_);
  return static_cast<size_t>(histogram_->total_count);
}

size_t Histogram::Exceeds() const {
  RwLock::ScopedReadLock lock(mutex_);
  return exceeds_;
}

int64_t Histogram::Min() const {
  RwLock::ScopedReadLock lock(mutex_);
  return hdr_min(histogram_.get());
}

int64_t Histogram::Max() const {
  RwLock::ScopedReadLock lock(mutex_);
  return hdr_max(histogram_.get());
}

double Histogram::Mean() const {
  RwLock::ScopedReadLock lock(mutex_);
  return hdr_mean(histogram_.get());
}

double Histogram::Stddev() const {
  RwLock::ScopedReadLock lock(mutex_);
  return hdr_stddev(histogram_.get());
}

int64_t Histogram::Percentile(double percentile) const {
  RwLock::ScopedReadLock lock(mutex_);
  CHECK_GT(percentile, 0);
  CHECK_LE(percentile, 100);
  return hdr_value_at_percentile(histogram_.get(), percentile);
}

template <typename Iterator>
void Histogram::Percentiles(Iterator&& fn) const {
  RwLock::ScopedReadLock lock(mutex_);
  hdr_iter iter;
  hdr_iter_percentile_init(&iter, histogram_.get(), 1);
  while (hdr_iter_next(&iter)) {
    double key = iter.specifics.percentiles.percentile;
    fn(key, iter.value);
  }
}

int64_t Histogram::CountAt(int64_t value) const {
  RwLock::ScopedReadLock lock(mutex_);
  return hdr_count_at_value(histogram_.get(), value);
}

bool Histogram::RecordCorrected(int64_t value, int64_t expected_interval) {
  RwLock::ScopedWriteLock lock(mutex_);
  bool recorded =
      hdr_record_corrected_value(histogram_.get(), value, expected_interval);
  if (!recorded) exceeds_++;
  return recorded;
}

bool Histogram::Record(int64_t value) {
  RwLock::ScopedWriteLock lock(mutex_);
  bool recorded = hdr_record_value(histogram_.get(), value);
  if (!recorded) exceeds_++;
  return recorded;
}

uint64_t Histogram::RecordDelta() {
  RwLock::ScopedWriteLock lock(mutex_);
  uint64_t time = uv_hrtime();
  int64_t delta = 0;
  if (prev_ > 0) {
    CHECK_GE(time, prev_);
    delta = time - prev_;
    if (!hdr_record_value(histogram_.get(), delta)) exceeds_++;
  }
  prev_ = time;
  return delta;
}

size_t Histogram::GetMemorySize() const {
  RwLock::ScopedReadLock lock(mutex_);
  return hdr_get_memory_size(histogram_.get());
}

template <typename Iterator>
void Histogram::LinearBuckets(int64_t step_size, Iterator&& fn) const {
  RwLock::ScopedReadLock lock(mutex_);
  hdr_iter iter;
  hdr_iter_linear_init(&iter, histogram_.get(), step_size);
  while (hdr_iter_next(&iter)) {
    fn(iter.value, iter.specifics.linear.count_added_in_this_iteration_step);
  }
}

template <typename Iterator>
void Histogram::LogBuckets(int64_t first_bucket,
                           double log_base,
                           Iterator&& fn) const {
  RwLock::ScopedReadLock lock(mutex_);
  hdr_iter iter;
  hdr_iter_log_init(&iter, histogram_.get(), first_bucket, log_base);
  while (hdr_iter_next(&iter)) {
    fn(iter.value, iter.specifics.log.count_added_in_this_iteration_step);
  }
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_HISTOGRAM_INL_H_

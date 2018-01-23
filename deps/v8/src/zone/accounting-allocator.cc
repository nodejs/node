// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/zone/accounting-allocator.h"

#include <cstdlib>

#if V8_LIBC_BIONIC
#include <malloc.h>  // NOLINT
#endif

#include "src/allocation.h"

namespace v8 {
namespace internal {

AccountingAllocator::AccountingAllocator() : unused_segments_mutex_() {
  static const size_t kDefaultBucketMaxSize = 5;

  memory_pressure_level_.SetValue(MemoryPressureLevel::kNone);
  std::fill(unused_segments_heads_, unused_segments_heads_ + kNumberBuckets,
            nullptr);
  std::fill(unused_segments_sizes_, unused_segments_sizes_ + kNumberBuckets, 0);
  std::fill(unused_segments_max_sizes_,
            unused_segments_max_sizes_ + kNumberBuckets, kDefaultBucketMaxSize);
}

AccountingAllocator::~AccountingAllocator() { ClearPool(); }

void AccountingAllocator::MemoryPressureNotification(
    MemoryPressureLevel level) {
  memory_pressure_level_.SetValue(level);

  if (level != MemoryPressureLevel::kNone) {
    ClearPool();
  }
}

void AccountingAllocator::ConfigureSegmentPool(const size_t max_pool_size) {
  // The sum of the bytes of one segment of each size.
  static const size_t full_size = (size_t(1) << (kMaxSegmentSizePower + 1)) -
                                  (size_t(1) << kMinSegmentSizePower);
  size_t fits_fully = max_pool_size / full_size;

  base::LockGuard<base::Mutex> lock_guard(&unused_segments_mutex_);

  // We assume few zones (less than 'fits_fully' many) to be active at the same
  // time. When zones grow regularly, they will keep requesting segments of
  // increasing size each time. Therefore we try to get as many segments with an
  // equal number of segments of each size as possible.
  // The remaining space is used to make more room for an 'incomplete set' of
  // segments beginning with the smaller ones.
  // This code will work best if the max_pool_size is a multiple of the
  // full_size. If max_pool_size is no sum of segment sizes the actual pool
  // size might be smaller then max_pool_size. Note that no actual memory gets
  // wasted though.
  // TODO(heimbuef): Determine better strategy generating a segment sizes
  // distribution that is closer to real/benchmark usecases and uses the given
  // max_pool_size more efficiently.
  size_t total_size = fits_fully * full_size;

  for (size_t power = 0; power < kNumberBuckets; ++power) {
    if (total_size + (size_t(1) << (power + kMinSegmentSizePower)) <=
        max_pool_size) {
      unused_segments_max_sizes_[power] = fits_fully + 1;
      total_size += size_t(1) << power;
    } else {
      unused_segments_max_sizes_[power] = fits_fully;
    }
  }
}

Segment* AccountingAllocator::GetSegment(size_t bytes) {
  Segment* result = GetSegmentFromPool(bytes);
  if (result == nullptr) {
    result = AllocateSegment(bytes);
    if (result != nullptr) {
      result->Initialize(bytes);
    }
  }

  return result;
}

Segment* AccountingAllocator::AllocateSegment(size_t bytes) {
  void* memory = AllocWithRetry(bytes);
  if (memory != nullptr) {
    base::AtomicWord current =
        base::Relaxed_AtomicIncrement(&current_memory_usage_, bytes);
    base::AtomicWord max = base::Relaxed_Load(&max_memory_usage_);
    while (current > max) {
      max = base::Relaxed_CompareAndSwap(&max_memory_usage_, max, current);
    }
  }
  return reinterpret_cast<Segment*>(memory);
}

void AccountingAllocator::ReturnSegment(Segment* segment) {
  segment->ZapContents();

  if (memory_pressure_level_.Value() != MemoryPressureLevel::kNone) {
    FreeSegment(segment);
  } else if (!AddSegmentToPool(segment)) {
    FreeSegment(segment);
  }
}

void AccountingAllocator::FreeSegment(Segment* memory) {
  base::Relaxed_AtomicIncrement(&current_memory_usage_,
                                -static_cast<base::AtomicWord>(memory->size()));
  memory->ZapHeader();
  free(memory);
}

size_t AccountingAllocator::GetCurrentMemoryUsage() const {
  return base::Relaxed_Load(&current_memory_usage_);
}

size_t AccountingAllocator::GetMaxMemoryUsage() const {
  return base::Relaxed_Load(&max_memory_usage_);
}

size_t AccountingAllocator::GetCurrentPoolSize() const {
  return base::Relaxed_Load(&current_pool_size_);
}

Segment* AccountingAllocator::GetSegmentFromPool(size_t requested_size) {
  if (requested_size > (1 << kMaxSegmentSizePower)) {
    return nullptr;
  }

  size_t power = kMinSegmentSizePower;
  while (requested_size > (static_cast<size_t>(1) << power)) power++;

  DCHECK_GE(power, kMinSegmentSizePower + 0);
  power -= kMinSegmentSizePower;

  Segment* segment;
  {
    base::LockGuard<base::Mutex> lock_guard(&unused_segments_mutex_);

    segment = unused_segments_heads_[power];

    if (segment != nullptr) {
      unused_segments_heads_[power] = segment->next();
      segment->set_next(nullptr);

      unused_segments_sizes_[power]--;
      base::Relaxed_AtomicIncrement(
          &current_pool_size_, -static_cast<base::AtomicWord>(segment->size()));
    }
  }

  if (segment) {
    DCHECK_GE(segment->size(), requested_size);
  }
  return segment;
}

bool AccountingAllocator::AddSegmentToPool(Segment* segment) {
  size_t size = segment->size();

  if (size >= (1 << (kMaxSegmentSizePower + 1))) return false;

  if (size < (1 << kMinSegmentSizePower)) return false;

  size_t power = kMaxSegmentSizePower;

  while (size < (static_cast<size_t>(1) << power)) power--;

  DCHECK_GE(power, kMinSegmentSizePower + 0);
  power -= kMinSegmentSizePower;

  {
    base::LockGuard<base::Mutex> lock_guard(&unused_segments_mutex_);

    if (unused_segments_sizes_[power] >= unused_segments_max_sizes_[power]) {
      return false;
    }

    segment->set_next(unused_segments_heads_[power]);
    unused_segments_heads_[power] = segment;
    base::Relaxed_AtomicIncrement(&current_pool_size_, size);
    unused_segments_sizes_[power]++;
  }

  return true;
}

void AccountingAllocator::ClearPool() {
  base::LockGuard<base::Mutex> lock_guard(&unused_segments_mutex_);

  for (size_t power = 0; power <= kMaxSegmentSizePower - kMinSegmentSizePower;
       power++) {
    Segment* current = unused_segments_heads_[power];
    while (current) {
      Segment* next = current->next();
      FreeSegment(current);
      current = next;
    }
    unused_segments_heads_[power] = nullptr;
  }
}

}  // namespace internal
}  // namespace v8

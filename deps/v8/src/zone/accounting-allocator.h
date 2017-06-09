// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_ACCOUNTING_ALLOCATOR_H_
#define V8_ZONE_ACCOUNTING_ALLOCATOR_H_

#include "include/v8-platform.h"
#include "src/base/atomic-utils.h"
#include "src/base/atomicops.h"
#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/semaphore.h"
#include "src/base/platform/time.h"
#include "src/zone/zone-segment.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // nogncheck

namespace v8 {
namespace internal {

class V8_EXPORT_PRIVATE AccountingAllocator {
 public:
  static const size_t kMaxPoolSizeLowMemoryDevice = 8ul * KB;
  static const size_t kMaxPoolSizeMediumMemoryDevice = 8ul * KB;
  static const size_t kMaxPoolSizeHighMemoryDevice = 8ul * KB;
  static const size_t kMaxPoolSizeHugeMemoryDevice = 8ul * KB;

  AccountingAllocator();
  virtual ~AccountingAllocator();

  // Gets an empty segment from the pool or creates a new one.
  virtual Segment* GetSegment(size_t bytes);
  // Return unneeded segments to either insert them into the pool or release
  // them if the pool is already full or memory pressure is high.
  virtual void ReturnSegment(Segment* memory);

  size_t GetCurrentMemoryUsage() const;
  size_t GetMaxMemoryUsage() const;

  size_t GetCurrentPoolSize() const;

  void MemoryPressureNotification(MemoryPressureLevel level);
  // Configures the zone segment pool size limits so the pool does not
  // grow bigger than max_pool_size.
  // TODO(heimbuef): Do not accept segments to pool that are larger than
  // their size class requires. Sometimes the zones generate weird segments.
  void ConfigureSegmentPool(const size_t max_pool_size);

  virtual void ZoneCreation(const Zone* zone) {}
  virtual void ZoneDestruction(const Zone* zone) {}

 private:
  FRIEND_TEST(Zone, SegmentPoolConstraints);

  static const size_t kMinSegmentSizePower = 13;
  static const size_t kMaxSegmentSizePower = 18;

  STATIC_ASSERT(kMinSegmentSizePower <= kMaxSegmentSizePower);

  static const size_t kNumberBuckets =
      1 + kMaxSegmentSizePower - kMinSegmentSizePower;

  // Allocates a new segment. Returns nullptr on failed allocation.
  Segment* AllocateSegment(size_t bytes);
  void FreeSegment(Segment* memory);

  // Returns a segment from the pool of at least the requested size.
  Segment* GetSegmentFromPool(size_t requested_size);
  // Trys to add a segment to the pool. Returns false if the pool is full.
  bool AddSegmentToPool(Segment* segment);

  // Empties the pool and puts all its contents onto the garbage stack.
  void ClearPool();

  Segment* unused_segments_heads_[kNumberBuckets];

  size_t unused_segments_sizes_[kNumberBuckets];
  size_t unused_segments_max_sizes_[kNumberBuckets];

  base::Mutex unused_segments_mutex_;

  base::AtomicWord current_memory_usage_ = 0;
  base::AtomicWord max_memory_usage_ = 0;
  base::AtomicWord current_pool_size_ = 0;

  base::AtomicValue<MemoryPressureLevel> memory_pressure_level_;

  DISALLOW_COPY_AND_ASSIGN(AccountingAllocator);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_ACCOUNTING_ALLOCATOR_H_

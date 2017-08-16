// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/zone/accounting-allocator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

TEST(Zone, SegmentPoolConstraints) {
  size_t sizes[]{
      0,  // Corner case
      AccountingAllocator::kMaxPoolSizeLowMemoryDevice,
      AccountingAllocator::kMaxPoolSizeMediumMemoryDevice,
      AccountingAllocator::kMaxPoolSizeHighMemoryDevice,
      AccountingAllocator::kMaxPoolSizeHugeMemoryDevice,
      GB  // Something really large
  };

  AccountingAllocator allocator;
  for (size_t size : sizes) {
    allocator.ConfigureSegmentPool(size);
    size_t total_size = 0;
    for (size_t power = 0; power < AccountingAllocator::kNumberBuckets;
         ++power) {
      total_size +=
          allocator.unused_segments_max_sizes_[power] * (size_t(1) << power);
    }
    EXPECT_LE(total_size, size);
  }
}

}  // namespace internal
}  // namespace v8

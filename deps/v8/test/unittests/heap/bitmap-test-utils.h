// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_HEAP_BITMAP_TEST_UTILS_H_
#define V8_UNITTESTS_HEAP_BITMAP_TEST_UTILS_H_

#include "src/base/build_config.h"
#include "src/base/platform/memory.h"
#include "src/common/globals.h"
#include "src/heap/marking.h"
#include "src/heap/memory-chunk-layout.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8::internal {

class TestWithBitmap : public ::testing::Test {
 public:
  static constexpr size_t kPageSize = 1u << kPageSizeBits;

  TestWithBitmap()
      : memory_(reinterpret_cast<uint8_t*>(
            base::AlignedAlloc(kPageSize, kPageSize))) {
    memset(memory_, 0, kPageSize);
  }

  ~TestWithBitmap() override { base::AlignedFree(memory_); }

  uint8_t* raw_bitmap() {
    return reinterpret_cast<uint8_t*>(memory_ +
                                      MemoryChunkLayout::kMarkingBitmapOffset);
  }
  MarkingBitmap* bitmap() {
    return reinterpret_cast<MarkingBitmap*>(raw_bitmap());
  }

 private:
  uint8_t* memory_;
};

}  // namespace v8::internal

#endif  // V8_UNITTESTS_HEAP_BITMAP_TEST_UTILS_H_

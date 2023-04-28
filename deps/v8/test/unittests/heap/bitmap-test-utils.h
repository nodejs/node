// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_HEAP_BITMAP_TEST_UTILS_H_
#define V8_UNITTESTS_HEAP_BITMAP_TEST_UTILS_H_

#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

template <typename T>
class TestWithBitmap : public ::testing::Test {
 public:
  TestWithBitmap() : memory_(new uint8_t[Bitmap::kSize]) {
    memset(memory_, 0, Bitmap::kSize);
  }

  ~TestWithBitmap() override { delete[] memory_; }

  T* bitmap() { return reinterpret_cast<T*>(memory_); }
  uint8_t* raw_bitmap() { return memory_; }

 private:
  uint8_t* memory_;
};

using BitmapTypes = ::testing::Types<ConcurrentBitmap<AccessMode::NON_ATOMIC>,
                                     ConcurrentBitmap<AccessMode::ATOMIC>>;

}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_HEAP_BITMAP_TEST_UTILS_H_

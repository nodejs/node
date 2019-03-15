// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/globals.h"
#include "src/heap/marking.h"
#include "test/unittests/heap/bitmap-test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

template <typename T>
using MarkingTest = TestWithBitmap<T>;

TYPED_TEST_SUITE(MarkingTest, BitmapTypes);

TYPED_TEST(MarkingTest, TransitionWhiteBlackWhite) {
  auto bitmap = this->bitmap();
  const int kLocationsSize = 3;
  int position[kLocationsSize] = {
      Bitmap::kBitsPerCell - 2, Bitmap::kBitsPerCell - 1, Bitmap::kBitsPerCell};
  for (int i = 0; i < kLocationsSize; i++) {
    MarkBit mark_bit = bitmap->MarkBitFromIndex(position[i]);
    CHECK(Marking::IsWhite(mark_bit));
    CHECK(!Marking::IsImpossible(mark_bit));
    Marking::WhiteToBlack<AccessMode::NON_ATOMIC>(mark_bit);
    CHECK(Marking::IsBlack(mark_bit));
    CHECK(!Marking::IsImpossible(mark_bit));
    Marking::MarkWhite(mark_bit);
    CHECK(Marking::IsWhite(mark_bit));
    CHECK(!Marking::IsImpossible(mark_bit));
  }
}

TYPED_TEST(MarkingTest, TransitionWhiteGreyBlack) {
  auto bitmap = this->bitmap();
  const int kLocationsSize = 3;
  int position[kLocationsSize] = {
      Bitmap::kBitsPerCell - 2, Bitmap::kBitsPerCell - 1, Bitmap::kBitsPerCell};
  for (int i = 0; i < kLocationsSize; i++) {
    MarkBit mark_bit = bitmap->MarkBitFromIndex(position[i]);
    CHECK(Marking::IsWhite(mark_bit));
    CHECK(!Marking::IsBlackOrGrey(mark_bit));
    CHECK(!Marking::IsImpossible(mark_bit));
    Marking::WhiteToGrey<AccessMode::NON_ATOMIC>(mark_bit);
    CHECK(Marking::IsGrey(mark_bit));
    CHECK(Marking::IsBlackOrGrey(mark_bit));
    CHECK(!Marking::IsImpossible(mark_bit));
    Marking::GreyToBlack<AccessMode::NON_ATOMIC>(mark_bit);
    CHECK(Marking::IsBlack(mark_bit));
    CHECK(Marking::IsBlackOrGrey(mark_bit));
    CHECK(!Marking::IsImpossible(mark_bit));
    Marking::MarkWhite(mark_bit);
    CHECK(Marking::IsWhite(mark_bit));
    CHECK(!Marking::IsImpossible(mark_bit));
  }
}

}  // namespace internal
}  // namespace v8

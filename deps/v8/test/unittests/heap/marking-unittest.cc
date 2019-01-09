// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/globals.h"
#include "src/heap/marking.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {


TEST(Marking, TransitionWhiteBlackWhite) {
  Bitmap* bitmap = reinterpret_cast<Bitmap*>(
      calloc(Bitmap::kSize / kTaggedSize, kTaggedSize));
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
  free(bitmap);
}

TEST(Marking, TransitionWhiteGreyBlack) {
  Bitmap* bitmap = reinterpret_cast<Bitmap*>(
      calloc(Bitmap::kSize / kTaggedSize, kTaggedSize));
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
  free(bitmap);
}

TEST(Marking, SetAndClearRange) {
  Bitmap* bitmap = reinterpret_cast<Bitmap*>(
      calloc(Bitmap::kSize / kTaggedSize, kTaggedSize));
  for (int i = 0; i < 3; i++) {
    bitmap->SetRange(i, Bitmap::kBitsPerCell + i);
    CHECK_EQ(reinterpret_cast<uint32_t*>(bitmap)[0], 0xFFFFFFFFu << i);
    CHECK_EQ(reinterpret_cast<uint32_t*>(bitmap)[1], (1u << i) - 1);
    bitmap->ClearRange(i, Bitmap::kBitsPerCell + i);
    CHECK_EQ(reinterpret_cast<uint32_t*>(bitmap)[0], 0x0u);
    CHECK_EQ(reinterpret_cast<uint32_t*>(bitmap)[1], 0x0u);
  }
  free(bitmap);
}

TEST(Marking, ClearMultipleRanges) {
  Bitmap* bitmap = reinterpret_cast<Bitmap*>(
      calloc(Bitmap::kSize / kTaggedSize, kTaggedSize));
  CHECK(bitmap->AllBitsClearInRange(0, Bitmap::kBitsPerCell * 3));
  bitmap->SetRange(0, Bitmap::kBitsPerCell * 3);
  CHECK_EQ(reinterpret_cast<uint32_t*>(bitmap)[0], 0xFFFFFFFFu);
  CHECK_EQ(reinterpret_cast<uint32_t*>(bitmap)[1], 0xFFFFFFFFu);
  CHECK_EQ(reinterpret_cast<uint32_t*>(bitmap)[2], 0xFFFFFFFFu);
  CHECK(bitmap->AllBitsSetInRange(0, Bitmap::kBitsPerCell * 3));
  bitmap->ClearRange(Bitmap::kBitsPerCell / 2, Bitmap::kBitsPerCell);
  bitmap->ClearRange(Bitmap::kBitsPerCell,
                     Bitmap::kBitsPerCell + Bitmap::kBitsPerCell / 2);
  bitmap->ClearRange(Bitmap::kBitsPerCell * 2 + 8,
                     Bitmap::kBitsPerCell * 2 + 16);
  bitmap->ClearRange(Bitmap::kBitsPerCell * 2 + 24, Bitmap::kBitsPerCell * 3);
  CHECK_EQ(reinterpret_cast<uint32_t*>(bitmap)[0], 0xFFFFu);
  CHECK(bitmap->AllBitsSetInRange(0, Bitmap::kBitsPerCell / 2));
  CHECK(bitmap->AllBitsClearInRange(Bitmap::kBitsPerCell / 2,
                                    Bitmap::kBitsPerCell));
  CHECK_EQ(reinterpret_cast<uint32_t*>(bitmap)[1], 0xFFFF0000u);
  CHECK(
      bitmap->AllBitsSetInRange(Bitmap::kBitsPerCell + Bitmap::kBitsPerCell / 2,
                                2 * Bitmap::kBitsPerCell));
  CHECK(bitmap->AllBitsClearInRange(
      Bitmap::kBitsPerCell, Bitmap::kBitsPerCell + Bitmap::kBitsPerCell / 2));
  CHECK_EQ(reinterpret_cast<uint32_t*>(bitmap)[2], 0xFF00FFu);
  CHECK(bitmap->AllBitsSetInRange(2 * Bitmap::kBitsPerCell,
                                  2 * Bitmap::kBitsPerCell + 8));
  CHECK(bitmap->AllBitsClearInRange(2 * Bitmap::kBitsPerCell + 24,
                                    Bitmap::kBitsPerCell * 3));
  free(bitmap);
}
}  // namespace internal
}  // namespace v8

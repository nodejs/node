// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/spaces.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using v8::internal::Bitmap;

class BitmapTest : public ::testing::Test {
 public:
  static const uint32_t kBlackCell;
  static const uint32_t kWhiteCell;
  static const uint32_t kBlackByte;
  static const uint32_t kWhiteByte;

  BitmapTest() : memory_(new uint8_t[Bitmap::kSize]) {
    memset(memory_, 0, Bitmap::kSize);
  }

  virtual ~BitmapTest() { delete[] memory_; }

  Bitmap* bitmap() { return reinterpret_cast<Bitmap*>(memory_); }
  uint8_t* raw_bitmap() { return memory_; }

 private:
  uint8_t* memory_;
};


const uint32_t BitmapTest::kBlackCell = 0xAAAAAAAA;
const uint32_t BitmapTest::kWhiteCell = 0x00000000;
const uint32_t BitmapTest::kBlackByte = 0xAA;
const uint32_t BitmapTest::kWhiteByte = 0x00;


TEST_F(BitmapTest, IsZeroInitialized) {
  // We require all tests to start from a zero-initialized bitmap. Manually
  // verify this invariant here.
  for (size_t i = 0; i < Bitmap::kSize; i++) {
    EXPECT_EQ(raw_bitmap()[i], kWhiteByte);
  }
}


TEST_F(BitmapTest, Cells) {
  Bitmap* bm = bitmap();
  bm->cells()[1] = kBlackCell;
  uint8_t* raw = raw_bitmap();
  int second_cell_base = Bitmap::kBytesPerCell;
  for (size_t i = 0; i < Bitmap::kBytesPerCell; i++) {
    EXPECT_EQ(raw[second_cell_base + i], kBlackByte);
  }
}


TEST_F(BitmapTest, CellsCount) {
  int last_cell_index = bitmap()->CellsCount() - 1;
  bitmap()->cells()[last_cell_index] = kBlackCell;
  // Manually verify on raw memory.
  uint8_t* raw = raw_bitmap();
  for (size_t i = 0; i < Bitmap::kSize; i++) {
    // Last cell should be set.
    if (i >= (Bitmap::kSize - Bitmap::kBytesPerCell)) {
      EXPECT_EQ(raw[i], kBlackByte);
    } else {
      EXPECT_EQ(raw[i], kWhiteByte);
    }
  }
}


TEST_F(BitmapTest, IsClean) {
  Bitmap* bm = bitmap();
  EXPECT_TRUE(bm->IsClean());
  bm->cells()[0] = kBlackCell;
  EXPECT_FALSE(bm->IsClean());
}


TEST_F(BitmapTest, ClearRange1) {
  Bitmap* bm = bitmap();
  bm->cells()[0] = kBlackCell;
  bm->cells()[1] = kBlackCell;
  bm->cells()[2] = kBlackCell;
  bm->ClearRange(0, Bitmap::kBitsPerCell + Bitmap::kBitsPerCell / 2);
  EXPECT_EQ(bm->cells()[0], kWhiteCell);
  EXPECT_EQ(bm->cells()[1], 0xAAAA0000);
  EXPECT_EQ(bm->cells()[2], kBlackCell);
}


TEST_F(BitmapTest, ClearRange2) {
  Bitmap* bm = bitmap();
  bm->cells()[0] = kBlackCell;
  bm->cells()[1] = kBlackCell;
  bm->cells()[2] = kBlackCell;
  bm->ClearRange(Bitmap::kBitsPerCell,
                 Bitmap::kBitsPerCell + Bitmap::kBitsPerCell / 2);
  EXPECT_EQ(bm->cells()[0], kBlackCell);
  EXPECT_EQ(bm->cells()[1], 0xAAAA0000);
  EXPECT_EQ(bm->cells()[2], kBlackCell);
}

}  // namespace

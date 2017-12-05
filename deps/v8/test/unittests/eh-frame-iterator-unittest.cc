// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/eh-frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

// Test enabled only on supported architectures.
#if defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_ARM) || \
    defined(V8_TARGET_ARCH_ARM64)

namespace {

class EhFrameIteratorTest : public testing::Test {};

}  // namespace

TEST_F(EhFrameIteratorTest, Values) {
  // Assuming little endian.
  static const byte kEncoded[] = {0xde, 0xc0, 0xad, 0xde, 0xef, 0xbe, 0xff};
  EhFrameIterator iterator(&kEncoded[0], &kEncoded[0] + sizeof(kEncoded));
  EXPECT_EQ(0xdeadc0de, iterator.GetNextUInt32());
  EXPECT_EQ(0xbeef, iterator.GetNextUInt16());
  EXPECT_EQ(0xff, iterator.GetNextByte());
  EXPECT_TRUE(iterator.Done());
}

TEST_F(EhFrameIteratorTest, Skip) {
  static const byte kEncoded[] = {0xde, 0xad, 0xc0, 0xde};
  EhFrameIterator iterator(&kEncoded[0], &kEncoded[0] + sizeof(kEncoded));
  iterator.Skip(2);
  EXPECT_EQ(2, iterator.GetCurrentOffset());
  EXPECT_EQ(0xc0, iterator.GetNextByte());
  iterator.Skip(1);
  EXPECT_TRUE(iterator.Done());
}

TEST_F(EhFrameIteratorTest, ULEB128Decoding) {
  static const byte kEncoded[] = {0xe5, 0x8e, 0x26};
  EhFrameIterator iterator(&kEncoded[0], &kEncoded[0] + sizeof(kEncoded));
  EXPECT_EQ(624485u, iterator.GetNextULeb128());
  EXPECT_TRUE(iterator.Done());
}

TEST_F(EhFrameIteratorTest, SLEB128DecodingPositive) {
  static const byte kEncoded[] = {0xe5, 0x8e, 0x26};
  EhFrameIterator iterator(&kEncoded[0], &kEncoded[0] + sizeof(kEncoded));
  EXPECT_EQ(624485, iterator.GetNextSLeb128());
  EXPECT_TRUE(iterator.Done());
}

TEST_F(EhFrameIteratorTest, SLEB128DecodingNegative) {
  static const byte kEncoded[] = {0x9b, 0xf1, 0x59};
  EhFrameIterator iterator(&kEncoded[0], &kEncoded[0] + sizeof(kEncoded));
  EXPECT_EQ(-624485, iterator.GetNextSLeb128());
  EXPECT_TRUE(iterator.Done());
}

#endif

}  // namespace internal
}  // namespace v8

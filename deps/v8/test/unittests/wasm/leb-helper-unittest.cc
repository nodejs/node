// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "src/objects/objects-inl.h"
#include "src/wasm/decoder.h"
#include "src/wasm/leb-helper.h"

namespace v8 {
namespace internal {
namespace wasm {

class LEBHelperTest : public TestWithZone {};

TEST_F(LEBHelperTest, sizeof_u32v) {
  EXPECT_EQ(1u, LEBHelper::sizeof_u32v(0));
  EXPECT_EQ(1u, LEBHelper::sizeof_u32v(1));
  EXPECT_EQ(1u, LEBHelper::sizeof_u32v(3));

  for (uint32_t i = 4; i < 128; i++) {
    EXPECT_EQ(1u, LEBHelper::sizeof_u32v(i));
  }

  for (uint32_t i = (1u << 7); i < (1u << 9); i++) {
    EXPECT_EQ(2u, LEBHelper::sizeof_u32v(i));
  }

  for (uint32_t i = (1u << 14); i < (1u << 16); i += 33) {
    EXPECT_EQ(3u, LEBHelper::sizeof_u32v(i));
  }

  for (uint32_t i = (1u << 21); i < (1u << 24); i += 33999) {
    EXPECT_EQ(4u, LEBHelper::sizeof_u32v(i));
  }

  for (uint32_t i = (1u << 28); i < (1u << 31); i += 33997779u) {
    EXPECT_EQ(5u, LEBHelper::sizeof_u32v(i));
  }

  EXPECT_EQ(5u, LEBHelper::sizeof_u32v(0xFFFFFFFF));
}

TEST_F(LEBHelperTest, sizeof_i32v) {
  EXPECT_EQ(1u, LEBHelper::sizeof_i32v(0));
  EXPECT_EQ(1u, LEBHelper::sizeof_i32v(1));
  EXPECT_EQ(1u, LEBHelper::sizeof_i32v(3));

  for (int32_t i = 0; i < (1 << 6); i++) {
    EXPECT_EQ(1u, LEBHelper::sizeof_i32v(i));
  }

  for (int32_t i = (1 << 6); i < (1 << 8); i++) {
    EXPECT_EQ(2u, LEBHelper::sizeof_i32v(i));
  }

  for (int32_t i = (1 << 13); i < (1 << 15); i += 31) {
    EXPECT_EQ(3u, LEBHelper::sizeof_i32v(i));
  }

  for (int32_t i = (1 << 20); i < (1 << 22); i += 31991) {
    EXPECT_EQ(4u, LEBHelper::sizeof_i32v(i));
  }

  for (int32_t i = (1 << 27); i < (1 << 29); i += 3199893) {
    EXPECT_EQ(5u, LEBHelper::sizeof_i32v(i));
  }

  for (int32_t i = -(1 << 6); i <= 0; i++) {
    EXPECT_EQ(1u, LEBHelper::sizeof_i32v(i));
  }

  for (int32_t i = -(1 << 13); i < -(1 << 6); i++) {
    EXPECT_EQ(2u, LEBHelper::sizeof_i32v(i));
  }

  for (int32_t i = -(1 << 20); i < -(1 << 18); i += 11) {
    EXPECT_EQ(3u, LEBHelper::sizeof_i32v(i));
  }

  for (int32_t i = -(1 << 27); i < -(1 << 25); i += 11999) {
    EXPECT_EQ(4u, LEBHelper::sizeof_i32v(i));
  }

  for (int32_t i = -(1 << 30); i < -(1 << 28); i += 1199999) {
    EXPECT_EQ(5u, LEBHelper::sizeof_i32v(i));
  }
}

#define DECLARE_ENCODE_DECODE_CHECKER(ctype, name)                         \
  static void CheckEncodeDecode_##name(ctype val) {                        \
    static const int kSize = 16;                                           \
    static byte buffer[kSize];                                             \
    byte* ptr = buffer;                                                    \
    LEBHelper::write_##name(&ptr, val);                                    \
    EXPECT_EQ(LEBHelper::sizeof_##name(val),                               \
              static_cast<size_t>(ptr - buffer));                          \
    Decoder decoder(buffer, buffer + kSize);                               \
    unsigned length = 0;                                                   \
    ctype result =                                                         \
        decoder.read_##name<Decoder::kNoValidation>(buffer, &length);      \
    EXPECT_EQ(val, result);                                                \
    EXPECT_EQ(LEBHelper::sizeof_##name(val), static_cast<size_t>(length)); \
  }

DECLARE_ENCODE_DECODE_CHECKER(int32_t, i32v)
DECLARE_ENCODE_DECODE_CHECKER(uint32_t, u32v)
DECLARE_ENCODE_DECODE_CHECKER(int64_t, i64v)
DECLARE_ENCODE_DECODE_CHECKER(uint64_t, u64v)

#undef DECLARE_ENCODE_DECODE_CHECKER

TEST_F(LEBHelperTest, WriteAndDecode_u32v) {
  CheckEncodeDecode_u32v(0);
  CheckEncodeDecode_u32v(1);
  CheckEncodeDecode_u32v(5);
  CheckEncodeDecode_u32v(99);
  CheckEncodeDecode_u32v(298);
  CheckEncodeDecode_u32v(87348723);
  CheckEncodeDecode_u32v(77777);

  for (uint32_t val = 0x3A; val != 0; val = val << 1) {
    CheckEncodeDecode_u32v(val);
  }
}

TEST_F(LEBHelperTest, WriteAndDecode_i32v) {
  CheckEncodeDecode_i32v(0);
  CheckEncodeDecode_i32v(1);
  CheckEncodeDecode_i32v(5);
  CheckEncodeDecode_i32v(99);
  CheckEncodeDecode_i32v(298);
  CheckEncodeDecode_i32v(87348723);
  CheckEncodeDecode_i32v(77777);

  CheckEncodeDecode_i32v(-2);
  CheckEncodeDecode_i32v(-4);
  CheckEncodeDecode_i32v(-59);
  CheckEncodeDecode_i32v(-288);
  CheckEncodeDecode_i32v(-12608);
  CheckEncodeDecode_i32v(-87328723);
  CheckEncodeDecode_i32v(-77377);

  for (uint32_t val = 0x3A; val != 0; val = val << 1) {
    CheckEncodeDecode_i32v(base::bit_cast<int32_t>(val));
  }

  for (uint32_t val = 0xFFFFFF3B; val != 0; val = val << 1) {
    CheckEncodeDecode_i32v(base::bit_cast<int32_t>(val));
  }
}

TEST_F(LEBHelperTest, WriteAndDecode_u64v) {
  CheckEncodeDecode_u64v(0);
  CheckEncodeDecode_u64v(1);
  CheckEncodeDecode_u64v(5);
  CheckEncodeDecode_u64v(99);
  CheckEncodeDecode_u64v(298);
  CheckEncodeDecode_u64v(87348723);
  CheckEncodeDecode_u64v(77777);

  for (uint64_t val = 0x3A; val != 0; val = val << 1) {
    CheckEncodeDecode_u64v(val);
  }
}

TEST_F(LEBHelperTest, WriteAndDecode_i64v) {
  CheckEncodeDecode_i64v(0);
  CheckEncodeDecode_i64v(1);
  CheckEncodeDecode_i64v(5);
  CheckEncodeDecode_i64v(99);
  CheckEncodeDecode_i64v(298);
  CheckEncodeDecode_i64v(87348723);
  CheckEncodeDecode_i64v(77777);

  CheckEncodeDecode_i64v(-2);
  CheckEncodeDecode_i64v(-4);
  CheckEncodeDecode_i64v(-59);
  CheckEncodeDecode_i64v(-288);
  CheckEncodeDecode_i64v(-87648723);
  CheckEncodeDecode_i64v(-77377);

  for (uint64_t val = 0x3A; val != 0; val = val << 1) {
    CheckEncodeDecode_i64v(base::bit_cast<int64_t>(val));
  }

  for (uint64_t val = 0xFFFFFFFFFFFFFF3B; val != 0; val = val << 1) {
    CheckEncodeDecode_i64v(base::bit_cast<int64_t>(val));
  }
}
}  // namespace wasm
}  // namespace internal
}  // namespace v8

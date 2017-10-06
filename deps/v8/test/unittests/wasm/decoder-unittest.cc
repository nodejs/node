// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "src/objects-inl.h"
#include "src/wasm/decoder.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {

class DecoderTest : public TestWithZone {
 public:
  DecoderTest() : decoder(nullptr, nullptr) {}

  Decoder decoder;
};

#define CHECK_UINT32V_INLINE(expected, expected_length, ...)            \
  do {                                                                  \
    const byte data[] = {__VA_ARGS__};                                  \
    decoder.Reset(data, data + sizeof(data));                           \
    unsigned length;                                                    \
    EXPECT_EQ(static_cast<uint32_t>(expected),                          \
              decoder.read_u32v<true>(decoder.start(), &length));       \
    EXPECT_EQ(static_cast<unsigned>(expected_length), length);          \
    EXPECT_EQ(data, decoder.pc());                                      \
    EXPECT_TRUE(decoder.ok());                                          \
    EXPECT_EQ(static_cast<uint32_t>(expected), decoder.consume_u32v()); \
    EXPECT_EQ(data + expected_length, decoder.pc());                    \
  } while (false)

#define CHECK_INT32V_INLINE(expected, expected_length, ...)                 \
  do {                                                                      \
    const byte data[] = {__VA_ARGS__};                                      \
    decoder.Reset(data, data + sizeof(data));                               \
    unsigned length;                                                        \
    EXPECT_EQ(expected, decoder.read_i32v<true>(decoder.start(), &length)); \
    EXPECT_EQ(static_cast<unsigned>(expected_length), length);              \
    EXPECT_EQ(data, decoder.pc());                                          \
    EXPECT_TRUE(decoder.ok());                                              \
    EXPECT_EQ(expected, decoder.consume_i32v());                            \
    EXPECT_EQ(data + expected_length, decoder.pc());                        \
  } while (false)

#define CHECK_UINT64V_INLINE(expected, expected_length, ...)       \
  do {                                                             \
    const byte data[] = {__VA_ARGS__};                             \
    decoder.Reset(data, data + sizeof(data));                      \
    unsigned length;                                               \
    EXPECT_EQ(static_cast<uint64_t>(expected),                     \
              decoder.read_u64v<false>(decoder.start(), &length)); \
    EXPECT_EQ(static_cast<unsigned>(expected_length), length);     \
  } while (false)

#define CHECK_INT64V_INLINE(expected, expected_length, ...)                  \
  do {                                                                       \
    const byte data[] = {__VA_ARGS__};                                       \
    decoder.Reset(data, data + sizeof(data));                                \
    unsigned length;                                                         \
    EXPECT_EQ(expected, decoder.read_i64v<false>(decoder.start(), &length)); \
    EXPECT_EQ(static_cast<unsigned>(expected_length), length);               \
  } while (false)

TEST_F(DecoderTest, ReadU32v_OneByte) {
  CHECK_UINT32V_INLINE(0, 1, 0);
  CHECK_UINT32V_INLINE(5, 1, 5);
  CHECK_UINT32V_INLINE(7, 1, 7);
  CHECK_UINT32V_INLINE(9, 1, 9);
  CHECK_UINT32V_INLINE(37, 1, 37);
  CHECK_UINT32V_INLINE(69, 1, 69);
  CHECK_UINT32V_INLINE(110, 1, 110);
  CHECK_UINT32V_INLINE(125, 1, 125);
  CHECK_UINT32V_INLINE(126, 1, 126);
  CHECK_UINT32V_INLINE(127, 1, 127);
}

TEST_F(DecoderTest, ReadU32v_TwoByte) {
  CHECK_UINT32V_INLINE(0, 1, 0, 0);
  CHECK_UINT32V_INLINE(10, 1, 10, 0);
  CHECK_UINT32V_INLINE(27, 1, 27, 0);
  CHECK_UINT32V_INLINE(100, 1, 100, 0);

  CHECK_UINT32V_INLINE(444, 2, U32V_2(444));
  CHECK_UINT32V_INLINE(544, 2, U32V_2(544));
  CHECK_UINT32V_INLINE(1311, 2, U32V_2(1311));
  CHECK_UINT32V_INLINE(2333, 2, U32V_2(2333));

  for (uint32_t i = 0; i < 1 << 14; i = i * 13 + 1) {
    CHECK_UINT32V_INLINE(i, 2, U32V_2(i));
  }

  const uint32_t max = (1 << 14) - 1;
  CHECK_UINT32V_INLINE(max, 2, U32V_2(max));
}

TEST_F(DecoderTest, ReadU32v_ThreeByte) {
  CHECK_UINT32V_INLINE(0, 1, 0, 0, 0, 0);
  CHECK_UINT32V_INLINE(10, 1, 10, 0, 0, 0);
  CHECK_UINT32V_INLINE(27, 1, 27, 0, 0, 0);
  CHECK_UINT32V_INLINE(100, 1, 100, 0, 0, 0);

  CHECK_UINT32V_INLINE(11, 3, U32V_3(11));
  CHECK_UINT32V_INLINE(101, 3, U32V_3(101));
  CHECK_UINT32V_INLINE(446, 3, U32V_3(446));
  CHECK_UINT32V_INLINE(546, 3, U32V_3(546));
  CHECK_UINT32V_INLINE(1319, 3, U32V_3(1319));
  CHECK_UINT32V_INLINE(2338, 3, U32V_3(2338));
  CHECK_UINT32V_INLINE(8191, 3, U32V_3(8191));
  CHECK_UINT32V_INLINE(9999, 3, U32V_3(9999));
  CHECK_UINT32V_INLINE(14444, 3, U32V_3(14444));
  CHECK_UINT32V_INLINE(314444, 3, U32V_3(314444));
  CHECK_UINT32V_INLINE(614444, 3, U32V_3(614444));

  const uint32_t max = (1 << 21) - 1;

  for (uint32_t i = 0; i <= max; i = i * 13 + 3) {
    CHECK_UINT32V_INLINE(i, 3, U32V_3(i), 0);
  }

  CHECK_UINT32V_INLINE(max, 3, U32V_3(max));
}

TEST_F(DecoderTest, ReadU32v_FourByte) {
  CHECK_UINT32V_INLINE(0, 1, 0, 0, 0, 0, 0);
  CHECK_UINT32V_INLINE(10, 1, 10, 0, 0, 0, 0);
  CHECK_UINT32V_INLINE(27, 1, 27, 0, 0, 0, 0);
  CHECK_UINT32V_INLINE(100, 1, 100, 0, 0, 0, 0);

  CHECK_UINT32V_INLINE(13, 4, U32V_4(13));
  CHECK_UINT32V_INLINE(107, 4, U32V_4(107));
  CHECK_UINT32V_INLINE(449, 4, U32V_4(449));
  CHECK_UINT32V_INLINE(541, 4, U32V_4(541));
  CHECK_UINT32V_INLINE(1317, 4, U32V_4(1317));
  CHECK_UINT32V_INLINE(2334, 4, U32V_4(2334));
  CHECK_UINT32V_INLINE(8191, 4, U32V_4(8191));
  CHECK_UINT32V_INLINE(9994, 4, U32V_4(9994));
  CHECK_UINT32V_INLINE(14442, 4, U32V_4(14442));
  CHECK_UINT32V_INLINE(314442, 4, U32V_4(314442));
  CHECK_UINT32V_INLINE(614442, 4, U32V_4(614442));
  CHECK_UINT32V_INLINE(1614442, 4, U32V_4(1614442));
  CHECK_UINT32V_INLINE(5614442, 4, U32V_4(5614442));
  CHECK_UINT32V_INLINE(19614442, 4, U32V_4(19614442));

  const uint32_t max = (1 << 28) - 1;

  for (uint32_t i = 0; i <= max; i = i * 13 + 5) {
    CHECK_UINT32V_INLINE(i, 4, U32V_4(i), 0);
  }

  CHECK_UINT32V_INLINE(max, 4, U32V_4(max));
}

TEST_F(DecoderTest, ReadU32v_FiveByte) {
  CHECK_UINT32V_INLINE(0, 1, 0, 0, 0, 0, 0);
  CHECK_UINT32V_INLINE(10, 1, 10, 0, 0, 0, 0);
  CHECK_UINT32V_INLINE(27, 1, 27, 0, 0, 0, 0);
  CHECK_UINT32V_INLINE(100, 1, 100, 0, 0, 0, 0);

  CHECK_UINT32V_INLINE(13, 5, U32V_5(13));
  CHECK_UINT32V_INLINE(107, 5, U32V_5(107));
  CHECK_UINT32V_INLINE(449, 5, U32V_5(449));
  CHECK_UINT32V_INLINE(541, 5, U32V_5(541));
  CHECK_UINT32V_INLINE(1317, 5, U32V_5(1317));
  CHECK_UINT32V_INLINE(2334, 5, U32V_5(2334));
  CHECK_UINT32V_INLINE(8191, 5, U32V_5(8191));
  CHECK_UINT32V_INLINE(9994, 5, U32V_5(9994));
  CHECK_UINT32V_INLINE(24442, 5, U32V_5(24442));
  CHECK_UINT32V_INLINE(414442, 5, U32V_5(414442));
  CHECK_UINT32V_INLINE(714442, 5, U32V_5(714442));
  CHECK_UINT32V_INLINE(1614442, 5, U32V_5(1614442));
  CHECK_UINT32V_INLINE(6614442, 5, U32V_5(6614442));
  CHECK_UINT32V_INLINE(89614442, 5, U32V_5(89614442));
  CHECK_UINT32V_INLINE(2219614442u, 5, U32V_5(2219614442u));
  CHECK_UINT32V_INLINE(3219614442u, 5, U32V_5(3219614442u));
  CHECK_UINT32V_INLINE(4019614442u, 5, U32V_5(4019614442u));

  const uint32_t max = 0xFFFFFFFFu;

  for (uint32_t i = 1; i < 32; i++) {
    uint32_t val = 0x983489aau << i;
    CHECK_UINT32V_INLINE(val, 5, U32V_5(val), 0);
  }

  CHECK_UINT32V_INLINE(max, 5, U32V_5(max));
}

TEST_F(DecoderTest, ReadU32v_various) {
  for (int i = 0; i < 10; i++) {
    uint32_t x = 0xCCCCCCCCu * i;
    for (int width = 0; width < 32; width++) {
      uint32_t val = x >> width;

      CHECK_UINT32V_INLINE(val & MASK_7, 1, U32V_1(val));
      CHECK_UINT32V_INLINE(val & MASK_14, 2, U32V_2(val));
      CHECK_UINT32V_INLINE(val & MASK_21, 3, U32V_3(val));
      CHECK_UINT32V_INLINE(val & MASK_28, 4, U32V_4(val));
      CHECK_UINT32V_INLINE(val, 5, U32V_5(val));
    }
  }
}

TEST_F(DecoderTest, ReadI32v_OneByte) {
  CHECK_INT32V_INLINE(0, 1, 0);
  CHECK_INT32V_INLINE(4, 1, 4);
  CHECK_INT32V_INLINE(6, 1, 6);
  CHECK_INT32V_INLINE(9, 1, 9);
  CHECK_INT32V_INLINE(33, 1, 33);
  CHECK_INT32V_INLINE(61, 1, 61);
  CHECK_INT32V_INLINE(63, 1, 63);

  CHECK_INT32V_INLINE(-1, 1, 127);
  CHECK_INT32V_INLINE(-2, 1, 126);
  CHECK_INT32V_INLINE(-11, 1, 117);
  CHECK_INT32V_INLINE(-62, 1, 66);
  CHECK_INT32V_INLINE(-63, 1, 65);
  CHECK_INT32V_INLINE(-64, 1, 64);
}

TEST_F(DecoderTest, ReadI32v_TwoByte) {
  CHECK_INT32V_INLINE(0, 2, U32V_2(0));
  CHECK_INT32V_INLINE(9, 2, U32V_2(9));
  CHECK_INT32V_INLINE(61, 2, U32V_2(61));
  CHECK_INT32V_INLINE(63, 2, U32V_2(63));

  CHECK_INT32V_INLINE(-1, 2, U32V_2(-1));
  CHECK_INT32V_INLINE(-2, 2, U32V_2(-2));
  CHECK_INT32V_INLINE(-63, 2, U32V_2(-63));
  CHECK_INT32V_INLINE(-64, 2, U32V_2(-64));

  CHECK_INT32V_INLINE(-200, 2, U32V_2(-200));
  CHECK_INT32V_INLINE(-1002, 2, U32V_2(-1002));
  CHECK_INT32V_INLINE(-2004, 2, U32V_2(-2004));
  CHECK_INT32V_INLINE(-4077, 2, U32V_2(-4077));

  CHECK_INT32V_INLINE(207, 2, U32V_2(207));
  CHECK_INT32V_INLINE(1009, 2, U32V_2(1009));
  CHECK_INT32V_INLINE(2003, 2, U32V_2(2003));
  CHECK_INT32V_INLINE(4072, 2, U32V_2(4072));

  const int32_t min = 0 - (1 << 13);
  for (int i = min; i < min + 10; i++) {
    CHECK_INT32V_INLINE(i, 2, U32V_2(i));
  }

  const int32_t max = (1 << 13) - 1;
  for (int i = max; i > max - 10; i--) {
    CHECK_INT32V_INLINE(i, 2, U32V_2(i));
  }
}

TEST_F(DecoderTest, ReadI32v_ThreeByte) {
  CHECK_INT32V_INLINE(0, 3, U32V_3(0));
  CHECK_INT32V_INLINE(9, 3, U32V_3(9));
  CHECK_INT32V_INLINE(61, 3, U32V_3(61));
  CHECK_INT32V_INLINE(63, 3, U32V_3(63));

  CHECK_INT32V_INLINE(-1, 3, U32V_3(-1));
  CHECK_INT32V_INLINE(-2, 3, U32V_3(-2));
  CHECK_INT32V_INLINE(-63, 3, U32V_3(-63));
  CHECK_INT32V_INLINE(-64, 3, U32V_3(-64));

  CHECK_INT32V_INLINE(-207, 3, U32V_3(-207));
  CHECK_INT32V_INLINE(-1012, 3, U32V_3(-1012));
  CHECK_INT32V_INLINE(-4067, 3, U32V_3(-4067));
  CHECK_INT32V_INLINE(-14067, 3, U32V_3(-14067));
  CHECK_INT32V_INLINE(-234061, 3, U32V_3(-234061));

  CHECK_INT32V_INLINE(237, 3, U32V_3(237));
  CHECK_INT32V_INLINE(1309, 3, U32V_3(1309));
  CHECK_INT32V_INLINE(4372, 3, U32V_3(4372));
  CHECK_INT32V_INLINE(64372, 3, U32V_3(64372));
  CHECK_INT32V_INLINE(374372, 3, U32V_3(374372));

  const int32_t min = 0 - (1 << 20);
  for (int i = min; i < min + 10; i++) {
    CHECK_INT32V_INLINE(i, 3, U32V_3(i));
  }

  const int32_t max = (1 << 20) - 1;
  for (int i = max; i > max - 10; i--) {
    CHECK_INT32V_INLINE(i, 3, U32V_3(i));
  }
}

TEST_F(DecoderTest, ReadI32v_FourByte) {
  CHECK_INT32V_INLINE(0, 4, U32V_4(0));
  CHECK_INT32V_INLINE(9, 4, U32V_4(9));
  CHECK_INT32V_INLINE(61, 4, U32V_4(61));
  CHECK_INT32V_INLINE(63, 4, U32V_4(63));

  CHECK_INT32V_INLINE(-1, 4, U32V_4(-1));
  CHECK_INT32V_INLINE(-2, 4, U32V_4(-2));
  CHECK_INT32V_INLINE(-63, 4, U32V_4(-63));
  CHECK_INT32V_INLINE(-64, 4, U32V_4(-64));

  CHECK_INT32V_INLINE(-267, 4, U32V_4(-267));
  CHECK_INT32V_INLINE(-1612, 4, U32V_4(-1612));
  CHECK_INT32V_INLINE(-4667, 4, U32V_4(-4667));
  CHECK_INT32V_INLINE(-16067, 4, U32V_4(-16067));
  CHECK_INT32V_INLINE(-264061, 4, U32V_4(-264061));
  CHECK_INT32V_INLINE(-1264061, 4, U32V_4(-1264061));
  CHECK_INT32V_INLINE(-6264061, 4, U32V_4(-6264061));
  CHECK_INT32V_INLINE(-8264061, 4, U32V_4(-8264061));

  CHECK_INT32V_INLINE(277, 4, U32V_4(277));
  CHECK_INT32V_INLINE(1709, 4, U32V_4(1709));
  CHECK_INT32V_INLINE(4772, 4, U32V_4(4772));
  CHECK_INT32V_INLINE(67372, 4, U32V_4(67372));
  CHECK_INT32V_INLINE(374372, 4, U32V_4(374372));
  CHECK_INT32V_INLINE(2374372, 4, U32V_4(2374372));
  CHECK_INT32V_INLINE(7374372, 4, U32V_4(7374372));
  CHECK_INT32V_INLINE(9374372, 4, U32V_4(9374372));

  const int32_t min = 0 - (1 << 27);
  for (int i = min; i < min + 10; i++) {
    CHECK_INT32V_INLINE(i, 4, U32V_4(i));
  }

  const int32_t max = (1 << 27) - 1;
  for (int i = max; i > max - 10; i--) {
    CHECK_INT32V_INLINE(i, 4, U32V_4(i));
  }
}

TEST_F(DecoderTest, ReadI32v_FiveByte) {
  CHECK_INT32V_INLINE(0, 5, U32V_5(0));
  CHECK_INT32V_INLINE(16, 5, U32V_5(16));
  CHECK_INT32V_INLINE(94, 5, U32V_5(94));
  CHECK_INT32V_INLINE(127, 5, U32V_5(127));

  CHECK_INT32V_INLINE(-1, 5, U32V_5(-1));
  CHECK_INT32V_INLINE(-2, 5, U32V_5(-2));
  CHECK_INT32V_INLINE(-63, 5, U32V_5(-63));
  CHECK_INT32V_INLINE(-64, 5, U32V_5(-64));

  CHECK_INT32V_INLINE(-257, 5, U32V_5(-257));
  CHECK_INT32V_INLINE(-1512, 5, U32V_5(-1512));
  CHECK_INT32V_INLINE(-4567, 5, U32V_5(-4567));
  CHECK_INT32V_INLINE(-15067, 5, U32V_5(-15067));
  CHECK_INT32V_INLINE(-254061, 5, U32V_5(-254061));
  CHECK_INT32V_INLINE(-1364061, 5, U32V_5(-1364061));
  CHECK_INT32V_INLINE(-6364061, 5, U32V_5(-6364061));
  CHECK_INT32V_INLINE(-8364061, 5, U32V_5(-8364061));
  CHECK_INT32V_INLINE(-28364061, 5, U32V_5(-28364061));
  CHECK_INT32V_INLINE(-228364061, 5, U32V_5(-228364061));

  CHECK_INT32V_INLINE(227, 5, U32V_5(227));
  CHECK_INT32V_INLINE(1209, 5, U32V_5(1209));
  CHECK_INT32V_INLINE(4272, 5, U32V_5(4272));
  CHECK_INT32V_INLINE(62372, 5, U32V_5(62372));
  CHECK_INT32V_INLINE(324372, 5, U32V_5(324372));
  CHECK_INT32V_INLINE(2274372, 5, U32V_5(2274372));
  CHECK_INT32V_INLINE(7274372, 5, U32V_5(7274372));
  CHECK_INT32V_INLINE(9274372, 5, U32V_5(9274372));
  CHECK_INT32V_INLINE(42374372, 5, U32V_5(42374372));
  CHECK_INT32V_INLINE(429374372, 5, U32V_5(429374372));

  const int32_t min = kMinInt;
  for (int i = min; i < min + 10; i++) {
    CHECK_INT32V_INLINE(i, 5, U32V_5(i));
  }

  const int32_t max = kMaxInt;
  for (int i = max; i > max - 10; i--) {
    CHECK_INT32V_INLINE(i, 5, U32V_5(i));
  }
}

TEST_F(DecoderTest, ReadU32v_off_end1) {
  static const byte data[] = {U32V_1(11)};
  unsigned length = 0;
  decoder.Reset(data, data);
  decoder.read_u32v<true>(decoder.start(), &length);
  EXPECT_EQ(0u, length);
  EXPECT_FALSE(decoder.ok());
}

TEST_F(DecoderTest, ReadU32v_off_end2) {
  static const byte data[] = {U32V_2(1111)};
  for (size_t i = 0; i < sizeof(data); i++) {
    unsigned length = 0;
    decoder.Reset(data, data + i);
    decoder.read_u32v<true>(decoder.start(), &length);
    EXPECT_EQ(i, length);
    EXPECT_FALSE(decoder.ok());
  }
}

TEST_F(DecoderTest, ReadU32v_off_end3) {
  static const byte data[] = {U32V_3(111111)};
  for (size_t i = 0; i < sizeof(data); i++) {
    unsigned length = 0;
    decoder.Reset(data, data + i);
    decoder.read_u32v<true>(decoder.start(), &length);
    EXPECT_EQ(i, length);
    EXPECT_FALSE(decoder.ok());
  }
}

TEST_F(DecoderTest, ReadU32v_off_end4) {
  static const byte data[] = {U32V_4(11111111)};
  for (size_t i = 0; i < sizeof(data); i++) {
    unsigned length = 0;
    decoder.Reset(data, data + i);
    decoder.read_u32v<true>(decoder.start(), &length);
    EXPECT_EQ(i, length);
    EXPECT_FALSE(decoder.ok());
  }
}

TEST_F(DecoderTest, ReadU32v_off_end5) {
  static const byte data[] = {U32V_5(111111111)};
  for (size_t i = 0; i < sizeof(data); i++) {
    unsigned length = 0;
    decoder.Reset(data, data + i);
    decoder.read_u32v<true>(decoder.start(), &length);
    EXPECT_EQ(i, length);
    EXPECT_FALSE(decoder.ok());
  }
}

TEST_F(DecoderTest, ReadU32v_extra_bits) {
  byte data[] = {0x80, 0x80, 0x80, 0x80, 0x00};
  for (int i = 1; i < 16; i++) {
    data[4] = static_cast<byte>(i << 4);
    unsigned length = 0;
    decoder.Reset(data, data + sizeof(data));
    decoder.read_u32v<true>(decoder.start(), &length);
    EXPECT_EQ(5u, length);
    EXPECT_FALSE(decoder.ok());
  }
}

TEST_F(DecoderTest, ReadI32v_extra_bits_negative) {
  // OK for negative signed values to have extra ones.
  unsigned length = 0;
  byte data[] = {0xff, 0xff, 0xff, 0xff, 0x7f};
  decoder.Reset(data, data + sizeof(data));
  decoder.read_i32v<true>(decoder.start(), &length);
  EXPECT_EQ(5u, length);
  EXPECT_TRUE(decoder.ok());
}

TEST_F(DecoderTest, ReadI32v_extra_bits_positive) {
  // Not OK for positive signed values to have extra ones.
  unsigned length = 0;
  byte data[] = {0x80, 0x80, 0x80, 0x80, 0x77};
  decoder.Reset(data, data + sizeof(data));
  decoder.read_i32v<true>(decoder.start(), &length);
  EXPECT_EQ(5u, length);
  EXPECT_FALSE(decoder.ok());
}

TEST_F(DecoderTest, ReadU32v_Bits) {
  // A more exhaustive test.
  const int kMaxSize = 5;
  const uint32_t kVals[] = {
      0xaabbccdd, 0x11223344, 0x33445566, 0xffeeddcc, 0xF0F0F0F0, 0x0F0F0F0F,
      0xEEEEEEEE, 0xAAAAAAAA, 0x12345678, 0x9abcdef0, 0x80309488, 0x729ed997,
      0xc4a0cf81, 0x16c6eb85, 0x4206db8e, 0xf3b089d5, 0xaa2e223e, 0xf99e29c8,
      0x4a4357d8, 0x1890b1c1, 0x8d80a085, 0xacb6ae4c, 0x1b827e10, 0xeb5c7bd9,
      0xbb1bc146, 0xdf57a33l};
  byte data[kMaxSize];

  // foreach value in above array
  for (size_t v = 0; v < arraysize(kVals); v++) {
    // foreach length 1...32
    for (int i = 1; i <= 32; i++) {
      uint32_t val = kVals[v];
      if (i < 32) val &= ((1 << i) - 1);

      unsigned length = 1 + i / 7;
      for (unsigned j = 0; j < kMaxSize; j++) {
        data[j] = static_cast<byte>((val >> (7 * j)) & MASK_7);
      }
      for (unsigned j = 0; j < length - 1; j++) {
        data[j] |= 0x80;
      }

      // foreach buffer size 0...5
      for (unsigned limit = 0; limit <= kMaxSize; limit++) {
        decoder.Reset(data, data + limit);
        unsigned rlen;
        uint32_t result = decoder.read_u32v<true>(data, &rlen);
        if (limit < length) {
          EXPECT_FALSE(decoder.ok());
        } else {
          EXPECT_TRUE(decoder.ok());
          EXPECT_EQ(val, result);
          EXPECT_EQ(length, rlen);
        }
      }
    }
  }
}

TEST_F(DecoderTest, ReadU64v_OneByte) {
  CHECK_UINT64V_INLINE(0, 1, 0);
  CHECK_UINT64V_INLINE(6, 1, 6);
  CHECK_UINT64V_INLINE(8, 1, 8);
  CHECK_UINT64V_INLINE(12, 1, 12);
  CHECK_UINT64V_INLINE(33, 1, 33);
  CHECK_UINT64V_INLINE(59, 1, 59);
  CHECK_UINT64V_INLINE(110, 1, 110);
  CHECK_UINT64V_INLINE(125, 1, 125);
  CHECK_UINT64V_INLINE(126, 1, 126);
  CHECK_UINT64V_INLINE(127, 1, 127);
}

TEST_F(DecoderTest, ReadI64v_OneByte) {
  CHECK_INT64V_INLINE(0, 1, 0);
  CHECK_INT64V_INLINE(4, 1, 4);
  CHECK_INT64V_INLINE(6, 1, 6);
  CHECK_INT64V_INLINE(9, 1, 9);
  CHECK_INT64V_INLINE(33, 1, 33);
  CHECK_INT64V_INLINE(61, 1, 61);
  CHECK_INT64V_INLINE(63, 1, 63);

  CHECK_INT64V_INLINE(-1, 1, 127);
  CHECK_INT64V_INLINE(-2, 1, 126);
  CHECK_INT64V_INLINE(-11, 1, 117);
  CHECK_INT64V_INLINE(-62, 1, 66);
  CHECK_INT64V_INLINE(-63, 1, 65);
  CHECK_INT64V_INLINE(-64, 1, 64);
}

TEST_F(DecoderTest, ReadU64v_PowerOf2) {
  const int kMaxSize = 10;
  byte data[kMaxSize];

  for (unsigned i = 0; i < 64; i++) {
    const uint64_t val = 1ull << i;
    unsigned index = i / 7;
    data[index] = 1 << (i % 7);
    memset(data, 0x80, index);

    for (unsigned limit = 0; limit <= kMaxSize; limit++) {
      decoder.Reset(data, data + limit);
      unsigned length;
      uint64_t result = decoder.read_u64v<true>(data, &length);
      if (limit <= index) {
        EXPECT_FALSE(decoder.ok());
      } else {
        EXPECT_TRUE(decoder.ok());
        EXPECT_EQ(val, result);
        EXPECT_EQ(index + 1, length);
      }
    }
  }
}

TEST_F(DecoderTest, ReadU64v_Bits) {
  const int kMaxSize = 10;
  const uint64_t kVals[] = {
      0xaabbccdd11223344ull, 0x33445566ffeeddccull, 0xF0F0F0F0F0F0F0F0ull,
      0x0F0F0F0F0F0F0F0Full, 0xEEEEEEEEEEEEEEEEull, 0xAAAAAAAAAAAAAAAAull,
      0x123456789abcdef0ull, 0x80309488729ed997ull, 0xc4a0cf8116c6eb85ull,
      0x4206db8ef3b089d5ull, 0xaa2e223ef99e29c8ull, 0x4a4357d81890b1c1ull,
      0x8d80a085acb6ae4cull, 0x1b827e10eb5c7bd9ull, 0xbb1bc146df57a338ull};
  byte data[kMaxSize];

  // foreach value in above array
  for (size_t v = 0; v < arraysize(kVals); v++) {
    // foreach length 1...64
    for (int i = 1; i <= 64; i++) {
      uint64_t val = kVals[v];
      if (i < 64) val &= ((1ull << i) - 1);

      unsigned length = 1 + i / 7;
      for (unsigned j = 0; j < kMaxSize; j++) {
        data[j] = static_cast<byte>((val >> (7 * j)) & MASK_7);
      }
      for (unsigned j = 0; j < length - 1; j++) {
        data[j] |= 0x80;
      }

      // foreach buffer size 0...10
      for (unsigned limit = 0; limit <= kMaxSize; limit++) {
        decoder.Reset(data, data + limit);
        unsigned rlen;
        uint64_t result = decoder.read_u64v<true>(data, &rlen);
        if (limit < length) {
          EXPECT_FALSE(decoder.ok());
        } else {
          EXPECT_TRUE(decoder.ok());
          EXPECT_EQ(val, result);
          EXPECT_EQ(length, rlen);
        }
      }
    }
  }
}

TEST_F(DecoderTest, ReadI64v_Bits) {
  const int kMaxSize = 10;
  // Exhaustive signedness test.
  const uint64_t kVals[] = {
      0xaabbccdd11223344ull, 0x33445566ffeeddccull, 0xF0F0F0F0F0F0F0F0ull,
      0x0F0F0F0F0F0F0F0Full, 0xEEEEEEEEEEEEEEEEull, 0xAAAAAAAAAAAAAAAAull,
      0x123456789abcdef0ull, 0x80309488729ed997ull, 0xc4a0cf8116c6eb85ull,
      0x4206db8ef3b089d5ull, 0xaa2e223ef99e29c8ull, 0x4a4357d81890b1c1ull,
      0x8d80a085acb6ae4cull, 0x1b827e10eb5c7bd9ull, 0xbb1bc146df57a338ull};
  byte data[kMaxSize];

  // foreach value in above array
  for (size_t v = 0; v < arraysize(kVals); v++) {
    // foreach length 1...64
    for (int i = 1; i <= 64; i++) {
      const int64_t val = bit_cast<int64_t>(kVals[v] << (64 - i)) >> (64 - i);

      unsigned length = 1 + i / 7;
      for (unsigned j = 0; j < kMaxSize; j++) {
        data[j] = static_cast<byte>((val >> (7 * j)) & MASK_7);
      }
      for (unsigned j = 0; j < length - 1; j++) {
        data[j] |= 0x80;
      }

      // foreach buffer size 0...10
      for (unsigned limit = 0; limit <= kMaxSize; limit++) {
        decoder.Reset(data, data + limit);
        unsigned rlen;
        int64_t result = decoder.read_i64v<true>(data, &rlen);
        if (limit < length) {
          EXPECT_FALSE(decoder.ok());
        } else {
          EXPECT_TRUE(decoder.ok());
          EXPECT_EQ(val, result);
          EXPECT_EQ(length, rlen);
        }
      }
    }
  }
}

TEST_F(DecoderTest, ReadU64v_extra_bits) {
  byte data[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
  for (int i = 1; i < 128; i++) {
    data[9] = static_cast<byte>(i << 1);
    unsigned length = 0;
    decoder.Reset(data, data + sizeof(data));
    decoder.read_u64v<true>(decoder.start(), &length);
    EXPECT_EQ(10u, length);
    EXPECT_FALSE(decoder.ok());
  }
}

TEST_F(DecoderTest, ReadI64v_extra_bits_negative) {
  // OK for negative signed values to have extra ones.
  unsigned length = 0;
  byte data[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};
  decoder.Reset(data, data + sizeof(data));
  decoder.read_i64v<true>(decoder.start(), &length);
  EXPECT_EQ(10u, length);
  EXPECT_TRUE(decoder.ok());
}

TEST_F(DecoderTest, ReadI64v_extra_bits_positive) {
  // Not OK for positive signed values to have extra ones.
  unsigned length = 0;
  byte data[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x77};
  decoder.Reset(data, data + sizeof(data));
  decoder.read_i64v<true>(decoder.start(), &length);
  EXPECT_EQ(10u, length);
  EXPECT_FALSE(decoder.ok());
}

TEST_F(DecoderTest, FailOnNullData) {
  decoder.Reset(nullptr, 0);
  decoder.checkAvailable(1);
  EXPECT_FALSE(decoder.ok());
  EXPECT_FALSE(decoder.toResult(nullptr).ok());
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

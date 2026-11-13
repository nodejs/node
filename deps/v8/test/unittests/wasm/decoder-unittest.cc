// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/decoder.h"

#include "src/base/overflowing-math.h"
#include "src/objects/objects-inl.h"
#include "test/common/flag-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock-support.h"

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
    const uint8_t data[] = {__VA_ARGS__};                               \
    decoder.Reset(data, data + sizeof(data));                           \
    auto [value, length] =                                              \
        decoder.read_u32v<Decoder::FullValidationTag>(decoder.start()); \
    EXPECT_EQ(static_cast<uint32_t>(expected), value);                  \
    EXPECT_EQ(static_cast<unsigned>(expected_length), length);          \
    EXPECT_EQ(data, decoder.pc());                                      \
    EXPECT_TRUE(decoder.ok());                                          \
    EXPECT_EQ(static_cast<uint32_t>(expected), decoder.consume_u32v()); \
    EXPECT_EQ(data + expected_length, decoder.pc());                    \
  } while (false)

#define CHECK_INT32V_INLINE(expected, expected_length, ...)             \
  do {                                                                  \
    const uint8_t data[] = {__VA_ARGS__};                               \
    decoder.Reset(data, data + sizeof(data));                           \
    auto [value, length] =                                              \
        decoder.read_i32v<Decoder::FullValidationTag>(decoder.start()); \
    EXPECT_EQ(expected, value);                                         \
    EXPECT_EQ(static_cast<unsigned>(expected_length), length);          \
    EXPECT_EQ(data, decoder.pc());                                      \
    EXPECT_TRUE(decoder.ok());                                          \
    EXPECT_EQ(expected, decoder.consume_i32v());                        \
    EXPECT_EQ(data + expected_length, decoder.pc());                    \
  } while (false)

#define CHECK_UINT64V_INLINE(expected, expected_length, ...)            \
  do {                                                                  \
    const uint8_t data[] = {__VA_ARGS__};                               \
    decoder.Reset(data, data + sizeof(data));                           \
    auto [value, length] =                                              \
        decoder.read_u64v<Decoder::FullValidationTag>(decoder.start()); \
    EXPECT_EQ(static_cast<uint64_t>(expected), value);                  \
    EXPECT_EQ(static_cast<unsigned>(expected_length), length);          \
  } while (false)

#define CHECK_INT64V_INLINE(expected, expected_length, ...)             \
  do {                                                                  \
    const uint8_t data[] = {__VA_ARGS__};                               \
    decoder.Reset(data, data + sizeof(data));                           \
    auto [value, length] =                                              \
        decoder.read_i64v<Decoder::FullValidationTag>(decoder.start()); \
    EXPECT_EQ(expected, value);                                         \
    EXPECT_EQ(static_cast<unsigned>(expected_length), length);          \
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
    uint32_t val = 0x983489AAu << i;
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
  static const uint8_t data[] = {U32V_1(11)};
  decoder.Reset(data, data);
  decoder.read_u32v<Decoder::FullValidationTag>(decoder.start());
  EXPECT_FALSE(decoder.ok());
}

TEST_F(DecoderTest, ReadU32v_off_end2) {
  static const uint8_t data[] = {U32V_2(1111)};
  for (size_t i = 0; i < sizeof(data); i++) {
    decoder.Reset(data, data + i);
    decoder.read_u32v<Decoder::FullValidationTag>(decoder.start());
    EXPECT_FALSE(decoder.ok());
  }
}

TEST_F(DecoderTest, ReadU32v_off_end3) {
  static const uint8_t data[] = {U32V_3(111111)};
  for (size_t i = 0; i < sizeof(data); i++) {
    decoder.Reset(data, data + i);
    decoder.read_u32v<Decoder::FullValidationTag>(decoder.start());
    EXPECT_FALSE(decoder.ok());
  }
}

TEST_F(DecoderTest, ReadU32v_off_end4) {
  static const uint8_t data[] = {U32V_4(11111111)};
  for (size_t i = 0; i < sizeof(data); i++) {
    decoder.Reset(data, data + i);
    decoder.read_u32v<Decoder::FullValidationTag>(decoder.start());
    EXPECT_FALSE(decoder.ok());
  }
}

TEST_F(DecoderTest, ReadU32v_off_end5) {
  static const uint8_t data[] = {U32V_5(111111111)};
  for (size_t i = 0; i < sizeof(data); i++) {
    decoder.Reset(data, data + i);
    decoder.read_u32v<Decoder::FullValidationTag>(decoder.start());
    EXPECT_FALSE(decoder.ok());
  }
}

TEST_F(DecoderTest, ReadU32v_extra_bits) {
  uint8_t data[] = {0x80, 0x80, 0x80, 0x80, 0x00};
  for (int i = 1; i < 16; i++) {
    data[4] = static_cast<uint8_t>(i << 4);
    decoder.Reset(data, data + sizeof(data));
    decoder.read_u32v<Decoder::FullValidationTag>(decoder.start());
    EXPECT_FALSE(decoder.ok());
  }
}

TEST_F(DecoderTest, ReadI32v_extra_bits_negative) {
  // OK for negative signed values to have extra ones.
  uint8_t data[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x7F};
  decoder.Reset(data, data + sizeof(data));
  auto [result, length] =
      decoder.read_i32v<Decoder::FullValidationTag>(decoder.start());
  EXPECT_EQ(5u, length);
  EXPECT_TRUE(decoder.ok());
}

TEST_F(DecoderTest, ReadI32v_extra_bits_positive) {
  // Not OK for positive signed values to have extra ones.
  uint8_t data[] = {0x80, 0x80, 0x80, 0x80, 0x77};
  decoder.Reset(data, data + sizeof(data));
  decoder.read_i32v<Decoder::FullValidationTag>(decoder.start());
  EXPECT_FALSE(decoder.ok());
}

TEST_F(DecoderTest, ReadU32v_Bits) {
  // A more exhaustive test.
  const int kMaxSize = 5;
  const uint32_t kVals[] = {
      0xAABBCCDD, 0x11223344, 0x33445566, 0xFFEEDDCC, 0xF0F0F0F0, 0x0F0F0F0F,
      0xEEEEEEEE, 0xAAAAAAAA, 0x12345678, 0x9ABCDEF0, 0x80309488, 0x729ED997,
      0xC4A0CF81, 0x16C6EB85, 0x4206DB8E, 0xF3B089D5, 0xAA2E223E, 0xF99E29C8,
      0x4A4357D8, 0x1890B1C1, 0x8D80A085, 0xACB6AE4C, 0x1B827E10, 0xEB5C7BD9,
      0xBB1BC146, 0xDF57A33l};
  uint8_t data[kMaxSize];

  // foreach value in above array
  for (size_t v = 0; v < arraysize(kVals); v++) {
    // foreach length 1...32
    for (int i = 1; i <= 32; i++) {
      uint32_t val = kVals[v];
      if (i < 32) {
        val &= base::SubWithWraparound(base::ShlWithWraparound(1, i), 1);
      }

      unsigned length = 1 + i / 7;
      for (unsigned j = 0; j < kMaxSize; j++) {
        data[j] = static_cast<uint8_t>((val >> (7 * j)) & MASK_7);
      }
      for (unsigned j = 0; j < length - 1; j++) {
        data[j] |= 0x80;
      }

      // foreach buffer size 0...5
      for (unsigned limit = 0; limit <= kMaxSize; limit++) {
        decoder.Reset(data, data + limit);
        auto [result, rlen] =
            decoder.read_u32v<Decoder::FullValidationTag>(data);
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
  uint8_t data[kMaxSize] = {};

  for (unsigned i = 0; i < 64; i++) {
    const uint64_t val = 1ull << i;
    unsigned index = i / 7;
    data[index] = 1 << (i % 7);
    memset(data, 0x80, index);

    for (unsigned limit = 0; limit <= kMaxSize; limit++) {
      decoder.Reset(data, data + limit);
      auto [result, length] =
          decoder.read_u64v<Decoder::FullValidationTag>(data);
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
      0xAABBCCDD11223344ull, 0x33445566FFEEDDCCull, 0xF0F0F0F0F0F0F0F0ull,
      0x0F0F0F0F0F0F0F0Full, 0xEEEEEEEEEEEEEEEEull, 0xAAAAAAAAAAAAAAAAull,
      0x123456789ABCDEF0ull, 0x80309488729ED997ull, 0xC4A0CF8116C6EB85ull,
      0x4206DB8EF3B089D5ull, 0xAA2E223EF99E29C8ull, 0x4A4357D81890B1C1ull,
      0x8D80A085ACB6AE4Cull, 0x1B827E10EB5C7BD9ull, 0xBB1BC146DF57A338ull};
  uint8_t data[kMaxSize];

  // foreach value in above array
  for (size_t v = 0; v < arraysize(kVals); v++) {
    // foreach length 1...64
    for (int i = 1; i <= 64; i++) {
      uint64_t val = kVals[v];
      if (i < 64) val &= ((1ull << i) - 1);

      unsigned length = 1 + i / 7;
      for (unsigned j = 0; j < kMaxSize; j++) {
        data[j] = static_cast<uint8_t>((val >> (7 * j)) & MASK_7);
      }
      for (unsigned j = 0; j < length - 1; j++) {
        data[j] |= 0x80;
      }

      // foreach buffer size 0...10
      for (unsigned limit = 0; limit <= kMaxSize; limit++) {
        decoder.Reset(data, data + limit);
        auto [result, rlen] =
            decoder.read_u64v<Decoder::FullValidationTag>(data);
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
      0xAABBCCDD11223344ull, 0x33445566FFEEDDCCull, 0xF0F0F0F0F0F0F0F0ull,
      0x0F0F0F0F0F0F0F0Full, 0xEEEEEEEEEEEEEEEEull, 0xAAAAAAAAAAAAAAAAull,
      0x123456789ABCDEF0ull, 0x80309488729ED997ull, 0xC4A0CF8116C6EB85ull,
      0x4206DB8EF3B089D5ull, 0xAA2E223EF99E29C8ull, 0x4A4357D81890B1C1ull,
      0x8D80A085ACB6AE4Cull, 0x1B827E10EB5C7BD9ull, 0xBB1BC146DF57A338ull};
  uint8_t data[kMaxSize];

  // foreach value in above array
  for (size_t v = 0; v < arraysize(kVals); v++) {
    // foreach length 1...64
    for (int i = 1; i <= 64; i++) {
      const int64_t val =
          base::bit_cast<int64_t>(kVals[v] << (64 - i)) >> (64 - i);

      unsigned length = 1 + i / 7;
      for (unsigned j = 0; j < kMaxSize; j++) {
        data[j] = static_cast<uint8_t>((val >> (7 * j)) & MASK_7);
      }
      for (unsigned j = 0; j < length - 1; j++) {
        data[j] |= 0x80;
      }

      // foreach buffer size 0...10
      for (unsigned limit = 0; limit <= kMaxSize; limit++) {
        decoder.Reset(data, data + limit);
        auto [result, rlen] =
            decoder.read_i64v<Decoder::FullValidationTag>(data);
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
  uint8_t data[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
  for (int i = 1; i < 128; i++) {
    data[9] = static_cast<uint8_t>(i << 1);
    decoder.Reset(data, data + sizeof(data));
    decoder.read_u64v<Decoder::FullValidationTag>(decoder.start());
    EXPECT_FALSE(decoder.ok());
  }
}

TEST_F(DecoderTest, ReadI64v_extra_bits_negative) {
  // OK for negative signed values to have extra ones.
  uint8_t data[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F};
  decoder.Reset(data, data + sizeof(data));
  auto [result, length] =
      decoder.read_i64v<Decoder::FullValidationTag>(decoder.start());
  EXPECT_EQ(10u, length);
  EXPECT_TRUE(decoder.ok());
}

TEST_F(DecoderTest, ReadI64v_extra_bits_positive) {
  // Not OK for positive signed values to have extra ones.
  uint8_t data[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x77};
  decoder.Reset(data, data + sizeof(data));
  decoder.read_i64v<Decoder::FullValidationTag>(decoder.start());
  EXPECT_FALSE(decoder.ok());
}

TEST_F(DecoderTest, FailOnNullData) {
  decoder.Reset(nullptr, nullptr);
  decoder.checkAvailable(1);
  EXPECT_FALSE(decoder.ok());
  EXPECT_FALSE(decoder.toResult(nullptr).ok());
}

// Exercises all LEB128 fast-path and fallback boundaries:
// - exact-sized heap buffers crossing 2-byte, 4-byte, and 8-byte SWAR
//   thresholds (so ASan catches any out-of-bounds read past {end_});
// - trailing bytes (0x00, 0x7f, 0x80, 0xff) inside the wider SWAR load right
//   after the terminating byte;
// - minimal and overlong encodings across extrema and bit-boundary widths for
//   u32v, i32v, i33v, u64v, and i64v under both FullValidationTag and
//   NoValidationTag.
TEST_F(DecoderTest, ReadLEB_SwarAndFallbackPaths) {
  constexpr uint8_t kPadBytes[] = {0x00, 0x7f, 0x80, 0xff};
  constexpr uint32_t kBufferLimits32[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 16};
  constexpr uint32_t kBufferLimits64[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16};
  // Covers 0, all-ones (-1 / UINT*_MAX), signed max (0x7fff...), signed min
  // (0x8000...), and two complementary mixed bit patterns when right-shifted by
  // (64 - bits).
  constexpr uint64_t kVals[] = {0x0000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL,
                                0x7FFFFFFFFFFFFFFFULL, 0x8000000000000000ULL,
                                0xAABBCCDD11223344ULL, 0x55443322EEDDCCBBULL};
  // Covers bit 1, bit 64, and every 7-bit LEB byte boundary (7*k and 7*k + 1,
  // including bit 56 at the top of the 8-byte SWAR load).
  constexpr int kBitWidths64[] = {1,  7,  8,  14, 15, 21, 22, 28, 29, 35,
                                  36, 42, 43, 49, 50, 56, 57, 63, 64};

  std::array<std::unique_ptr<uint8_t[]>, 17> buffers;
  for (uint32_t limit : kBufferLimits64) {
    buffers[limit] = std::make_unique<uint8_t[]>(limit);
  }

  auto check_leb = [&](auto expected, uint32_t length, uint32_t limit,
                       uint8_t pad, auto read_validated,
                       auto read_unvalidated) {
    uint8_t* data = buffers[limit].get();
    uint32_t write_len = std::min(limit, length);
    if (limit > write_len) {
      memset(data + write_len, pad, limit - write_len);
    }
    for (uint32_t j = 0; j < write_len; j++) {
      data[j] = static_cast<uint8_t>((expected >> (7 * j)) & MASK_7);
      if (j + 1 < length) data[j] |= 0x80;
    }
    decoder.Reset(data, data + limit);
    auto [res, len] = read_validated(data);
    if (limit < length) {
      EXPECT_FALSE(decoder.ok());
      EXPECT_EQ(limit, decoder.error().offset());
      EXPECT_EQ(decltype(expected){0}, res);
      EXPECT_EQ(0u, len);
    } else {
      EXPECT_TRUE(decoder.ok());
      EXPECT_EQ(expected, res);
      EXPECT_EQ(length, len);
      auto [res_noval, len_noval] = read_unvalidated(data);
      EXPECT_EQ(expected, res_noval);
      EXPECT_EQ(length, len_noval);
    }
  };

  auto read_u32_val = [&](const uint8_t* p) {
    return decoder.read_u32v<Decoder::FullValidationTag>(p);
  };
  auto read_u32_noval = [&](const uint8_t* p) {
    return decoder.read_u32v<Decoder::NoValidationTag>(p);
  };
  auto read_i32_val = [&](const uint8_t* p) {
    return decoder.read_i32v<Decoder::FullValidationTag>(p);
  };
  auto read_i32_noval = [&](const uint8_t* p) {
    return decoder.read_i32v<Decoder::NoValidationTag>(p);
  };
  auto read_i33_val = [&](const uint8_t* p) {
    return decoder.read_i33v<Decoder::FullValidationTag>(p);
  };
  auto read_i33_noval = [&](const uint8_t* p) {
    return decoder.read_i33v<Decoder::NoValidationTag>(p);
  };
  auto read_u64_val = [&](const uint8_t* p) {
    return decoder.read_u64v<Decoder::FullValidationTag>(p);
  };
  auto read_u64_noval = [&](const uint8_t* p) {
    return decoder.read_u64v<Decoder::NoValidationTag>(p);
  };
  auto read_i64_val = [&](const uint8_t* p) {
    return decoder.read_i64v<Decoder::FullValidationTag>(p);
  };
  auto read_i64_noval = [&](const uint8_t* p) {
    return decoder.read_i64v<Decoder::NoValidationTag>(p);
  };

  for (uint8_t pad : kPadBytes) {
    for (uint64_t raw_val : kVals) {
      // Test u32v and i32v across 1..5 byte encodings (including overlong) and
      // buffer limits crossing the 2-byte and 8-byte fast-path thresholds.
      for (int bits = 1; bits <= 32; bits++) {
        uint32_t uval = static_cast<uint32_t>(raw_val >> (64 - bits));
        int32_t ival = static_cast<int32_t>(base::bit_cast<int64_t>(raw_val) >>
                                            (64 - bits));
        for (uint32_t length = 1 + (bits - 1) / 7; length <= 5; length++) {
          for (uint32_t limit : kBufferLimits32) {
            if (limit <= length && pad != kPadBytes[0]) continue;
            check_leb(uval, length, limit, pad, read_u32_val, read_u32_noval);
            check_leb(ival, length, limit, pad, read_i32_val, read_i32_noval);
          }
        }
      }

      // Test i33v across 1..5 byte encodings and buffer limits.
      for (int bits = 1; bits <= 33; bits++) {
        int64_t ival33 = base::bit_cast<int64_t>(raw_val) >> (64 - bits);
        for (uint32_t length = 1 + (bits - 1) / 7; length <= 5; length++) {
          for (uint32_t limit : kBufferLimits32) {
            if (limit <= length && pad != kPadBytes[0]) continue;
            check_leb(ival33, length, limit, pad, read_i33_val, read_i33_noval);
          }
        }
      }

      // Test u64v and i64v across 1..10 byte encodings (including <= 4 byte
      // u64v SWAR path, 5..8 byte SWAR path, and 9..10 byte fallback).
      for (int bits : kBitWidths64) {
        uint64_t uval64 = raw_val >> (64 - bits);
        int64_t ival64 = base::bit_cast<int64_t>(raw_val) >> (64 - bits);
        for (uint32_t length = 1 + (bits - 1) / 7; length <= 10; length++) {
          for (uint32_t limit : kBufferLimits64) {
            if (limit <= length && pad != kPadBytes[0]) continue;
            check_leb(uval64, length, limit, pad, read_u64_val, read_u64_noval);
            check_leb(ival64, length, limit, pad, read_i64_val, read_i64_noval);
          }
        }
      }
    }
  }
}

// Exhaustively checks all 256 possible final-byte values at maximum LEB length
// (5th byte for 32/33-bit, 10th byte for 64-bit) across both the < 8-byte
// fallback path (buf_size == 5) and the >= 8-byte SWAR path (buf_size >= 8),
// verifying decoded values, NoValidationTag parity, and error messages.
TEST_F(DecoderTest, ReadLEB_ExhaustiveLastByteValidation) {
  constexpr uint8_t kPadBytes[] = {0x00, 0x7f, 0x80, 0xff};
  constexpr size_t kBufferSizes32[] = {5, 8, 16};
  constexpr size_t kBufferSizes64[] = {10, 16};

  auto check_last_byte = [&](uint8_t* data, size_t buf_size, uint8_t pad,
                             uint32_t max_len, uint8_t last, bool is_valid,
                             auto expected, auto read_validated,
                             auto read_unvalidated) {
    memset(data, pad, buf_size);
    memset(data, 0x80, max_len - 1);
    data[max_len - 1] = last;
    decoder.Reset(data, data + buf_size);
    auto [res, len] = read_validated(data);
    if (is_valid) {
      EXPECT_TRUE(decoder.ok());
      EXPECT_EQ(expected, res);
      EXPECT_EQ(max_len, len);
      auto [res_noval, len_noval] = read_unvalidated(data);
      EXPECT_EQ(expected, res_noval);
      EXPECT_EQ(max_len, len_noval);
    } else {
      EXPECT_FALSE(decoder.ok());
      EXPECT_EQ(max_len - 1, decoder.error().offset());
      if (last & 0x80) {
        EXPECT_THAT(decoder.error().message(),
                    ::testing::HasSubstr("length overflow while decoding"));
      } else {
        EXPECT_EQ("extra bits in varint", decoder.error().message());
      }
    }
  };

  for (size_t buf_size : kBufferSizes32) {
    std::unique_ptr<uint8_t[]> buf = std::make_unique<uint8_t[]>(buf_size);
    uint8_t* data = buf.get();
    for (uint8_t pad : kPadBytes) {
      if (buf_size == 5 && pad != kPadBytes[0]) continue;
      for (int byte_val = 0; byte_val < 256; byte_val++) {
        uint8_t last = static_cast<uint8_t>(byte_val);

        // u32v 5th byte: valid iff last < 0x10.
        check_last_byte(
            data, buf_size, pad, 5u, last, last < 0x10,
            static_cast<uint32_t>(last) << 28,
            [&](const uint8_t* p) {
              return decoder.read_u32v<Decoder::FullValidationTag>(p);
            },
            [&](const uint8_t* p) {
              return decoder.read_u32v<Decoder::NoValidationTag>(p);
            });

        // i32v 5th byte: valid iff last in [0x00..0x07] or [0x78..0x7f].
        bool valid_i32 = (last <= 0x07) || (last >= 0x78 && last <= 0x7f);
        int32_t expected_i32 =
            base::bit_cast<int32_t>(static_cast<uint32_t>(last & 0x0f) << 28);
        check_last_byte(
            data, buf_size, pad, 5u, last, valid_i32, expected_i32,
            [&](const uint8_t* p) {
              return decoder.read_i32v<Decoder::FullValidationTag>(p);
            },
            [&](const uint8_t* p) {
              return decoder.read_i32v<Decoder::NoValidationTag>(p);
            });

        // i33v 5th byte: valid iff last in [0x00..0x0f] or [0x70..0x7f].
        bool valid_i33 = (last <= 0x0f) || (last >= 0x70 && last <= 0x7f);
        int64_t sign_ext_last =
            static_cast<int64_t>(static_cast<int8_t>(last << 1)) >> 1;
        int64_t expected_i33 =
            static_cast<int64_t>(static_cast<uint64_t>(sign_ext_last) << 28);
        check_last_byte(
            data, buf_size, pad, 5u, last, valid_i33, expected_i33,
            [&](const uint8_t* p) {
              return decoder.read_i33v<Decoder::FullValidationTag>(p);
            },
            [&](const uint8_t* p) {
              return decoder.read_i33v<Decoder::NoValidationTag>(p);
            });
      }
    }
  }

  for (size_t buf_size : kBufferSizes64) {
    std::unique_ptr<uint8_t[]> buf = std::make_unique<uint8_t[]>(buf_size);
    uint8_t* data = buf.get();
    for (uint8_t pad : kPadBytes) {
      if (buf_size == 10 && pad != kPadBytes[0]) continue;
      for (int byte_val = 0; byte_val < 256; byte_val++) {
        uint8_t last = static_cast<uint8_t>(byte_val);

        // u64v 10th byte: valid iff last <= 0x01.
        check_last_byte(
            data, buf_size, pad, 10u, last, last <= 0x01,
            static_cast<uint64_t>(last) << 63,
            [&](const uint8_t* p) {
              return decoder.read_u64v<Decoder::FullValidationTag>(p);
            },
            [&](const uint8_t* p) {
              return decoder.read_u64v<Decoder::NoValidationTag>(p);
            });

        // i64v 10th byte: valid iff last == 0x00 or last == 0x7f.
        bool valid_i64 = (last == 0x00) || (last == 0x7f);
        int64_t expected_i64 =
            last == 0 ? 0 : std::numeric_limits<int64_t>::min();
        check_last_byte(
            data, buf_size, pad, 10u, last, valid_i64, expected_i64,
            [&](const uint8_t* p) {
              return decoder.read_i64v<Decoder::FullValidationTag>(p);
            },
            [&](const uint8_t* p) {
              return decoder.read_i64v<Decoder::NoValidationTag>(p);
            });
      }
    }
  }
}

#ifdef DEBUG
TEST_F(DecoderTest, ReadLEB_TraceWasmDecoder) {
  FLAG_SCOPE(trace_wasm_decoder);
  // Multi-byte valid u32v and i32v in >= 8-byte buffer (bypasses SWAR fast
  // paths to emit per-byte trace output).
  const uint8_t valid_data[] = {U32V_2(300), U32V_3(-10000), 0, 0, 0, 0, 0};
  decoder.Reset(valid_data, valid_data + sizeof(valid_data));
  EXPECT_EQ(300u, decoder.consume_u32v("u32"));
  EXPECT_EQ(-10000, decoder.consume_i32v("i32"));
  EXPECT_TRUE(decoder.ok());

  // Truncated multi-byte LEB (<end> trace branch).
  const uint8_t truncated[] = {0x80, 0x80};
  decoder.Reset(truncated, truncated + sizeof(truncated));
  EXPECT_EQ(0u, decoder.consume_u32v("truncated"));
  EXPECT_FALSE(decoder.ok());

  // Length-overflow LEB (<length overflow> trace branch).
  const uint8_t overflow[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0, 0, 0};
  decoder.Reset(overflow, overflow + sizeof(overflow));
  EXPECT_EQ(0u, decoder.consume_u32v("overflow"));
  EXPECT_FALSE(decoder.ok());
}
#endif  // DEBUG

TEST_F(DecoderTest, ReadPrefixedOpcode) {
  const uint8_t kPrefixes[] = {kGCPrefix, kNumericPrefix, kSimdPrefix,
                               kAtomicPrefix};
  const uint8_t kTrailingBytes[] = {0x00, 0x0b, 0x1f, 0x20, 0x7f, 0x80, 0xff};

  for (uint8_t prefix : kPrefixes) {
    auto check_valid = [this, prefix](base::Vector<const uint8_t> bytes,
                                      uint32_t index,
                                      uint32_t expected_length) {
      int shift = index > 0xff ? 12 : 8;
      WasmOpcode expected_opcode = static_cast<WasmOpcode>(
          (static_cast<uint32_t>(prefix) << shift) | index);
      std::pair<WasmOpcode, uint32_t> expected = {expected_opcode,
                                                  expected_length};
      decoder.Reset(bytes.begin(), bytes.end());
      EXPECT_EQ(expected,
                decoder.read_prefixed_opcode<Decoder::FullValidationTag>(
                    bytes.begin()));
      EXPECT_TRUE(decoder.ok());
      EXPECT_EQ(expected,
                decoder.read_prefixed_opcode<Decoder::NoValidationTag>(
                    bytes.begin()));
    };

    // All 128 1-byte opcode indices (0x00..0x7f) in both exact 2-byte buffers
    // and >= 3-byte buffers (with various next instruction bytes).
    for (uint32_t index = 0; index <= 0x7f; ++index) {
      const uint8_t exact_two[] = {prefix, static_cast<uint8_t>(index)};
      check_valid(base::ArrayVector(exact_two), index, 2u);

      for (uint8_t trailing : kTrailingBytes) {
        const uint8_t with_trailing[] = {prefix, static_cast<uint8_t>(index),
                                         trailing};
        check_valid(base::ArrayVector(with_trailing), index, 2u);
      }
    }

    // All 4096 2-byte opcode indices (0x00..0xfff), covering 0x00..0x7f
    // (zero-padded), 0x80..0xff (shift 8), and 0x100..0xfff (shift 12).
    for (uint32_t index = 0; index <= 0xfff; ++index) {
      const uint8_t exact_three[] = {prefix, U32V_2(index)};
      check_valid(base::ArrayVector(exact_three), index, 3u);

      const uint8_t with_end[] = {prefix, U32V_2(index), kExprEnd};
      check_valid(base::ArrayVector(with_end), index, 3u);
    }

    // Valid zero-padded 3-, 4-, and 5-byte LEB128 opcode indices <= 0xfff.
    for (uint32_t index : {0x00u, 0x7fu, 0xa5u, 0xffu, 0x100u, 0xfffu}) {
      const uint8_t leb3[] = {prefix, U32V_3(index), kExprEnd};
      check_valid(base::ArrayVector(leb3), index, 4u);

      const uint8_t leb4[] = {prefix, U32V_4(index), kExprEnd};
      check_valid(base::ArrayVector(leb4), index, 5u);

      const uint8_t leb5[] = {prefix, U32V_5(index), kExprEnd};
      check_valid(base::ArrayVector(leb5), index, 6u);
    }
  }
}

TEST_F(DecoderTest, ReadPrefixedOpcode_InvalidAndTruncated) {
  const std::pair<WasmOpcode, uint32_t> kErrorResult = {kExprUnreachable, 0u};

  // Truncated right after prefix byte (1..5 continuation bytes missing).
  const uint8_t kTruncatedMax[] = {kSimdPrefix, 0x80, 0x80, 0x80, 0x80};
  for (size_t len = 1; len <= sizeof(kTruncatedMax); ++len) {
    decoder.Reset(kTruncatedMax, kTruncatedMax + len);
    EXPECT_EQ(kErrorResult,
              decoder.read_prefixed_opcode<Decoder::FullValidationTag>(
                  kTruncatedMax));
    EXPECT_FALSE(decoder.ok());
    EXPECT_EQ(len, decoder.error().offset());
    EXPECT_NE(std::string::npos,
              decoder.error().message().find(
                  "reached end while decoding prefixed opcode index"));
  }

  // Truncated at pc > start_ (verifies error offset is relative to pc, not
  // pc_).
  const uint8_t kTruncatedAtOffset[] = {kExprNop, kExprNop, kSimdPrefix, 0x80};
  for (size_t len = 1; len <= 2; ++len) {
    decoder.Reset(kTruncatedAtOffset, kTruncatedAtOffset + 2 + len);
    EXPECT_EQ(kErrorResult,
              decoder.read_prefixed_opcode<Decoder::FullValidationTag>(
                  kTruncatedAtOffset + 2));
    EXPECT_FALSE(decoder.ok());
    EXPECT_EQ(2u + len, decoder.error().offset());
    EXPECT_NE(std::string::npos,
              decoder.error().message().find(
                  "reached end while decoding prefixed opcode index"));
  }

  // Out-of-range indices (> 0xfff) in 2-, 3-, 4-, and 5-byte LEB encodings.
  const uint8_t kTooLarge2Min[] = {kSimdPrefix, U32V_2(0x1000), kExprEnd};
  decoder.Reset(kTooLarge2Min, kTooLarge2Min + sizeof(kTooLarge2Min));
  EXPECT_EQ(
      kErrorResult,
      decoder.read_prefixed_opcode<Decoder::FullValidationTag>(kTooLarge2Min));
  EXPECT_FALSE(decoder.ok());
  EXPECT_EQ(0u, decoder.error().offset());
  EXPECT_EQ("Invalid prefixed opcode 4096", decoder.error().message());

  // Out-of-range index at pc > start_ (verifies errorf uses pc, not pc_).
  const uint8_t kTooLargeAtOffset[] = {kExprNop, kExprNop, kSimdPrefix,
                                       U32V_2(0x1000), kExprEnd};
  decoder.Reset(kTooLargeAtOffset,
                kTooLargeAtOffset + sizeof(kTooLargeAtOffset));
  EXPECT_EQ(kErrorResult,
            decoder.read_prefixed_opcode<Decoder::FullValidationTag>(
                kTooLargeAtOffset + 2));
  EXPECT_FALSE(decoder.ok());
  EXPECT_EQ(2u, decoder.error().offset());
  EXPECT_EQ("Invalid prefixed opcode 4096", decoder.error().message());

  const uint8_t kTooLarge2Max[] = {kSimdPrefix, 0xff, 0x7f, kExprEnd};
  decoder.Reset(kTooLarge2Max, kTooLarge2Max + sizeof(kTooLarge2Max));
  EXPECT_EQ(
      kErrorResult,
      decoder.read_prefixed_opcode<Decoder::FullValidationTag>(kTooLarge2Max));
  EXPECT_FALSE(decoder.ok());
  EXPECT_EQ("Invalid prefixed opcode 16383", decoder.error().message());

  const uint8_t kTooLarge3[] = {kGCPrefix, U32V_3(0x1000), kExprEnd};
  decoder.Reset(kTooLarge3, kTooLarge3 + sizeof(kTooLarge3));
  EXPECT_EQ(
      kErrorResult,
      decoder.read_prefixed_opcode<Decoder::FullValidationTag>(kTooLarge3));
  EXPECT_FALSE(decoder.ok());
  EXPECT_EQ("Invalid prefixed opcode 4096", decoder.error().message());

  const uint8_t kTooLarge5[] = {kSimdPrefix, U32V_5(0xffffffffu), kExprEnd};
  decoder.Reset(kTooLarge5, kTooLarge5 + sizeof(kTooLarge5));
  EXPECT_EQ(
      kErrorResult,
      decoder.read_prefixed_opcode<Decoder::FullValidationTag>(kTooLarge5));
  EXPECT_FALSE(decoder.ok());
  EXPECT_EQ("Invalid prefixed opcode 4294967295", decoder.error().message());

  // Extra bits set in 5th LEB byte.
  const uint8_t kExtraBits[] = {kSimdPrefix, 0x80, 0x80,    0x80,
                                0x80,        0x10, kExprEnd};
  decoder.Reset(kExtraBits, kExtraBits + sizeof(kExtraBits));
  EXPECT_EQ(
      kErrorResult,
      decoder.read_prefixed_opcode<Decoder::FullValidationTag>(kExtraBits));
  EXPECT_FALSE(decoder.ok());
  EXPECT_EQ(5u, decoder.error().offset());
  EXPECT_EQ("extra bits in varint", decoder.error().message());

  // 6-byte LEB length overflow.
  const uint8_t kLengthOverflow[] = {kSimdPrefix, 0x80, 0x80, 0x80,
                                     0x80,        0x80, 0x00, kExprEnd};
  decoder.Reset(kLengthOverflow, kLengthOverflow + sizeof(kLengthOverflow));
  EXPECT_EQ(kErrorResult,
            decoder.read_prefixed_opcode<Decoder::FullValidationTag>(
                kLengthOverflow));
  EXPECT_FALSE(decoder.ok());
  EXPECT_EQ(5u, decoder.error().offset());
  EXPECT_NE(std::string::npos,
            decoder.error().message().find(
                "length overflow while decoding prefixed opcode index"));
}

#undef CHECK_UINT32V_INLINE
#undef CHECK_INT32V_INLINE
#undef CHECK_UINT64V_INLINE
#undef CHECK_INT64V_INLINE

}  // namespace wasm
}  // namespace internal
}  // namespace v8

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Slightly adapted for inclusion in V8.
// Copyright 2025 the V8 project authors. All rights reserved.

#include "src/base/numerics/byte_conversions.h"

#include <array>
#include <bit>
#include <concepts>
#include <cstdint>

#include "testing/gtest/include/gtest/gtest.h"

namespace v8::base::numerics {

TEST(NumericsTest, FromNativeEndian) {
  // The implementation of FromNativeEndian and FromLittleEndian assumes the
  // native endian is little. If support of big endian is desired, compile-time
  // branches will need to be added to the implementation, and the test results
  // will differ there (they would match FromBigEndian in this test).
  static_assert(std::endian::native == std::endian::little);
  {
    constexpr uint8_t bytes[] = {0x12u};
    EXPECT_EQ(U8FromNativeEndian(bytes), 0x12u);
    static_assert(std::same_as<uint8_t, decltype(U8FromNativeEndian(bytes))>);
    static_assert(U8FromNativeEndian(bytes) == 0x12u);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u};
    EXPECT_EQ(U16FromNativeEndian(bytes), 0x34'12u);
    static_assert(std::same_as<uint16_t, decltype(U16FromNativeEndian(bytes))>);
    static_assert(U16FromNativeEndian(bytes) == 0x34'12u);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u};
    EXPECT_EQ(U32FromNativeEndian(bytes), 0x78'56'34'12u);
    static_assert(std::same_as<uint32_t, decltype(U32FromNativeEndian(bytes))>);
    static_assert(U32FromNativeEndian(bytes) == 0x78'56'34'12u);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u,
                                 0x90u, 0x12u, 0x34u, 0x56u};
    EXPECT_EQ(U64FromNativeEndian(bytes), 0x56'34'12'90'78'56'34'12u);
    static_assert(std::same_as<uint64_t, decltype(U64FromNativeEndian(bytes))>);
    static_assert(U64FromNativeEndian(bytes) == 0x56'34'12'90'78'56'34'12u);
  }

  {
    constexpr uint8_t bytes[] = {0x12u};
    EXPECT_EQ(I8FromNativeEndian(bytes), 0x12);
    static_assert(std::same_as<int8_t, decltype(I8FromNativeEndian(bytes))>);
    static_assert(U8FromNativeEndian(bytes) == 0x12);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u};
    EXPECT_EQ(I16FromNativeEndian(bytes), 0x34'12);
    static_assert(std::same_as<int16_t, decltype(I16FromNativeEndian(bytes))>);
    static_assert(U16FromNativeEndian(bytes) == 0x34'12);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u};
    EXPECT_EQ(I32FromNativeEndian(bytes), 0x78'56'34'12);
    static_assert(std::same_as<int32_t, decltype(I32FromNativeEndian(bytes))>);
    static_assert(U32FromNativeEndian(bytes) == 0x78'56'34'12);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u,
                                 0x90u, 0x12u, 0x34u, 0x56u};
    EXPECT_EQ(I64FromNativeEndian(bytes), 0x56'34'12'90'78'56'34'12);
    static_assert(std::same_as<int64_t, decltype(I64FromNativeEndian(bytes))>);
    static_assert(I64FromNativeEndian(bytes) == 0x56'34'12'90'78'56'34'12);
  }

  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u};
    EXPECT_EQ(FloatFromNativeEndian(bytes), 1.73782443614e+34f);
    EXPECT_EQ(std::bit_cast<uint32_t>(FloatFromNativeEndian(bytes)),
              0x78'56'34'12u);
    static_assert(std::same_as<float, decltype(FloatFromNativeEndian(bytes))>);
    static_assert(FloatFromNativeEndian(bytes) == 1.73782443614e+34f);
    static_assert(std::bit_cast<uint32_t>(FloatFromNativeEndian(bytes)) ==
                  0x78'56'34'12u);
  }

  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u,
                                 0x90u, 0x12u, 0x34u, 0x56u};
    EXPECT_EQ(DoubleFromNativeEndian(bytes),
              1.84145159269283616391989849435e107);
    EXPECT_EQ(std::bit_cast<uint64_t>(DoubleFromNativeEndian(bytes)),
              0x56'34'12'90'78'56'34'12u);
    static_assert(
        std::same_as<double, decltype(DoubleFromNativeEndian(bytes))>);
    static_assert(DoubleFromNativeEndian(bytes) ==
                  1.84145159269283616391989849435e107);
    static_assert(std::bit_cast<uint64_t>(DoubleFromNativeEndian(bytes)) ==
                  0x56'34'12'90'78'56'34'12u);
  }
}

TEST(NumericsTest, FromLittleEndian) {
  // The implementation of FromNativeEndian and FromLittleEndian assumes the
  // native endian is little. If support of big endian is desired, compile-time
  // branches will need to be added to the implementation, and the test results
  // will differ there (they would match FromBigEndian in this test).
  static_assert(std::endian::native == std::endian::little);
  {
    constexpr uint8_t bytes[] = {0x12u};
    EXPECT_EQ(U8FromLittleEndian(bytes), 0x12u);
    static_assert(std::same_as<uint8_t, decltype(U8FromLittleEndian(bytes))>);
    static_assert(U8FromLittleEndian(bytes) == 0x12u);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u};
    EXPECT_EQ(U16FromLittleEndian(bytes), 0x34'12u);
    static_assert(std::same_as<uint16_t, decltype(U16FromLittleEndian(bytes))>);
    static_assert(U16FromLittleEndian(bytes) == 0x34'12u);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u};
    EXPECT_EQ(U32FromLittleEndian(bytes), 0x78'56'34'12u);
    static_assert(std::same_as<uint32_t, decltype(U32FromLittleEndian(bytes))>);
    static_assert(U32FromLittleEndian(bytes) == 0x78'56'34'12u);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u,
                                 0x90u, 0x12u, 0x34u, 0x56u};
    EXPECT_EQ(U64FromLittleEndian(bytes), 0x56'34'12'90'78'56'34'12u);
    static_assert(std::same_as<uint64_t, decltype(U64FromLittleEndian(bytes))>);
    static_assert(U64FromLittleEndian(bytes) == 0x56'34'12'90'78'56'34'12u);
  }

  {
    constexpr uint8_t bytes[] = {0x12u};
    EXPECT_EQ(I8FromLittleEndian(bytes), 0x12);
    static_assert(std::same_as<int8_t, decltype(I8FromLittleEndian(bytes))>);
    static_assert(I8FromLittleEndian(bytes) == 0x12);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u};
    EXPECT_EQ(I16FromLittleEndian(bytes), 0x34'12);
    static_assert(std::same_as<int16_t, decltype(I16FromLittleEndian(bytes))>);
    static_assert(I16FromLittleEndian(bytes) == 0x34'12);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u};
    EXPECT_EQ(I32FromLittleEndian(bytes), 0x78'56'34'12);
    static_assert(std::same_as<int32_t, decltype(I32FromLittleEndian(bytes))>);
    static_assert(I32FromLittleEndian(bytes) == 0x78'56'34'12);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u,
                                 0x90u, 0x12u, 0x34u, 0x56u};
    EXPECT_EQ(I64FromLittleEndian(bytes), 0x56'34'12'90'78'56'34'12);
    static_assert(std::same_as<int64_t, decltype(I64FromLittleEndian(bytes))>);
    static_assert(I64FromLittleEndian(bytes) == 0x56'34'12'90'78'56'34'12);
  }

  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u};
    EXPECT_EQ(FloatFromLittleEndian(bytes), 1.73782443614e+34f);
    EXPECT_EQ(std::bit_cast<uint32_t>(FloatFromLittleEndian(bytes)),
              0x78'56'34'12u);
    static_assert(std::same_as<float, decltype(FloatFromLittleEndian(bytes))>);
    static_assert(FloatFromLittleEndian(bytes) == 1.73782443614e+34f);
    static_assert(std::bit_cast<uint32_t>(FloatFromLittleEndian(bytes)) ==
                  0x78'56'34'12u);
  }

  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u,
                                 0x90u, 0x12u, 0x34u, 0x56u};
    EXPECT_EQ(DoubleFromLittleEndian(bytes),
              1.84145159269283616391989849435e107);
    EXPECT_EQ(std::bit_cast<uint64_t>(DoubleFromLittleEndian(bytes)),
              0x56'34'12'90'78'56'34'12u);
    static_assert(
        std::same_as<double, decltype(DoubleFromLittleEndian(bytes))>);
    static_assert(DoubleFromLittleEndian(bytes) ==
                  1.84145159269283616391989849435e107);
    static_assert(std::bit_cast<uint64_t>(DoubleFromLittleEndian(bytes)) ==
                  0x56'34'12'90'78'56'34'12u);
  }
}

TEST(NumericsTest, FromBigEndian) {
  // The implementation of FromNativeEndian and FromLittleEndian assumes the
  // native endian is little. If support of big endian is desired, compile-time
  // branches will need to be added to the implementation, and the test results
  // will differ there (they would match FromLittleEndian in this test).
  static_assert(std::endian::native == std::endian::little);
  {
    constexpr uint8_t bytes[] = {0x12u};
    EXPECT_EQ(U8FromBigEndian(bytes), 0x12u);
    static_assert(U8FromBigEndian(bytes) == 0x12u);
    static_assert(std::same_as<uint8_t, decltype(U8FromBigEndian(bytes))>);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u};
    EXPECT_EQ(U16FromBigEndian(bytes), 0x12'34u);
    static_assert(U16FromBigEndian(bytes) == 0x12'34u);
    static_assert(std::same_as<uint16_t, decltype(U16FromBigEndian(bytes))>);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u};
    EXPECT_EQ(U32FromBigEndian(bytes), 0x12'34'56'78u);
    static_assert(U32FromBigEndian(bytes) == 0x12'34'56'78u);
    static_assert(std::same_as<uint32_t, decltype(U32FromBigEndian(bytes))>);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u,
                                 0x90u, 0x12u, 0x34u, 0x56u};
    EXPECT_EQ(U64FromBigEndian(bytes), 0x12'34'56'78'90'12'34'56u);
    static_assert(U64FromBigEndian(bytes) == 0x12'34'56'78'90'12'34'56u);
    static_assert(std::same_as<uint64_t, decltype(U64FromBigEndian(bytes))>);
  }

  {
    constexpr uint8_t bytes[] = {0x12u};
    EXPECT_EQ(I8FromBigEndian(bytes), 0x12);
    static_assert(I8FromBigEndian(bytes) == 0x12);
    static_assert(std::same_as<int8_t, decltype(I8FromBigEndian(bytes))>);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u};
    EXPECT_EQ(I16FromBigEndian(bytes), 0x12'34);
    static_assert(I16FromBigEndian(bytes) == 0x12'34);
    static_assert(std::same_as<int16_t, decltype(I16FromBigEndian(bytes))>);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u};
    EXPECT_EQ(I32FromBigEndian(bytes), 0x12'34'56'78);
    static_assert(I32FromBigEndian(bytes) == 0x12'34'56'78);
    static_assert(std::same_as<int32_t, decltype(I32FromBigEndian(bytes))>);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u,
                                 0x90u, 0x12u, 0x34u, 0x56u};
    EXPECT_EQ(I64FromBigEndian(bytes), 0x12'34'56'78'90'12'34'56);
    static_assert(I64FromBigEndian(bytes) == 0x12'34'56'78'90'12'34'56);
    static_assert(std::same_as<int64_t, decltype(I64FromBigEndian(bytes))>);
  }

  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u};
    EXPECT_EQ(FloatFromBigEndian(bytes), 5.6904566139e-28f);
    EXPECT_EQ(std::bit_cast<uint32_t>(FloatFromBigEndian(bytes)),
              0x12'34'56'78u);
    static_assert(std::same_as<float, decltype(FloatFromBigEndian(bytes))>);
    static_assert(FloatFromBigEndian(bytes) == 5.6904566139e-28f);
    static_assert(std::bit_cast<uint32_t>(FloatFromBigEndian(bytes)) ==
                  0x12'34'56'78u);
  }
  {
    constexpr uint8_t bytes[] = {0x12u, 0x34u, 0x56u, 0x78u,
                                 0x90u, 0x12u, 0x34u, 0x56u};
    EXPECT_EQ(DoubleFromBigEndian(bytes), 5.62634909901491201382066931077e-221);
    EXPECT_EQ(std::bit_cast<uint64_t>(DoubleFromBigEndian(bytes)),
              0x12'34'56'78'90'12'34'56u);
    static_assert(std::same_as<double, decltype(DoubleFromBigEndian(bytes))>);
    static_assert(DoubleFromBigEndian(bytes) ==
                  5.62634909901491201382066931077e-221);
    static_assert(std::bit_cast<uint64_t>(DoubleFromBigEndian(bytes)) ==
                  0x12'34'56'78'90'12'34'56u);
  }
}

TEST(NumericsTest, ToNativeEndian) {
  // The implementation of ToNativeEndian and ToLittleEndian assumes the native
  // endian is little. If support of big endian is desired, compile-time
  // branches will need to be added to the implementation, and the test results
  // will differ there (they would match ToBigEndian in this test).
  static_assert(std::endian::native == std::endian::little);
  {
    constexpr std::array<uint8_t, 1u> bytes = {0x12u};
    constexpr auto val = uint8_t{0x12u};
    EXPECT_EQ(U8ToNativeEndian(val), bytes);
    static_assert(
        std::same_as<std::array<uint8_t, 1u>, decltype(U8ToNativeEndian(val))>);
    static_assert(U8ToNativeEndian(val) == bytes);
  }
  {
    constexpr std::array<uint8_t, 2u> bytes = {0x12u, 0x34u};
    constexpr auto val = uint16_t{0x34'12u};
    EXPECT_EQ(U16ToNativeEndian(val), bytes);
    static_assert(std::same_as<std::array<uint8_t, 2u>,
                               decltype(U16ToNativeEndian(val))>);
    static_assert(U16ToNativeEndian(val) == bytes);
  }
  {
    constexpr std::array<uint8_t, 4u> bytes = {0x12u, 0x34u, 0x56u, 0x78u};
    constexpr auto val = uint32_t{0x78'56'34'12u};
    EXPECT_EQ(U32ToNativeEndian(val), bytes);
    static_assert(std::same_as<std::array<uint8_t, 4u>,
                               decltype(U32ToNativeEndian(val))>);
    static_assert(U32ToNativeEndian(val) == bytes);
  }
  {
    constexpr std::array<uint8_t, 8u> bytes = {0x12u, 0x34u, 0x56u, 0x78u,
                                               0x90u, 0x12u, 0x34u, 0x56u};
    constexpr auto val = uint64_t{0x56'34'12'90'78'56'34'12u};
    EXPECT_EQ(U64ToNativeEndian(val), bytes);
    static_assert(std::same_as<std::array<uint8_t, 8u>,
                               decltype(U64ToNativeEndian(val))>);
    static_assert(U64ToNativeEndian(val) == bytes);
  }

  {
    constexpr std::array<uint8_t, 1u> bytes = {0x12u};
    constexpr auto val = int8_t{0x12};
    EXPECT_EQ(I8ToNativeEndian(val), bytes);
    static_assert(
        std::same_as<std::array<uint8_t, 1u>, decltype(I8ToNativeEndian(val))>);
    static_assert(I8ToNativeEndian(val) == bytes);
  }
  {
    constexpr std::array<uint8_t, 2u> bytes = {0x12u, 0x34u};
    constexpr auto val = int16_t{0x34'12};
    EXPECT_EQ(I16ToNativeEndian(val), bytes);
    static_assert(std::same_as<std::array<uint8_t, 2u>,
                               decltype(I16ToNativeEndian(val))>);
    static_assert(I16ToNativeEndian(val) == bytes);
  }
  {
    constexpr std::array<uint8_t, 4u> bytes = {0x12u, 0x34u, 0x56u, 0x78u};
    constexpr auto val = int32_t{0x78'56'34'12};
    EXPECT_EQ(I32ToNativeEndian(val), bytes);
    static_assert(std::same_as<std::array<uint8_t, 4u>,
                               decltype(I32ToNativeEndian(val))>);
    static_assert(I32ToNativeEndian(val) == bytes);
  }
  {
    constexpr std::array<uint8_t, 8u> bytes = {0x12u, 0x34u, 0x56u, 0x78u,
                                               0x90u, 0x12u, 0x34u, 0x56u};
    constexpr auto val = int64_t{0x56'34'12'90'78'56'34'12};
    EXPECT_EQ(I64ToNativeEndian(val), bytes);
    static_assert(std::same_as<std::array<uint8_t, 8u>,
                               decltype(I64ToNativeEndian(val))>);
    static_assert(I64ToNativeEndian(val) == bytes);
  }

  {
    constexpr std::array<uint8_t, 4u> bytes = {0x12u, 0x34u, 0x56u, 0x78u};
    constexpr float val = 1.73782443614e+34f;
    EXPECT_EQ(FloatToNativeEndian(val), bytes);
    static_assert(std::same_as<std::array<uint8_t, 4u>,
                               decltype(FloatToNativeEndian(val))>);
    static_assert(FloatToNativeEndian(val) == bytes);
  }

  {
    constexpr std::array<uint8_t, 8u> bytes = {0x12u, 0x34u, 0x56u, 0x78u,
                                               0x90u, 0x12u, 0x34u, 0x56u};
    constexpr double val = 1.84145159269283616391989849435e107;
    EXPECT_EQ(DoubleToNativeEndian(val), bytes);
    static_assert(std::same_as<std::array<uint8_t, 8u>,
                               decltype(DoubleToNativeEndian(val))>);
    static_assert(DoubleToNativeEndian(val) == bytes);
  }
}

TEST(NumericsTest, ToLittleEndian) {
  // The implementation of ToNativeEndian and ToLittleEndian assumes the native
  // endian is little. If support of big endian is desired, compile-time
  // branches will need to be added to the implementation, and the test results
  // will differ there (they would match ToBigEndian in this test).
  static_assert(std::endian::native == std::endian::little);
  {
    constexpr std::array<uint8_t, 1u> bytes = {0x12u};
    constexpr auto val = uint8_t{0x12u};
    EXPECT_EQ(U8ToLittleEndian(val), bytes);
    static_assert(
        std::same_as<std::array<uint8_t, 1u>, decltype(U8ToLittleEndian(val))>);
    static_assert(U8ToLittleEndian(val) == bytes);
  }
  {
    constexpr std::array<uint8_t, 2u> bytes = {0x12u, 0x34u};
    constexpr auto val = uint16_t{0x34'12u};
    EXPECT_EQ(U16ToLittleEndian(val), bytes);
    static_assert(std::same_as<std::array<uint8_t, 2u>,
                               decltype(U16ToLittleEndian(val))>);
    static_assert(U16ToLittleEndian(val) == bytes);
  }
  {
    constexpr std::array<uint8_t, 4u> bytes = {0x12u, 0x34u, 0x56u, 0x78u};
    constexpr auto val = uint32_t{0x78'56'34'12u};
    EXPECT_EQ(U32ToLittleEndian(val), bytes);
    static_assert(std::same_as<std::array<uint8_t, 4u>,
                               decltype(U32ToLittleEndian(val))>);
    static_assert(U32ToLittleEndian(val) == bytes);
  }
  {
    constexpr std::array<uint8_t, 8u> bytes = {0x12u, 0x34u, 0x56u, 0x78u,
                                               0x90u, 0x12u, 0x34u, 0x56u};
    constexpr auto val = uint64_t{0x56'34'12'90'78'56'34'12u};
    EXPECT_EQ(U64ToLittleEndian(val), bytes);
    static_assert(std::same_as<std::array<uint8_t, 8u>,
                               decltype(U64ToLittleEndian(val))>);
    static_assert(U64ToLittleEndian(val) == bytes);
  }

  {
    constexpr std::array<uint8_t, 1u> bytes = {0x12u};
    constexpr auto val = int8_t{0x12};
    EXPECT_EQ(I8ToLittleEndian(val), bytes);
    static_assert(
        std::same_as<std::array<uint8_t, 1u>, decltype(I8ToLittleEndian(val))>);
    static_assert(I8ToLittleEndian(val) == bytes);
  }
  {
    constexpr std::array<uint8_t, 2u> bytes = {0x12u, 0x34u};
    constexpr auto val = int16_t{0x34'12};
    EXPECT_EQ(I16ToLittleEndian(val), bytes);
    static_assert(std::same_as<std::array<uint8_t, 2u>,
                               decltype(I16ToLittleEndian(val))>);
    static_assert(I16ToLittleEndian(val) == bytes);
  }
  {
    constexpr std::array<uint8_t, 4u> bytes = {0x12u, 0x34u, 0x56u, 0x78u};
    constexpr auto val = int32_t{0x78'56'34'12};
    EXPECT_EQ(I32ToLittleEndian(val), bytes);
    static_assert(std::same_as<std::array<uint8_t, 4u>,
                               decltype(I32ToLittleEndian(val))>);
    static_assert(I32ToLittleEndian(val) == bytes);
  }
  {
    constexpr std::array<uint8_t, 8u> bytes = {0x12u, 0x34u, 0x56u, 0x78u,
                                               0x90u, 0x12u, 0x34u, 0x56u};
    constexpr auto val = int64_t{0x56'34'12'90'78'56'34'12};
    EXPECT_EQ(I64ToLittleEndian(val), bytes);
    static_assert(std::same_as<std::array<uint8_t, 8u>,
                               decltype(I64ToLittleEndian(val))>);
    static_assert(I64ToLittleEndian(val) == bytes);
  }

  {
    constexpr std::array<uint8_t, 4u> bytes = {0x12u, 0x34u, 0x56u, 0x78u};
    constexpr float val = 1.73782443614e+34f;
    EXPECT_EQ(FloatToLittleEndian(val), bytes);
    static_assert(std::same_as<std::array<uint8_t, 4u>,
                               decltype(FloatToLittleEndian(val))>);
    static_assert(FloatToLittleEndian(val) == bytes);
  }

  {
    constexpr std::array<uint8_t, 8u> bytes = {0x12u, 0x34u, 0x56u, 0x78u,
                                               0x90u, 0x12u, 0x34u, 0x56u};
    constexpr double val = 1.84145159269283616391989849435e107;
    EXPECT_EQ(DoubleToLittleEndian(val), bytes);
    static_assert(std::same_as<std::array<uint8_t, 8u>,
                               decltype(DoubleToLittleEndian(val))>);
    static_assert(DoubleToLittleEndian(val) == bytes);
  }
}

TEST(NumericsTest, ToBigEndian) {
  // The implementation of ToBigEndian assumes the native endian is little. If
  // support of big endian is desired, compile-time branches will need to be
  // added to the implementation, and the test results will differ there (they
  // would match ToLittleEndian in this test).
  static_assert(std::endian::native == std::endian::little);
  {
    constexpr std::array<uint8_t, 1u> bytes = {0x12u};
    constexpr auto val = uint8_t{0x12u};
    EXPECT_EQ(U8ToBigEndian(val), bytes);
    static_assert(
        std::same_as<std::array<uint8_t, 1u>, decltype(U8ToBigEndian(val))>);
    static_assert(U8ToBigEndian(val) == bytes);
  }
  {
    constexpr std::array<uint8_t, 2u> bytes = {0x12u, 0x34u};
    constexpr auto val = uint16_t{0x12'34u};
    EXPECT_EQ(U16ToBigEndian(val), bytes);
    static_assert(
        std::same_as<std::array<uint8_t, 2u>, decltype(U16ToBigEndian(val))>);
    static_assert(U16ToBigEndian(val) == bytes);
  }
  {
    constexpr std::array<uint8_t, 4u> bytes = {0x12u, 0x34u, 0x56u, 0x78u};
    constexpr auto val = uint32_t{0x12'34'56'78u};
    EXPECT_EQ(U32ToBigEndian(val), bytes);
    static_assert(
        std::same_as<std::array<uint8_t, 4u>, decltype(U32ToBigEndian(val))>);
    static_assert(U32ToBigEndian(val) == bytes);
  }
  {
    constexpr std::array<uint8_t, 8u> bytes = {0x12u, 0x34u, 0x56u, 0x78u,
                                               0x90u, 0x12u, 0x34u, 0x56u};
    constexpr auto val = uint64_t{0x12'34'56'78'90'12'34'56u};
    EXPECT_EQ(U64ToBigEndian(val), bytes);
    static_assert(
        std::same_as<std::array<uint8_t, 8u>, decltype(U64ToBigEndian(val))>);
    static_assert(U64ToBigEndian(val) == bytes);
  }

  {
    constexpr std::array<uint8_t, 1u> bytes = {0x12u};
    constexpr auto val = int8_t{0x12u};
    EXPECT_EQ(I8ToBigEndian(val), bytes);
    static_assert(
        std::same_as<std::array<uint8_t, 1u>, decltype(I8ToBigEndian(val))>);
    static_assert(I8ToBigEndian(val) == bytes);
  }
  {
    constexpr std::array<uint8_t, 2u> bytes = {0x12u, 0x34u};
    constexpr auto val = int16_t{0x12'34u};
    EXPECT_EQ(I16ToBigEndian(val), bytes);
    static_assert(
        std::same_as<std::array<uint8_t, 2u>, decltype(I16ToBigEndian(val))>);
    static_assert(I16ToBigEndian(val) == bytes);
  }
  {
    constexpr std::array<uint8_t, 4u> bytes = {0x12u, 0x34u, 0x56u, 0x78u};
    constexpr auto val = int32_t{0x12'34'56'78u};
    EXPECT_EQ(I32ToBigEndian(val), bytes);
    static_assert(
        std::same_as<std::array<uint8_t, 4u>, decltype(I32ToBigEndian(val))>);
    static_assert(I32ToBigEndian(val) == bytes);
  }
  {
    constexpr std::array<uint8_t, 8u> bytes = {0x12u, 0x34u, 0x56u, 0x78u,
                                               0x90u, 0x12u, 0x34u, 0x56u};
    constexpr auto val = int64_t{0x12'34'56'78'90'12'34'56u};
    EXPECT_EQ(I64ToBigEndian(val), bytes);
    static_assert(
        std::same_as<std::array<uint8_t, 8u>, decltype(I64ToBigEndian(val))>);
    static_assert(I64ToBigEndian(val) == bytes);
  }

  {
    constexpr std::array<uint8_t, 4u> bytes = {0x12u, 0x34u, 0x56u, 0x78u};
    constexpr float val = 5.6904566139e-28f;
    EXPECT_EQ(FloatToBigEndian(val), bytes);
    static_assert(
        std::same_as<std::array<uint8_t, 4u>, decltype(FloatToBigEndian(val))>);
    static_assert(FloatToBigEndian(val) == bytes);
  }

  {
    constexpr std::array<uint8_t, 8u> bytes = {0x12u, 0x34u, 0x56u, 0x78u,
                                               0x90u, 0x12u, 0x34u, 0x56u};
    constexpr double val = 5.62634909901491201382066931077e-221;
    EXPECT_EQ(DoubleToBigEndian(val), bytes);
    static_assert(std::same_as<std::array<uint8_t, 8u>,
                               decltype(DoubleToBigEndian(val))>);
    static_assert(DoubleToBigEndian(val) == bytes);
  }
}

}  // namespace v8::base::numerics

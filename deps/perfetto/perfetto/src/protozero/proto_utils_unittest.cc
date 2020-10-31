/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfetto/protozero/proto_utils.h"

#include <limits>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/utils.h"
#include "test/gtest_and_gmock.h"

namespace protozero {
namespace proto_utils {
namespace {

using ::perfetto::base::ArraySize;

struct VarIntExpectation {
  const char* encoded;
  size_t encoded_size;
  uint64_t int_value;
};

const VarIntExpectation kVarIntExpectations[] = {
    {"\x00", 1, 0},
    {"\x01", 1, 0x1},
    {"\x7f", 1, 0x7F},
    {"\xFF\x01", 2, 0xFF},
    {"\xFF\x7F", 2, 0x3FFF},
    {"\x80\x80\x01", 3, 0x4000},
    {"\xFF\xFF\x7F", 3, 0x1FFFFF},
    {"\x80\x80\x80\x01", 4, 0x200000},
    {"\xFF\xFF\xFF\x7F", 4, 0xFFFFFFF},
    {"\x80\x80\x80\x80\x01", 5, 0x10000000},
    {"\xFF\xFF\xFF\xFF\x0F", 5, 0xFFFFFFFF},
    {"\x80\x80\x80\x80\x10", 5, 0x100000000},
    {"\xFF\xFF\xFF\xFF\x7F", 5, 0x7FFFFFFFF},
    {"\x80\x80\x80\x80\x80\x01", 6, 0x800000000},
    {"\xFF\xFF\xFF\xFF\xFF\x7F", 6, 0x3FFFFFFFFFF},
    {"\x80\x80\x80\x80\x80\x80\x01", 7, 0x40000000000},
    {"\xFF\xFF\xFF\xFF\xFF\xFF\x7F", 7, 0x1FFFFFFFFFFFF},
    {"\x80\x80\x80\x80\x80\x80\x80\x01", 8, 0x2000000000000},
    {"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x7F", 8, 0xFFFFFFFFFFFFFF},
    {"\x80\x80\x80\x80\x80\x80\x80\x80\x01", 9, 0x100000000000000},
    {"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x7F", 9, 0x7FFFFFFFFFFFFFFF},
    {"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x01", 10, 0x8000000000000000},
    {"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01", 10, 0xFFFFFFFFFFFFFFFF},
};

TEST(ProtoUtilsTest, FieldPreambleEncoding) {
  // According to C++ standard, right shift of negative value has
  // implementation-defined resulting value.
  if ((static_cast<int32_t>(0x80000000u) >> 31) != -1)
    FAIL() << "Platform has unsupported negative number format or arithmetic";

  EXPECT_EQ(0x08u, MakeTagVarInt(1));
  EXPECT_EQ(0x09u, MakeTagFixed<uint64_t>(1));
  EXPECT_EQ(0x0Au, MakeTagLengthDelimited(1));
  EXPECT_EQ(0x0Du, MakeTagFixed<uint32_t>(1));

  EXPECT_EQ(0x03F8u, MakeTagVarInt(0x7F));
  EXPECT_EQ(0x03F9u, MakeTagFixed<int64_t>(0x7F));
  EXPECT_EQ(0x03FAu, MakeTagLengthDelimited(0x7F));
  EXPECT_EQ(0x03FDu, MakeTagFixed<int32_t>(0x7F));

  EXPECT_EQ(0x0400u, MakeTagVarInt(0x80));
  EXPECT_EQ(0x0401u, MakeTagFixed<double>(0x80));
  EXPECT_EQ(0x0402u, MakeTagLengthDelimited(0x80));
  EXPECT_EQ(0x0405u, MakeTagFixed<float>(0x80));

  EXPECT_EQ(0x01FFF8u, MakeTagVarInt(0x3fff));
  EXPECT_EQ(0x01FFF9u, MakeTagFixed<int64_t>(0x3fff));
  EXPECT_EQ(0x01FFFAu, MakeTagLengthDelimited(0x3fff));
  EXPECT_EQ(0x01FFFDu, MakeTagFixed<int32_t>(0x3fff));

  EXPECT_EQ(0x020000u, MakeTagVarInt(0x4000));
  EXPECT_EQ(0x020001u, MakeTagFixed<int64_t>(0x4000));
  EXPECT_EQ(0x020002u, MakeTagLengthDelimited(0x4000));
  EXPECT_EQ(0x020005u, MakeTagFixed<int32_t>(0x4000));
}

TEST(ProtoUtilsTest, ZigZagEncoding) {
  EXPECT_EQ(0u, ZigZagEncode(0));
  EXPECT_EQ(1u, ZigZagEncode(-1));
  EXPECT_EQ(2u, ZigZagEncode(1));
  EXPECT_EQ(3u, ZigZagEncode(-2));
  EXPECT_EQ(4294967293u, ZigZagEncode(-2147483647));
  EXPECT_EQ(4294967294u, ZigZagEncode(2147483647));
  EXPECT_EQ(std::numeric_limits<uint32_t>::max(),
            ZigZagEncode(std::numeric_limits<int32_t>::min()));
  EXPECT_EQ(std::numeric_limits<uint64_t>::max(),
            ZigZagEncode(std::numeric_limits<int64_t>::min()));

  EXPECT_EQ(0, ZigZagDecode(ZigZagEncode(0)));
  EXPECT_EQ(-1, ZigZagDecode(ZigZagEncode(-1)));
  EXPECT_EQ(1, ZigZagDecode(ZigZagEncode(1)));
  EXPECT_EQ(-127, ZigZagDecode(ZigZagEncode(-127)));
  EXPECT_EQ(0x7fffffff, ZigZagDecode(ZigZagEncode(0x7fffffff)));
  EXPECT_EQ(9000000000, ZigZagDecode(ZigZagEncode(9000000000)));
  EXPECT_EQ(-9000000000, ZigZagDecode(ZigZagEncode(-9000000000)));
}

TEST(ProtoUtilsTest, VarIntEncoding) {
  for (size_t i = 0; i < ArraySize(kVarIntExpectations); ++i) {
    const VarIntExpectation& exp = kVarIntExpectations[i];
    uint8_t buf[32];
    uint8_t* res = WriteVarInt<uint64_t>(exp.int_value, buf);
    ASSERT_EQ(exp.encoded_size, static_cast<size_t>(res - buf));
    ASSERT_EQ(0, memcmp(buf, exp.encoded, exp.encoded_size));

    if (exp.int_value <= std::numeric_limits<uint32_t>::max()) {
      uint8_t* res_32 =
          WriteVarInt<uint32_t>(static_cast<uint32_t>(exp.int_value), buf);
      ASSERT_EQ(exp.encoded_size, static_cast<size_t>(res_32 - buf));
      ASSERT_EQ(0, memcmp(buf, exp.encoded, exp.encoded_size));
    }
  }
}

TEST(ProtoUtilsTest, VarIntEncodingNegative) {
  uint8_t buf[32];
  size_t expected_size = 10;
  uint8_t expected[] = "\x9c\xff\xff\xff\xff\xff\xff\xff\xff\x01";

  {
    uint8_t* res = WriteVarInt<int8_t>(-100, buf);
    ASSERT_EQ(expected_size, static_cast<size_t>(res - buf));
    ASSERT_EQ(0, memcmp(buf, expected, expected_size));
  }

  {
    uint8_t* res = WriteVarInt<int16_t>(-100, buf);
    ASSERT_EQ(expected_size, static_cast<size_t>(res - buf));
    ASSERT_EQ(0, memcmp(buf, expected, expected_size));
  }

  {
    uint8_t* res = WriteVarInt<int32_t>(-100, buf);
    ASSERT_EQ(expected_size, static_cast<size_t>(res - buf));
    ASSERT_EQ(0, memcmp(buf, expected, expected_size));
  }

  {
    uint8_t* res = WriteVarInt<int64_t>(-100, buf);
    ASSERT_EQ(expected_size, static_cast<size_t>(res - buf));
    ASSERT_EQ(0, memcmp(buf, expected, expected_size));
  }
}

TEST(ProtoUtilsTest, RedundantVarIntEncoding) {
  uint8_t buf[kMessageLengthFieldSize];

  WriteRedundantVarInt(0, buf);
  EXPECT_EQ(0, memcmp("\x80\x80\x80\x00", buf, sizeof(buf)));

  WriteRedundantVarInt(1, buf);
  EXPECT_EQ(0, memcmp("\x81\x80\x80\x00", buf, sizeof(buf)));

  WriteRedundantVarInt(0x80, buf);
  EXPECT_EQ(0, memcmp("\x80\x81\x80\x00", buf, sizeof(buf)));

  WriteRedundantVarInt(0x332211, buf);
  EXPECT_EQ(0, memcmp("\x91\xC4\xCC\x01", buf, sizeof(buf)));

  // Largest allowed length.
  WriteRedundantVarInt(0x0FFFFFFF, buf);
  EXPECT_EQ(0, memcmp("\xFF\xFF\xFF\x7F", buf, sizeof(buf)));
}

TEST(ProtoUtilsTest, VarIntDecoding) {
  for (size_t i = 0; i < ArraySize(kVarIntExpectations); ++i) {
    const VarIntExpectation& exp = kVarIntExpectations[i];
    uint64_t value = std::numeric_limits<uint64_t>::max();
    const uint8_t* res = ParseVarInt(
        reinterpret_cast<const uint8_t*>(exp.encoded),
        reinterpret_cast<const uint8_t*>(exp.encoded + exp.encoded_size),
        &value);
    ASSERT_EQ(reinterpret_cast<const void*>(exp.encoded + exp.encoded_size),
              reinterpret_cast<const void*>(res));
    ASSERT_EQ(exp.int_value, value);
  }
}

// ParseVarInt() must fail gracefully if we hit the |end| without seeing the
// MSB == 0 (i.e. end-of-sequence).
TEST(ProtoUtilsTest, VarIntDecodingOutOfBounds) {
  uint8_t buf[] = {0xff, 0xff, 0xff, 0xff};
  for (size_t i = 0; i < 5; i++) {
    uint64_t value = static_cast<uint64_t>(-1);
    const uint8_t* res = ParseVarInt(buf, buf + i, &value);
    EXPECT_EQ(&buf[0], res);
    EXPECT_EQ(0u, value);
  }
}

// Even if we see a valid end-of-sequence, ParseVarInt() must fail if the number
// is larger than 10 bytes. That would cause subtl bugs when trying to shift
// left by more than 64 bits.
TEST(ProtoUtilsTest, RejectVarIntTooBig) {
  // This is the biggest valid varint we support (2**64 - 1).
  uint8_t good[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01};

  // Parsing this value must succeed.
  uint64_t value = static_cast<uint64_t>(-1);
  const uint8_t* res = ParseVarInt(&good[0], &good[sizeof(good)], &value);
  EXPECT_EQ(&good[sizeof(good)], res);
  EXPECT_EQ(value, static_cast<uint64_t>(-1ULL));

  uint8_t bad[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                   0xff, 0xff, 0xff, 0xff, 0x01};
  value = static_cast<uint64_t>(-1);
  res = ParseVarInt(&bad[0], &bad[sizeof(bad)], &value);
  EXPECT_EQ(&bad[0], res);
  EXPECT_EQ(0u, value);
}

}  // namespace
}  // namespace proto_utils
}  // namespace protozero

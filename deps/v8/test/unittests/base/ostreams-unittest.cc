// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/utils/ostreams.h"

#include "testing/gtest-support.h"

namespace v8 {
namespace internal {

TEST(Ostream, AsHex) {
  auto testAsHex = [](const char* expected, const AsHex& value) {
    std::ostringstream out;
    out << value;
    std::string result = out.str();
    EXPECT_EQ(expected, result);
    EXPECT_TRUE(result == expected)
        << "\nexpected: " << expected << "\ngot: " << result << "\n";
  };

  testAsHex("0", AsHex(0));
  testAsHex("", AsHex(0, 0));
  testAsHex("0x", AsHex(0, 0, true));
  testAsHex("0x0", AsHex(0, 1, true));
  testAsHex("0x00", AsHex(0, 2, true));
  testAsHex("123", AsHex(0x123, 0));
  testAsHex("0123", AsHex(0x123, 4));
  testAsHex("0x123", AsHex(0x123, 0, true));
  testAsHex("0x123", AsHex(0x123, 3, true));
  testAsHex("0x0123", AsHex(0x123, 4, true));
  testAsHex("0x00000123", AsHex(0x123, 8, true));
}

TEST(Ostream, AsHexBytes) {
  auto testAsHexBytes = [](const char* expected, const AsHexBytes& value) {
    std::ostringstream out;
    out << value;
    std::string result = out.str();
    EXPECT_EQ(expected, result);
  };

  // Little endian (default):
  testAsHexBytes("00", AsHexBytes(0));
  testAsHexBytes("", AsHexBytes(0, 0));
  testAsHexBytes("23 01", AsHexBytes(0x123));
  testAsHexBytes("23 01", AsHexBytes(0x123, 1));
  testAsHexBytes("23 01", AsHexBytes(0x123, 2));
  testAsHexBytes("23 01 00", AsHexBytes(0x123, 3));
  testAsHexBytes("ff ff ff ff", AsHexBytes(0xFFFFFFFF));
  testAsHexBytes("00 00 00 00", AsHexBytes(0, 4));
  testAsHexBytes("56 34 12", AsHexBytes(0x123456));

  // Big endian:
  testAsHexBytes("00", AsHexBytes(0, 1, AsHexBytes::kBigEndian));
  testAsHexBytes("", AsHexBytes(0, 0, AsHexBytes::kBigEndian));
  testAsHexBytes("01 23", AsHexBytes(0x123, 1, AsHexBytes::kBigEndian));
  testAsHexBytes("01 23", AsHexBytes(0x123, 1, AsHexBytes::kBigEndian));
  testAsHexBytes("01 23", AsHexBytes(0x123, 2, AsHexBytes::kBigEndian));
  testAsHexBytes("00 01 23", AsHexBytes(0x123, 3, AsHexBytes::kBigEndian));
  testAsHexBytes("ff ff ff ff", AsHexBytes(0xFFFFFFFF, AsHexBytes::kBigEndian));
  testAsHexBytes("00 00 00 00", AsHexBytes(0, 4, AsHexBytes::kBigEndian));
  testAsHexBytes("12 34 56", AsHexBytes(0x123456, 1, AsHexBytes::kBigEndian));
}

}  // namespace internal
}  // namespace v8

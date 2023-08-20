// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <initializer_list>
#include <limits>

#include "src/base/vlq-base64.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace base {

TEST(VLQBASE64, charToDigit) {
  char kSyms[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  for (int i = 0; i < 256; ++i) {
    char* pos = strchr(kSyms, static_cast<char>(i));
    int8_t expected = i == 0 || pos == nullptr ? -1 : pos - kSyms;
    EXPECT_EQ(expected, charToDigitDecodeForTesting(static_cast<uint8_t>(i)));
  }
}

struct ExpectedVLQBase64Result {
  size_t pos;
  int32_t result;
};

void TestVLQBase64Decode(
    const char* str,
    std::initializer_list<ExpectedVLQBase64Result> expected_results) {
  size_t pos = 0;
  for (const auto& expect : expected_results) {
    int32_t result = VLQBase64Decode(str, strlen(str), &pos);
    EXPECT_EQ(expect.result, result);
    EXPECT_EQ(expect.pos, pos);
  }
}

TEST(VLQBASE64, DecodeOneSegment) {
  TestVLQBase64Decode("", {{0, std::numeric_limits<int32_t>::min()}});

  // Unsupported symbol.
  TestVLQBase64Decode("*", {{0, std::numeric_limits<int32_t>::min()}});
  TestVLQBase64Decode("&", {{0, std::numeric_limits<int32_t>::min()}});
  TestVLQBase64Decode("kt:", {{2, std::numeric_limits<int32_t>::min()}});
  TestVLQBase64Decode("k^C", {{1, std::numeric_limits<int32_t>::min()}});

  // Imcomplete string.
  TestVLQBase64Decode("kth4yp", {{6, std::numeric_limits<int32_t>::min()}});

  // Interpretable strings.
  TestVLQBase64Decode("A", {{1, 0}});
  TestVLQBase64Decode("C", {{1, 1}});
  TestVLQBase64Decode("Y", {{1, 12}});
  TestVLQBase64Decode("2H", {{2, 123}});
  TestVLQBase64Decode("ktC", {{3, 1234}});
  TestVLQBase64Decode("yjY", {{3, 12345}});
  TestVLQBase64Decode("gkxH", {{4, 123456}});
  TestVLQBase64Decode("uorrC", {{5, 1234567}});
  TestVLQBase64Decode("80wxX", {{5, 12345678}});
  TestVLQBase64Decode("qxmvrH", {{6, 123456789}});
  TestVLQBase64Decode("kth4ypC", {{7, 1234567890}});
  TestVLQBase64Decode("+/////D", {{7, std::numeric_limits<int32_t>::max()}});
  TestVLQBase64Decode("D", {{1, -1}});
  TestVLQBase64Decode("Z", {{1, -12}});
  TestVLQBase64Decode("3H", {{2, -123}});
  TestVLQBase64Decode("ltC", {{3, -1234}});
  TestVLQBase64Decode("zjY", {{3, -12345}});
  TestVLQBase64Decode("hkxH", {{4, -123456}});
  TestVLQBase64Decode("vorrC", {{5, -1234567}});
  TestVLQBase64Decode("90wxX", {{5, -12345678}});
  TestVLQBase64Decode("rxmvrH", {{6, -123456789}});
  TestVLQBase64Decode("lth4ypC", {{7, -1234567890}});
  TestVLQBase64Decode("//////D", {{7, -std::numeric_limits<int32_t>::max()}});

  // An overflowed value 12345678901 (0x2DFDC1C35).
  TestVLQBase64Decode("qjuw7/2A", {{6, std::numeric_limits<int32_t>::min()}});

  // An overflowed value 123456789012(0x1CBE991A14).
  TestVLQBase64Decode("ohtkz+lH", {{6, std::numeric_limits<int32_t>::min()}});

  // An overflowed value 4294967296  (0x100000000).
  TestVLQBase64Decode("ggggggE", {{6, std::numeric_limits<int32_t>::min()}});

  // An overflowed value -12345678901, |value| = (0x2DFDC1C35).
  TestVLQBase64Decode("rjuw7/2A", {{6, std::numeric_limits<int32_t>::min()}});

  // An overflowed value -123456789012,|value| = (0x1CBE991A14).
  TestVLQBase64Decode("phtkz+lH", {{6, std::numeric_limits<int32_t>::min()}});

  // An overflowed value -4294967296,  |value| = (0x100000000).
  TestVLQBase64Decode("hgggggE", {{6, std::numeric_limits<int32_t>::min()}});
}

TEST(VLQBASE64, DecodeTwoSegment) {
  TestVLQBase64Decode("AA", {{1, 0}, {2, 0}});
  TestVLQBase64Decode("KA", {{1, 5}, {2, 0}});
  TestVLQBase64Decode("AQ", {{1, 0}, {2, 8}});
  TestVLQBase64Decode("MG", {{1, 6}, {2, 3}});
  TestVLQBase64Decode("a4E", {{1, 13}, {3, 76}});
  TestVLQBase64Decode("4GyO", {{2, 108}, {4, 233}});
  TestVLQBase64Decode("ggEqnD", {{3, 2048}, {6, 1653}});
  TestVLQBase64Decode("g2/D0ilF", {{4, 65376}, {8, 84522}});
  TestVLQBase64Decode("ss6gBy0m3B", {{5, 537798}, {10, 904521}});
  TestVLQBase64Decode("LA", {{1, -5}, {2, 0}});
  TestVLQBase64Decode("AR", {{1, 0}, {2, -8}});
  TestVLQBase64Decode("NH", {{1, -6}, {2, -3}});
  TestVLQBase64Decode("b5E", {{1, -13}, {3, -76}});
  TestVLQBase64Decode("5GzO", {{2, -108}, {4, -233}});
  TestVLQBase64Decode("hgErnD", {{3, -2048}, {6, -1653}});
  TestVLQBase64Decode("h2/D1ilF", {{4, -65376}, {8, -84522}});
  TestVLQBase64Decode("ts6gBz0m3B", {{5, -537798}, {10, -904521}});
  TestVLQBase64Decode("4GzO", {{2, 108}, {4, -233}});
  TestVLQBase64Decode("ggErnD", {{3, 2048}, {6, -1653}});
  TestVLQBase64Decode("g2/D1ilF", {{4, 65376}, {8, -84522}});
  TestVLQBase64Decode("ss6gBz0m3B", {{5, 537798}, {10, -904521}});
  TestVLQBase64Decode("5GyO", {{2, -108}, {4, 233}});
  TestVLQBase64Decode("hgEqnD", {{3, -2048}, {6, 1653}});
  TestVLQBase64Decode("h2/D0ilF", {{4, -65376}, {8, 84522}});
  TestVLQBase64Decode("ts6gBy0m3B", {{5, -537798}, {10, 904521}});
}

TEST(VLQBASE64, DecodeFourSegment) {
  TestVLQBase64Decode("AAAA", {{1, 0}, {2, 0}, {3, 0}, {4, 0}});
  TestVLQBase64Decode("QADA", {{1, 8}, {2, 0}, {3, -1}, {4, 0}});
  TestVLQBase64Decode("ECQY", {{1, 2}, {2, 1}, {3, 8}, {4, 12}});
  TestVLQBase64Decode("goGguCioPk9I",
                      {{3, 3200}, {6, 1248}, {9, 7809}, {12, 4562}});
  TestVLQBase64Decode("6/BACA", {{3, 1021}, {4, 0}, {5, 1}, {6, 0}});
  TestVLQBase64Decode("urCAQA", {{3, 1207}, {4, 0}, {5, 8}, {6, 0}});
  TestVLQBase64Decode("sDACA", {{2, 54}, {3, 0}, {4, 1}, {5, 0}});
}
}  // namespace base
}  // namespace v8

// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <google/protobuf/stubs/int128.h>

#include <algorithm>
#include <sstream>
#include <utility>

#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {

TEST(Int128, AllTests) {
  uint128 zero(0);
  uint128 one(1);
  uint128 one_2arg(0, 1);
  uint128 two(0, 2);
  uint128 three(0, 3);
  uint128 big(2000, 2);
  uint128 big_minus_one(2000, 1);
  uint128 bigger(2001, 1);
  uint128 biggest(kuint128max);
  uint128 high_low(1, 0);
  uint128 low_high(0, kuint64max);
  EXPECT_LT(one, two);
  EXPECT_GT(two, one);
  EXPECT_LT(one, big);
  EXPECT_LT(one, big);
  EXPECT_EQ(one, one_2arg);
  EXPECT_NE(one, two);
  EXPECT_GT(big, one);
  EXPECT_GE(big, two);
  EXPECT_GE(big, big_minus_one);
  EXPECT_GT(big, big_minus_one);
  EXPECT_LT(big_minus_one, big);
  EXPECT_LE(big_minus_one, big);
  EXPECT_NE(big_minus_one, big);
  EXPECT_LT(big, biggest);
  EXPECT_LE(big, biggest);
  EXPECT_GT(biggest, big);
  EXPECT_GE(biggest, big);
  EXPECT_EQ(big, ~~big);
  EXPECT_EQ(one, one | one);
  EXPECT_EQ(big, big | big);
  EXPECT_EQ(one, one | zero);
  EXPECT_EQ(one, one & one);
  EXPECT_EQ(big, big & big);
  EXPECT_EQ(zero, one & zero);
  EXPECT_EQ(zero, big & ~big);
  EXPECT_EQ(zero, one ^ one);
  EXPECT_EQ(zero, big ^ big);
  EXPECT_EQ(one, one ^ zero);

  // Shift operators.
  EXPECT_EQ(big, big << 0);
  EXPECT_EQ(big, big >> 0);
  EXPECT_GT(big << 1, big);
  EXPECT_LT(big >> 1, big);
  EXPECT_EQ(big, (big << 10) >> 10);
  EXPECT_EQ(big, (big >> 1) << 1);
  EXPECT_EQ(one, (one << 80) >> 80);
  EXPECT_EQ(zero, (one >> 80) << 80);
  EXPECT_EQ(zero, big >> 128);
  EXPECT_EQ(zero, big << 128);

  // Shift assignments.
  uint128 big_copy = big;
  EXPECT_EQ(big << 0, big_copy <<= 0);
  big_copy = big;
  EXPECT_EQ(big >> 0, big_copy >>= 0);
  big_copy = big;
  EXPECT_EQ(big << 1, big_copy <<= 1);
  big_copy = big;
  EXPECT_EQ(big >> 1, big_copy >>= 1);
  big_copy = big;
  EXPECT_EQ(big << 10, big_copy <<= 10);
  big_copy = big;
  EXPECT_EQ(big >> 10, big_copy >>= 10);
  big_copy = big;
  EXPECT_EQ(big << 64, big_copy <<= 64);
  big_copy = big;
  EXPECT_EQ(big >> 64, big_copy >>= 64);
  big_copy = big;
  EXPECT_EQ(big << 73, big_copy <<= 73);
  big_copy = big;
  EXPECT_EQ(big >> 73, big_copy >>= 73);
  big_copy = big;
  EXPECT_EQ(big << 128, big_copy <<= 128);
  big_copy = big;
  EXPECT_EQ(big >> 128, big_copy >>= 128);

  EXPECT_EQ(Uint128High64(biggest), kuint64max);
  EXPECT_EQ(Uint128Low64(biggest), kuint64max);
  EXPECT_EQ(zero + one, one);
  EXPECT_EQ(one + one, two);
  EXPECT_EQ(big_minus_one + one, big);
  EXPECT_EQ(one - one, zero);
  EXPECT_EQ(one - zero, one);
  EXPECT_EQ(zero - one, biggest);
  EXPECT_EQ(big - big, zero);
  EXPECT_EQ(big - one, big_minus_one);
  EXPECT_EQ(big + kuint64max, bigger);
  EXPECT_EQ(biggest + 1, zero);
  EXPECT_EQ(zero - 1, biggest);
  EXPECT_EQ(high_low - one, low_high);
  EXPECT_EQ(low_high + one, high_low);
  EXPECT_EQ(Uint128High64((uint128(1) << 64) - 1), 0);
  EXPECT_EQ(Uint128Low64((uint128(1) << 64) - 1), kuint64max);
  EXPECT_TRUE(!!one);
  EXPECT_TRUE(!!high_low);
  EXPECT_FALSE(!!zero);
  EXPECT_FALSE(!one);
  EXPECT_FALSE(!high_low);
  EXPECT_TRUE(!zero);
  EXPECT_TRUE(zero == 0);
  EXPECT_FALSE(zero != 0);
  EXPECT_FALSE(one == 0);
  EXPECT_TRUE(one != 0);

  uint128 test = zero;
  EXPECT_EQ(++test, one);
  EXPECT_EQ(test, one);
  EXPECT_EQ(test++, one);
  EXPECT_EQ(test, two);
  EXPECT_EQ(test -= 2, zero);
  EXPECT_EQ(test, zero);
  EXPECT_EQ(test += 2, two);
  EXPECT_EQ(test, two);
  EXPECT_EQ(--test, one);
  EXPECT_EQ(test, one);
  EXPECT_EQ(test--, one);
  EXPECT_EQ(test, zero);
  EXPECT_EQ(test |= three, three);
  EXPECT_EQ(test &= one, one);
  EXPECT_EQ(test ^= three, two);
  EXPECT_EQ(test >>= 1, one);
  EXPECT_EQ(test <<= 1, two);

  EXPECT_EQ(big, -(-big));
  EXPECT_EQ(two, -((-one) - 1));
  EXPECT_EQ(kuint128max, -one);
  EXPECT_EQ(zero, -zero);

  GOOGLE_LOG(INFO) << one;
  GOOGLE_LOG(INFO) << big_minus_one;
}

TEST(Int128, PodTests) {
  uint128_pod pod = { 12345, 67890 };
  uint128 from_pod(pod);
  EXPECT_EQ(12345, Uint128High64(from_pod));
  EXPECT_EQ(67890, Uint128Low64(from_pod));

  uint128 zero(0);
  uint128_pod zero_pod = {0, 0};
  uint128 one(1);
  uint128_pod one_pod = {0, 1};
  uint128 two(2);
  uint128_pod two_pod = {0, 2};
  uint128 three(3);
  uint128_pod three_pod = {0, 3};
  uint128 big(1, 0);
  uint128_pod big_pod = {1, 0};

  EXPECT_EQ(zero, zero_pod);
  EXPECT_EQ(zero_pod, zero);
  EXPECT_EQ(zero_pod, zero_pod);
  EXPECT_EQ(one, one_pod);
  EXPECT_EQ(one_pod, one);
  EXPECT_EQ(one_pod, one_pod);
  EXPECT_EQ(two, two_pod);
  EXPECT_EQ(two_pod, two);
  EXPECT_EQ(two_pod, two_pod);

  EXPECT_NE(one, two_pod);
  EXPECT_NE(one_pod, two);
  EXPECT_NE(one_pod, two_pod);

  EXPECT_LT(one, two_pod);
  EXPECT_LT(one_pod, two);
  EXPECT_LT(one_pod, two_pod);
  EXPECT_LE(one, one_pod);
  EXPECT_LE(one_pod, one);
  EXPECT_LE(one_pod, one_pod);
  EXPECT_LE(one, two_pod);
  EXPECT_LE(one_pod, two);
  EXPECT_LE(one_pod, two_pod);

  EXPECT_GT(two, one_pod);
  EXPECT_GT(two_pod, one);
  EXPECT_GT(two_pod, one_pod);
  EXPECT_GE(two, two_pod);
  EXPECT_GE(two_pod, two);
  EXPECT_GE(two_pod, two_pod);
  EXPECT_GE(two, one_pod);
  EXPECT_GE(two_pod, one);
  EXPECT_GE(two_pod, one_pod);

  EXPECT_EQ(three, one | two_pod);
  EXPECT_EQ(three, one_pod | two);
  EXPECT_EQ(three, one_pod | two_pod);
  EXPECT_EQ(one, three & one_pod);
  EXPECT_EQ(one, three_pod & one);
  EXPECT_EQ(one, three_pod & one_pod);
  EXPECT_EQ(two, three ^ one_pod);
  EXPECT_EQ(two, three_pod ^ one);
  EXPECT_EQ(two, three_pod ^ one_pod);
  EXPECT_EQ(two, three & (~one));
  EXPECT_EQ(three, ~~three);

  EXPECT_EQ(two, two_pod << 0);
  EXPECT_EQ(two, one_pod << 1);
  EXPECT_EQ(big, one_pod << 64);
  EXPECT_EQ(zero, one_pod << 128);
  EXPECT_EQ(two, two_pod >> 0);
  EXPECT_EQ(one, two_pod >> 1);
  EXPECT_EQ(one, big_pod >> 64);

  EXPECT_EQ(one, zero + one_pod);
  EXPECT_EQ(one, zero_pod + one);
  EXPECT_EQ(one, zero_pod + one_pod);
  EXPECT_EQ(one, two - one_pod);
  EXPECT_EQ(one, two_pod - one);
  EXPECT_EQ(one, two_pod - one_pod);
}

TEST(Int128, OperatorAssignReturnRef) {
  uint128 v(1);
  (v += 4) -= 3;
  EXPECT_EQ(2, v);
}

TEST(Int128, Multiply) {
  uint128 a, b, c;

  // Zero test.
  a = 0;
  b = 0;
  c = a * b;
  EXPECT_EQ(0, c);

  // Max carries.
  a = uint128(0) - 1;
  b = uint128(0) - 1;
  c = a * b;
  EXPECT_EQ(1, c);

  // Self-operation with max carries.
  c = uint128(0) - 1;
  c *= c;
  EXPECT_EQ(1, c);

  // 1-bit x 1-bit.
  for (int i = 0; i < 64; ++i) {
    for (int j = 0; j < 64; ++j) {
      a = uint128(1) << i;
      b = uint128(1) << j;
      c = a * b;
      EXPECT_EQ(uint128(1) << (i+j), c);
    }
  }

  // Verified with dc.
  a = uint128(PROTOBUF_ULONGLONG(0xffffeeeeddddcccc),
              PROTOBUF_ULONGLONG(0xbbbbaaaa99998888));
  b = uint128(PROTOBUF_ULONGLONG(0x7777666655554444),
              PROTOBUF_ULONGLONG(0x3333222211110000));
  c = a * b;
  EXPECT_EQ(uint128(PROTOBUF_ULONGLONG(0x530EDA741C71D4C3),
                    PROTOBUF_ULONGLONG(0xBF25975319080000)),
            c);
  EXPECT_EQ(0, c - b * a);
  EXPECT_EQ(a * a - b * b, (a + b) * (a - b));

  // Verified with dc.
  a = uint128(PROTOBUF_ULONGLONG(0x0123456789abcdef),
              PROTOBUF_ULONGLONG(0xfedcba9876543210));
  b = uint128(PROTOBUF_ULONGLONG(0x02468ace13579bdf),
              PROTOBUF_ULONGLONG(0xfdb97531eca86420));
  c = a * b;
  EXPECT_EQ(uint128(PROTOBUF_ULONGLONG(0x97a87f4f261ba3f2),
                    PROTOBUF_ULONGLONG(0x342d0bbf48948200)),
            c);
  EXPECT_EQ(0, c - b * a);
  EXPECT_EQ(a*a - b*b, (a+b) * (a-b));
}

TEST(Int128, AliasTests) {
  uint128 x1(1, 2);
  uint128 x2(2, 4);
  x1 += x1;
  EXPECT_EQ(x2, x1);

  uint128 x3(1, static_cast<uint64>(1) << 63);
  uint128 x4(3, 0);
  x3 += x3;
  EXPECT_EQ(x4, x3);
}

#ifdef PROTOBUF_HAS_DEATH_TEST
TEST(Int128, DivideByZeroCheckFails) {
  uint128 a = 0;
  uint128 b = 0;
  EXPECT_DEATH(a / b, "Division or mod by zero:");
  a = 123;
  EXPECT_DEATH(a / b, "Division or mod by zero:");
}

TEST(Int128, ModByZeroCheckFails) {
  uint128 a = 0;
  uint128 b = 0;
  EXPECT_DEATH(a % b, "Division or mod by zero:");
  a = 123;
  EXPECT_DEATH(a % b, "Division or mod by zero:");
}
#endif  // PROTOBUF_HAS_DEATH_TEST

TEST(Int128, DivideAndMod) {
  // a := q * b + r
  uint128 a, b, q, r;

  // Zero test.
  a = 0;
  b = 123;
  q = a / b;
  r = a % b;
  EXPECT_EQ(0, q);
  EXPECT_EQ(0, r);

  a = uint128(PROTOBUF_ULONGLONG(0x530eda741c71d4c3),
              PROTOBUF_ULONGLONG(0xbf25975319080000));
  q = uint128(PROTOBUF_ULONGLONG(0x4de2cab081),
              PROTOBUF_ULONGLONG(0x14c34ab4676e4bab));
  b = uint128(0x1110001);
  r = uint128(0x3eb455);
  ASSERT_EQ(a, q * b + r);  // Sanity-check.

  uint128 result_q, result_r;
  result_q = a / b;
  result_r = a % b;
  EXPECT_EQ(q, result_q);
  EXPECT_EQ(r, result_r);

  // Try the other way around.
  std::swap(q, b);
  result_q = a / b;
  result_r = a % b;
  EXPECT_EQ(q, result_q);
  EXPECT_EQ(r, result_r);
  // Restore.
  std::swap(b, q);

  // Dividend < divisor; result should be q:0 r:<dividend>.
  std::swap(a, b);
  result_q = a / b;
  result_r = a % b;
  EXPECT_EQ(0, result_q);
  EXPECT_EQ(a, result_r);
  // Try the other way around.
  std::swap(a, q);
  result_q = a / b;
  result_r = a % b;
  EXPECT_EQ(0, result_q);
  EXPECT_EQ(a, result_r);
  // Restore.
  std::swap(q, a);
  std::swap(b, a);

  // Try a large remainder.
  b = a / 2 + 1;
  uint128 expected_r(PROTOBUF_ULONGLONG(0x29876d3a0e38ea61),
                     PROTOBUF_ULONGLONG(0xdf92cba98c83ffff));
  // Sanity checks.
  ASSERT_EQ(a / 2 - 1, expected_r);
  ASSERT_EQ(a, b + expected_r);
  result_q = a / b;
  result_r = a % b;
  EXPECT_EQ(1, result_q);
  EXPECT_EQ(expected_r, result_r);
}

static uint64 RandomUint64() {
  uint64 v1 = rand();
  uint64 v2 = rand();
  uint64 v3 = rand();
  return v1 * v2 + v3;
}

TEST(Int128, DivideAndModRandomInputs) {
  const int kNumIters = 1 << 18;
  for (int i = 0; i < kNumIters; ++i) {
    const uint128 a(RandomUint64(), RandomUint64());
    const uint128 b(RandomUint64(), RandomUint64());
    if (b == 0) {
      continue;  // Avoid a div-by-zero.
    }
    const uint128 q = a / b;
    const uint128 r = a % b;
    ASSERT_EQ(a, b * q + r);
  }
}

#ifdef GOOGLE_PROTOBUF_HAS_CONSTEXPR
TEST(Int128, ConstexprTest) {
  constexpr uint128 zero;
  constexpr uint128 one = 1;
  constexpr uint128_pod pod = {2, 3};
  constexpr uint128 from_pod = pod;
  constexpr uint128 minus_two = -2;
  EXPECT_EQ(one, uint128(1));
  EXPECT_EQ(from_pod, uint128(2, 3));
  EXPECT_EQ(minus_two, uint128(-1ULL, -2ULL));
}

TEST(Int128, Traits) {
  EXPECT_TRUE(std::is_trivially_copy_constructible<uint128>::value);
  EXPECT_TRUE(std::is_trivially_copy_assignable<uint128>::value);
  EXPECT_TRUE(std::is_trivially_destructible<uint128>::value);
}
#endif  // GOOGLE_PROTOBUF_HAS_CONSTEXPR

TEST(Int128, OStream) {
  struct {
    uint128 val;
    std::ios_base::fmtflags flags;
    std::streamsize width;
    char fill;
    const char* rep;
  } cases[] = {
      // zero with different bases
      {uint128(0), std::ios::dec, 0, '_', "0"},
      {uint128(0), std::ios::oct, 0, '_', "0"},
      {uint128(0), std::ios::hex, 0, '_', "0"},
      // crossover between lo_ and hi_
      {uint128(0, -1), std::ios::dec, 0, '_', "18446744073709551615"},
      {uint128(0, -1), std::ios::oct, 0, '_', "1777777777777777777777"},
      {uint128(0, -1), std::ios::hex, 0, '_', "ffffffffffffffff"},
      {uint128(1, 0), std::ios::dec, 0, '_', "18446744073709551616"},
      {uint128(1, 0), std::ios::oct, 0, '_', "2000000000000000000000"},
      {uint128(1, 0), std::ios::hex, 0, '_', "10000000000000000"},
      // just the top bit
      {uint128(PROTOBUF_ULONGLONG(0x8000000000000000), 0), std::ios::dec, 0,
       '_', "170141183460469231731687303715884105728"},
      {uint128(PROTOBUF_ULONGLONG(0x8000000000000000), 0), std::ios::oct, 0,
       '_', "2000000000000000000000000000000000000000000"},
      {uint128(PROTOBUF_ULONGLONG(0x8000000000000000), 0), std::ios::hex, 0,
       '_', "80000000000000000000000000000000"},
      // maximum uint128 value
      {uint128(-1, -1), std::ios::dec, 0, '_',
       "340282366920938463463374607431768211455"},
      {uint128(-1, -1), std::ios::oct, 0, '_',
       "3777777777777777777777777777777777777777777"},
      {uint128(-1, -1), std::ios::hex, 0, '_',
       "ffffffffffffffffffffffffffffffff"},
      // uppercase
      {uint128(-1, -1), std::ios::hex | std::ios::uppercase, 0, '_',
       "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"},
      // showbase
      {uint128(1), std::ios::dec | std::ios::showbase, 0, '_', "1"},
      {uint128(1), std::ios::oct | std::ios::showbase, 0, '_', "01"},
      {uint128(1), std::ios::hex | std::ios::showbase, 0, '_', "0x1"},
      // showbase does nothing on zero
      {uint128(0), std::ios::dec | std::ios::showbase, 0, '_', "0"},
      {uint128(0), std::ios::oct | std::ios::showbase, 0, '_', "0"},
      {uint128(0), std::ios::hex | std::ios::showbase, 0, '_', "0"},
      // showpos does nothing on unsigned types
      {uint128(1), std::ios::dec | std::ios::showpos, 0, '_', "1"},
      // padding
      {uint128(9), std::ios::dec, 6, '_', "_____9"},
      {uint128(12345), std::ios::dec, 6, '_', "_12345"},
      // left adjustment
      {uint128(9), std::ios::dec | std::ios::left, 6, '_', "9_____"},
      {uint128(12345), std::ios::dec | std::ios::left, 6, '_', "12345_"},
  };
  for (size_t i = 0; i < GOOGLE_ARRAYSIZE(cases); ++i) {
    std::ostringstream os;
    os.flags(cases[i].flags);
    os.width(cases[i].width);
    os.fill(cases[i].fill);
    os << cases[i].val;
    EXPECT_EQ(cases[i].rep, os.str());
  }
}
}  // namespace protobuf
}  // namespace google

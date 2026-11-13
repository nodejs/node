// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/numbers/bignum.h"

#include <stdlib.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {

using BignumTest = ::testing::Test;

namespace base {
namespace test_bignum {

static const int kBufferSize = 1024;

static void AssignHexString(Bignum* bignum, const char* str) {
  bignum->AssignHexString(str);
}

TEST_F(BignumTest, Assign) {
  char buffer[kBufferSize];
  Bignum bignum;
  Bignum bignum2;
  bignum.AssignUInt16(0);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("0", buffer));
  bignum.AssignUInt16(0xA);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("A", buffer));
  bignum.AssignUInt16(0x20);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("20", buffer));

  bignum.AssignUInt64(0);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("0", buffer));
  bignum.AssignUInt64(0xA);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("A", buffer));
  bignum.AssignUInt64(0x20);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("20", buffer));
  bignum.AssignUInt64(0x100);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("100", buffer));

  // The first real test, since this will not fit into one bigit.
  bignum.AssignUInt64(0x12345678);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("12345678", buffer));

  uint64_t big = 0xFFFF'FFFF'FFFF'FFFF;
  bignum.AssignUInt64(big);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFFFFFFFFFFFFFF", buffer));

  big = 0x1234'5678'9ABC'DEF0;
  bignum.AssignUInt64(big);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("123456789ABCDEF0", buffer));

  bignum2.AssignBignum(bignum);
  EXPECT_TRUE(bignum2.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("123456789ABCDEF0", buffer));

  AssignHexString(&bignum, "0");
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("0", buffer));

  AssignHexString(&bignum, "123456789ABCDEF0");
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("123456789ABCDEF0", buffer));
}

TEST_F(BignumTest, ShiftLeft) {
  char buffer[kBufferSize];
  Bignum bignum;
  AssignHexString(&bignum, "0");
  bignum.ShiftLeft(100);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("0", buffer));

  AssignHexString(&bignum, "1");
  bignum.ShiftLeft(1);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("2", buffer));

  AssignHexString(&bignum, "1");
  bignum.ShiftLeft(4);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("10", buffer));

  AssignHexString(&bignum, "1");
  bignum.ShiftLeft(32);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("100000000", buffer));

  AssignHexString(&bignum, "1");
  bignum.ShiftLeft(64);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("10000000000000000", buffer));

  AssignHexString(&bignum, "123456789ABCDEF");
  bignum.ShiftLeft(64);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("123456789ABCDEF0000000000000000", buffer));
  bignum.ShiftLeft(1);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("2468ACF13579BDE0000000000000000", buffer));
}

TEST_F(BignumTest, AddBignum) {
  char buffer[kBufferSize];
  Bignum bignum;
  Bignum other;

  AssignHexString(&other, "1");
  AssignHexString(&bignum, "0");
  bignum.AddBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1", buffer));

  AssignHexString(&bignum, "1");
  bignum.AddBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("2", buffer));

  AssignHexString(&bignum, "FFFFFFF");
  bignum.AddBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("10000000", buffer));

  AssignHexString(&bignum, "FFFFFFFFFFFFFF");
  bignum.AddBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("100000000000000", buffer));

  AssignHexString(&bignum, "10000000000000000000000000000000000000000000");
  bignum.AddBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("10000000000000000000000000000000000000000001", buffer));

  AssignHexString(&other, "1000000000000");

  AssignHexString(&bignum, "1");
  bignum.AddBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1000000000001", buffer));

  AssignHexString(&bignum, "FFFFFFF");
  bignum.AddBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("100000FFFFFFF", buffer));

  AssignHexString(&bignum, "10000000000000000000000000000000000000000000");
  bignum.AddBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("10000000000000000000000000000001000000000000", buffer));

  AssignHexString(&bignum, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  bignum.AddBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1000000000000000000000000000000FFFFFFFFFFFF", buffer));

  bignum.AssignUInt16(0x1);
  bignum.ShiftLeft(100);
  bignum.AddBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("10000000000001000000000000", buffer));

  other.ShiftLeft(64);
  // other == "10000000000000000000000000000"

  bignum.AssignUInt16(0x1);
  bignum.AddBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("10000000000000000000000000001", buffer));

  AssignHexString(&bignum, "FFFFFFF");
  bignum.AddBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1000000000000000000000FFFFFFF", buffer));

  AssignHexString(&bignum, "10000000000000000000000000000000000000000000");
  bignum.AddBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("10000000000000010000000000000000000000000000", buffer));

  AssignHexString(&bignum, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  bignum.AddBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("100000000000000FFFFFFFFFFFFFFFFFFFFFFFFFFFF", buffer));

  bignum.AssignUInt16(0x1);
  bignum.ShiftLeft(100);
  bignum.AddBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("10010000000000000000000000000", buffer));
}

TEST_F(BignumTest, SubtractBignum) {
  char buffer[kBufferSize];
  Bignum bignum;
  Bignum other;

  AssignHexString(&bignum, "1");
  AssignHexString(&other, "0");
  bignum.SubtractBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1", buffer));

  AssignHexString(&bignum, "2");
  AssignHexString(&other, "0");
  bignum.SubtractBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("2", buffer));

  AssignHexString(&bignum, "10000000");
  AssignHexString(&other, "1");
  bignum.SubtractBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFFFFF", buffer));

  AssignHexString(&bignum, "100000000000000");
  AssignHexString(&other, "1");
  bignum.SubtractBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFFFFFFFFFFFF", buffer));

  AssignHexString(&bignum, "10000000000000000000000000000000000000000001");
  AssignHexString(&other, "1");
  bignum.SubtractBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("10000000000000000000000000000000000000000000", buffer));

  AssignHexString(&bignum, "1000000000001");
  AssignHexString(&other, "1000000000000");
  bignum.SubtractBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1", buffer));

  AssignHexString(&bignum, "100000FFFFFFF");
  AssignHexString(&other, "1000000000000");
  bignum.SubtractBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFFFFF", buffer));

  AssignHexString(&bignum, "10000000000000000000000000000001000000000000");
  AssignHexString(&other, "1000000000000");
  bignum.SubtractBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("10000000000000000000000000000000000000000000", buffer));

  AssignHexString(&bignum, "1000000000000000000000000000000FFFFFFFFFFFF");
  AssignHexString(&other, "1000000000000");
  bignum.SubtractBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", buffer));

  bignum.AssignUInt16(0x1);
  bignum.ShiftLeft(100);
  // "10 0000 0000 0000 0000 0000 0000"
  AssignHexString(&other, "1000000000000");
  bignum.SubtractBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFFFFFFFFFFF000000000000", buffer));

  AssignHexString(&other, "1000000000000");
  other.ShiftLeft(48);
  // other == "1000000000000000000000000"

  bignum.AssignUInt16(0x1);
  bignum.ShiftLeft(100);
  // bignum == "10000000000000000000000000"
  bignum.SubtractBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("F000000000000000000000000", buffer));

  other.AssignUInt16(0x1);
  other.ShiftLeft(35);
  // other == "800000000"
  AssignHexString(&bignum, "FFFFFFF");
  bignum.ShiftLeft(60);
  // bignum = FFFFFFF000000000000000
  bignum.SubtractBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFFFFEFFFFFF800000000", buffer));

  AssignHexString(&bignum, "10000000000000000000000000000000000000000000");
  bignum.SubtractBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF800000000", buffer));

  AssignHexString(&bignum, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  bignum.SubtractBignum(other);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFF", buffer));
}

TEST_F(BignumTest, MultiplyUInt32) {
  char buffer[kBufferSize];
  Bignum bignum;

  AssignHexString(&bignum, "0");
  bignum.MultiplyByUInt32(0x25);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("0", buffer));

  AssignHexString(&bignum, "2");
  bignum.MultiplyByUInt32(0x5);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("A", buffer));

  AssignHexString(&bignum, "10000000");
  bignum.MultiplyByUInt32(0x9);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("90000000", buffer));

  AssignHexString(&bignum, "100000000000000");
  bignum.MultiplyByUInt32(0xFFFF);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFF00000000000000", buffer));

  AssignHexString(&bignum, "100000000000000");
  bignum.MultiplyByUInt32(0xFFFFFFFF);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFFFFFF00000000000000", buffer));

  AssignHexString(&bignum, "1234567ABCD");
  bignum.MultiplyByUInt32(0xFFF);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("12333335552433", buffer));

  AssignHexString(&bignum, "1234567ABCD");
  bignum.MultiplyByUInt32(0xFFFFFFF);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("12345679998A985433", buffer));

  AssignHexString(&bignum, "FFFFFFFFFFFFFFFF");
  bignum.MultiplyByUInt32(0x2);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1FFFFFFFFFFFFFFFE", buffer));

  AssignHexString(&bignum, "FFFFFFFFFFFFFFFF");
  bignum.MultiplyByUInt32(0x4);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("3FFFFFFFFFFFFFFFC", buffer));

  AssignHexString(&bignum, "FFFFFFFFFFFFFFFF");
  bignum.MultiplyByUInt32(0xF);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("EFFFFFFFFFFFFFFF1", buffer));

  AssignHexString(&bignum, "FFFFFFFFFFFFFFFF");
  bignum.MultiplyByUInt32(0xFFFFFF);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFFFEFFFFFFFFFF000001", buffer));

  bignum.AssignUInt16(0x1);
  bignum.ShiftLeft(100);
  // "10 0000 0000 0000 0000 0000 0000"
  bignum.MultiplyByUInt32(2);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("20000000000000000000000000", buffer));

  bignum.AssignUInt16(0x1);
  bignum.ShiftLeft(100);
  // "10 0000 0000 0000 0000 0000 0000"
  bignum.MultiplyByUInt32(0xF);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("F0000000000000000000000000", buffer));

  bignum.AssignUInt16(0xFFFF);
  bignum.ShiftLeft(100);
  // "FFFF0 0000 0000 0000 0000 0000 0000"
  bignum.MultiplyByUInt32(0xFFFF);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFE00010000000000000000000000000", buffer));

  bignum.AssignUInt16(0xFFFF);
  bignum.ShiftLeft(100);
  // "FFFF0 0000 0000 0000 0000 0000 0000"
  bignum.MultiplyByUInt32(0xFFFFFFFF);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFEFFFF00010000000000000000000000000", buffer));

  bignum.AssignUInt16(0xFFFF);
  bignum.ShiftLeft(100);
  // "FFFF0 0000 0000 0000 0000 0000 0000"
  bignum.MultiplyByUInt32(0xFFFFFFFF);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFEFFFF00010000000000000000000000000", buffer));

  bignum.AssignUInt64(15611230384529777ULL);
  bignum.MultiplyByUInt32(10000000);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("210EDD6D4CDD2580EE80", buffer));
}

TEST_F(BignumTest, MultiplyUInt64) {
  char buffer[kBufferSize];
  Bignum bignum;

  AssignHexString(&bignum, "0");
  bignum.MultiplyByUInt64(0x25);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("0", buffer));

  AssignHexString(&bignum, "2");
  bignum.MultiplyByUInt64(0x5);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("A", buffer));

  AssignHexString(&bignum, "10000000");
  bignum.MultiplyByUInt64(0x9);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("90000000", buffer));

  AssignHexString(&bignum, "100000000000000");
  bignum.MultiplyByUInt64(0xFFFF);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFF00000000000000", buffer));

  AssignHexString(&bignum, "100000000000000");
  bignum.MultiplyByUInt64(0xFFFF'FFFF'FFFF'FFFF);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFFFFFFFFFFFFFF00000000000000", buffer));

  AssignHexString(&bignum, "1234567ABCD");
  bignum.MultiplyByUInt64(0xFFF);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("12333335552433", buffer));

  AssignHexString(&bignum, "1234567ABCD");
  bignum.MultiplyByUInt64(0xFF'FFFF'FFFF);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1234567ABCBDCBA985433", buffer));

  AssignHexString(&bignum, "FFFFFFFFFFFFFFFF");
  bignum.MultiplyByUInt64(0x2);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1FFFFFFFFFFFFFFFE", buffer));

  AssignHexString(&bignum, "FFFFFFFFFFFFFFFF");
  bignum.MultiplyByUInt64(0x4);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("3FFFFFFFFFFFFFFFC", buffer));

  AssignHexString(&bignum, "FFFFFFFFFFFFFFFF");
  bignum.MultiplyByUInt64(0xF);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("EFFFFFFFFFFFFFFF1", buffer));

  AssignHexString(&bignum, "FFFFFFFFFFFFFFFF");
  bignum.MultiplyByUInt64(0xFFFF'FFFF'FFFF'FFFF);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFFFFFFFFFFFFFE0000000000000001", buffer));

  bignum.AssignUInt16(0x1);
  bignum.ShiftLeft(100);
  // "10 0000 0000 0000 0000 0000 0000"
  bignum.MultiplyByUInt64(2);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("20000000000000000000000000", buffer));

  bignum.AssignUInt16(0x1);
  bignum.ShiftLeft(100);
  // "10 0000 0000 0000 0000 0000 0000"
  bignum.MultiplyByUInt64(0xF);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("F0000000000000000000000000", buffer));

  bignum.AssignUInt16(0xFFFF);
  bignum.ShiftLeft(100);
  // "FFFF0 0000 0000 0000 0000 0000 0000"
  bignum.MultiplyByUInt64(0xFFFF);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFE00010000000000000000000000000", buffer));

  bignum.AssignUInt16(0xFFFF);
  bignum.ShiftLeft(100);
  // "FFFF0 0000 0000 0000 0000 0000 0000"
  bignum.MultiplyByUInt64(0xFFFFFFFF);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFEFFFF00010000000000000000000000000", buffer));

  bignum.AssignUInt16(0xFFFF);
  bignum.ShiftLeft(100);
  // "FFFF0 0000 0000 0000 0000 0000 0000"
  bignum.MultiplyByUInt64(0xFFFF'FFFF'FFFF'FFFF);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFEFFFFFFFFFFFF00010000000000000000000000000", buffer));

  bignum.AssignUInt64(15611230384529777ULL);
  bignum.MultiplyByUInt64(0x8AC7'2304'89E8'0000);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1E10EE4B11D15A7F3DE7F3C7680000", buffer));
}

TEST_F(BignumTest, DivideModuloIntBignum) {
  char buffer[kBufferSize];
  Bignum bignum;
  Bignum other;
  Bignum third;

  bignum.AssignUInt16(10);
  other.AssignUInt16(2);
  EXPECT_EQ(5, bignum.DivideModuloIntBignum(other));
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("0", buffer));

  bignum.AssignUInt16(10);
  bignum.ShiftLeft(500);
  other.AssignUInt16(2);
  other.ShiftLeft(500);
  EXPECT_EQ(5, bignum.DivideModuloIntBignum(other));
  EXPECT_EQ(0, strcmp("0", buffer));

  bignum.AssignUInt16(11);
  other.AssignUInt16(2);
  EXPECT_EQ(5, bignum.DivideModuloIntBignum(other));
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1", buffer));

  bignum.AssignUInt16(10);
  bignum.ShiftLeft(500);
  other.AssignUInt16(1);
  bignum.AddBignum(other);
  other.AssignUInt16(2);
  other.ShiftLeft(500);
  EXPECT_EQ(5, bignum.DivideModuloIntBignum(other));
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1", buffer));

  bignum.AssignUInt16(10);
  bignum.ShiftLeft(500);
  other.AssignBignum(bignum);
  bignum.MultiplyByUInt32(0x1234);
  third.AssignUInt16(0xFFF);
  bignum.AddBignum(third);
  EXPECT_EQ(0x1234, bignum.DivideModuloIntBignum(other));
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFF", buffer));

  bignum.AssignUInt16(10);
  AssignHexString(&other, "1234567890");
  EXPECT_EQ(0, bignum.DivideModuloIntBignum(other));
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("A", buffer));

  AssignHexString(&bignum, "12345678");
  AssignHexString(&other, "3789012");
  EXPECT_EQ(5, bignum.DivideModuloIntBignum(other));
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("D9861E", buffer));

  AssignHexString(&bignum, "70000001");
  AssignHexString(&other, "1FFFFFFF");
  EXPECT_EQ(3, bignum.DivideModuloIntBignum(other));
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("10000004", buffer));

  AssignHexString(&bignum, "28000000");
  AssignHexString(&other, "12A05F20");
  EXPECT_EQ(2, bignum.DivideModuloIntBignum(other));
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("2BF41C0", buffer));

  bignum.AssignUInt16(10);
  bignum.ShiftLeft(500);
  other.AssignBignum(bignum);
  bignum.MultiplyByUInt32(0x1234);
  third.AssignUInt16(0xFFF);
  other.SubtractBignum(third);
  EXPECT_EQ(0x1234, bignum.DivideModuloIntBignum(other));
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1232DCC", buffer));
  EXPECT_EQ(0, bignum.DivideModuloIntBignum(other));
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1232DCC", buffer));
}

TEST_F(BignumTest, Compare) {
  Bignum bignum1;
  Bignum bignum2;
  bignum1.AssignUInt16(1);
  bignum2.AssignUInt16(1);
  EXPECT_EQ(0, Bignum::Compare(bignum1, bignum2));
  EXPECT_TRUE(Bignum::Equal(bignum1, bignum2));
  EXPECT_TRUE(Bignum::LessEqual(bignum1, bignum2));
  EXPECT_TRUE(!Bignum::Less(bignum1, bignum2));

  bignum1.AssignUInt16(0);
  bignum2.AssignUInt16(1);
  EXPECT_EQ(-1, Bignum::Compare(bignum1, bignum2));
  EXPECT_EQ(+1, Bignum::Compare(bignum2, bignum1));
  EXPECT_TRUE(!Bignum::Equal(bignum1, bignum2));
  EXPECT_TRUE(!Bignum::Equal(bignum2, bignum1));
  EXPECT_TRUE(Bignum::LessEqual(bignum1, bignum2));
  EXPECT_TRUE(!Bignum::LessEqual(bignum2, bignum1));
  EXPECT_TRUE(Bignum::Less(bignum1, bignum2));
  EXPECT_TRUE(!Bignum::Less(bignum2, bignum1));

  AssignHexString(&bignum1, "1234567890ABCDEF12345");
  AssignHexString(&bignum2, "1234567890ABCDEF12345");
  EXPECT_EQ(0, Bignum::Compare(bignum1, bignum2));

  AssignHexString(&bignum1, "1234567890ABCDEF12345");
  AssignHexString(&bignum2, "1234567890ABCDEF12346");
  EXPECT_EQ(-1, Bignum::Compare(bignum1, bignum2));
  EXPECT_EQ(+1, Bignum::Compare(bignum2, bignum1));

  AssignHexString(&bignum1, "1234567890ABCDEF12345");
  bignum1.ShiftLeft(500);
  AssignHexString(&bignum2, "1234567890ABCDEF12345");
  bignum2.ShiftLeft(500);
  EXPECT_EQ(0, Bignum::Compare(bignum1, bignum2));

  AssignHexString(&bignum1, "1234567890ABCDEF12345");
  bignum1.ShiftLeft(500);
  AssignHexString(&bignum2, "1234567890ABCDEF12346");
  bignum2.ShiftLeft(500);
  EXPECT_EQ(-1, Bignum::Compare(bignum1, bignum2));
  EXPECT_EQ(+1, Bignum::Compare(bignum2, bignum1));

  bignum1.AssignUInt16(1);
  bignum1.ShiftLeft(64);
  AssignHexString(&bignum2, "10000000000000000");
  EXPECT_EQ(0, Bignum::Compare(bignum1, bignum2));
  EXPECT_EQ(0, Bignum::Compare(bignum2, bignum1));

  bignum1.AssignUInt16(1);
  bignum1.ShiftLeft(64);
  AssignHexString(&bignum2, "10000000000000001");
  EXPECT_EQ(-1, Bignum::Compare(bignum1, bignum2));
  EXPECT_EQ(+1, Bignum::Compare(bignum2, bignum1));

  bignum1.AssignUInt16(1);
  bignum1.ShiftLeft(96);
  AssignHexString(&bignum2, "10000000000000001");
  bignum2.ShiftLeft(32);
  EXPECT_EQ(-1, Bignum::Compare(bignum1, bignum2));
  EXPECT_EQ(+1, Bignum::Compare(bignum2, bignum1));

  AssignHexString(&bignum1, "FFFFFFFFFFFFFFFF");
  bignum2.AssignUInt16(1);
  bignum2.ShiftLeft(64);
  EXPECT_EQ(-1, Bignum::Compare(bignum1, bignum2));
  EXPECT_EQ(+1, Bignum::Compare(bignum2, bignum1));

  AssignHexString(&bignum1, "FFFFFFFFFFFFFFFF");
  bignum1.ShiftLeft(32);
  bignum2.AssignUInt16(1);
  bignum2.ShiftLeft(96);
  EXPECT_EQ(-1, Bignum::Compare(bignum1, bignum2));
  EXPECT_EQ(+1, Bignum::Compare(bignum2, bignum1));

  AssignHexString(&bignum1, "FFFFFFFFFFFFFFFF");
  bignum1.ShiftLeft(32);
  bignum2.AssignUInt16(1);
  bignum2.ShiftLeft(95);
  EXPECT_EQ(+1, Bignum::Compare(bignum1, bignum2));
  EXPECT_EQ(-1, Bignum::Compare(bignum2, bignum1));

  AssignHexString(&bignum1, "FFFFFFFFFFFFFFFF");
  bignum1.ShiftLeft(32);
  bignum2.AssignUInt16(1);
  bignum2.ShiftLeft(100);
  EXPECT_EQ(-1, Bignum::Compare(bignum1, bignum2));
  EXPECT_EQ(+1, Bignum::Compare(bignum2, bignum1));

  AssignHexString(&bignum1, "100000000000000");
  bignum2.AssignUInt16(1);
  bignum2.ShiftLeft(14 * 4);
  EXPECT_EQ(0, Bignum::Compare(bignum1, bignum2));
  EXPECT_EQ(0, Bignum::Compare(bignum2, bignum1));

  AssignHexString(&bignum1, "100000000000001");
  bignum2.AssignUInt16(1);
  bignum2.ShiftLeft(14 * 4);
  EXPECT_EQ(+1, Bignum::Compare(bignum1, bignum2));
  EXPECT_EQ(-1, Bignum::Compare(bignum2, bignum1));

  AssignHexString(&bignum1, "200000000000000");
  bignum2.AssignUInt16(3);
  bignum2.ShiftLeft(14 * 4);
  EXPECT_EQ(-1, Bignum::Compare(bignum1, bignum2));
  EXPECT_EQ(+1, Bignum::Compare(bignum2, bignum1));
}

TEST_F(BignumTest, PlusCompare) {
  Bignum a;
  Bignum b;
  Bignum c;
  a.AssignUInt16(1);
  b.AssignUInt16(0);
  c.AssignUInt16(1);
  EXPECT_EQ(0, Bignum::PlusCompare(a, b, c));
  EXPECT_TRUE(Bignum::PlusEqual(a, b, c));
  EXPECT_TRUE(Bignum::PlusLessEqual(a, b, c));
  EXPECT_TRUE(!Bignum::PlusLess(a, b, c));

  a.AssignUInt16(0);
  b.AssignUInt16(0);
  c.AssignUInt16(1);
  EXPECT_EQ(-1, Bignum::PlusCompare(a, b, c));
  EXPECT_EQ(+1, Bignum::PlusCompare(c, b, a));
  EXPECT_TRUE(!Bignum::PlusEqual(a, b, c));
  EXPECT_TRUE(!Bignum::PlusEqual(c, b, a));
  EXPECT_TRUE(Bignum::PlusLessEqual(a, b, c));
  EXPECT_TRUE(!Bignum::PlusLessEqual(c, b, a));
  EXPECT_TRUE(Bignum::PlusLess(a, b, c));
  EXPECT_TRUE(!Bignum::PlusLess(c, b, a));

  AssignHexString(&a, "1234567890ABCDEF12345");
  b.AssignUInt16(1);
  AssignHexString(&c, "1234567890ABCDEF12345");
  EXPECT_EQ(+1, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567890ABCDEF12344");
  b.AssignUInt16(1);
  AssignHexString(&c, "1234567890ABCDEF12345");
  EXPECT_EQ(0, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567890");
  a.ShiftLeft(11 * 4);
  AssignHexString(&b, "ABCDEF12345");
  AssignHexString(&c, "1234567890ABCDEF12345");
  EXPECT_EQ(0, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567890");
  a.ShiftLeft(11 * 4);
  AssignHexString(&b, "ABCDEF12344");
  AssignHexString(&c, "1234567890ABCDEF12345");
  EXPECT_EQ(-1, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567890");
  a.ShiftLeft(11 * 4);
  AssignHexString(&b, "ABCDEF12346");
  AssignHexString(&c, "1234567890ABCDEF12345");
  EXPECT_EQ(1, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567891");
  a.ShiftLeft(11 * 4);
  AssignHexString(&b, "ABCDEF12345");
  AssignHexString(&c, "1234567890ABCDEF12345");
  EXPECT_EQ(1, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567889");
  a.ShiftLeft(11 * 4);
  AssignHexString(&b, "ABCDEF12345");
  AssignHexString(&c, "1234567890ABCDEF12345");
  EXPECT_EQ(-1, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567890");
  a.ShiftLeft(11 * 4 + 32);
  AssignHexString(&b, "ABCDEF12345");
  b.ShiftLeft(32);
  AssignHexString(&c, "1234567890ABCDEF12345");
  c.ShiftLeft(32);
  EXPECT_EQ(0, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567890");
  a.ShiftLeft(11 * 4 + 32);
  AssignHexString(&b, "ABCDEF12344");
  b.ShiftLeft(32);
  AssignHexString(&c, "1234567890ABCDEF12345");
  c.ShiftLeft(32);
  EXPECT_EQ(-1, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567890");
  a.ShiftLeft(11 * 4 + 32);
  AssignHexString(&b, "ABCDEF12346");
  b.ShiftLeft(32);
  AssignHexString(&c, "1234567890ABCDEF12345");
  c.ShiftLeft(32);
  EXPECT_EQ(1, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567891");
  a.ShiftLeft(11 * 4 + 32);
  AssignHexString(&b, "ABCDEF12345");
  b.ShiftLeft(32);
  AssignHexString(&c, "1234567890ABCDEF12345");
  c.ShiftLeft(32);
  EXPECT_EQ(1, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567889");
  a.ShiftLeft(11 * 4 + 32);
  AssignHexString(&b, "ABCDEF12345");
  b.ShiftLeft(32);
  AssignHexString(&c, "1234567890ABCDEF12345");
  c.ShiftLeft(32);
  EXPECT_EQ(-1, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567890");
  a.ShiftLeft(11 * 4 + 32);
  AssignHexString(&b, "ABCDEF12345");
  b.ShiftLeft(32);
  AssignHexString(&c, "1234567890ABCDEF1234500000000");
  EXPECT_EQ(0, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567890");
  a.ShiftLeft(11 * 4 + 32);
  AssignHexString(&b, "ABCDEF12344");
  b.ShiftLeft(32);
  AssignHexString(&c, "1234567890ABCDEF1234500000000");
  EXPECT_EQ(-1, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567890");
  a.ShiftLeft(11 * 4 + 32);
  AssignHexString(&b, "ABCDEF12346");
  b.ShiftLeft(32);
  AssignHexString(&c, "1234567890ABCDEF1234500000000");
  EXPECT_EQ(1, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567891");
  a.ShiftLeft(11 * 4 + 32);
  AssignHexString(&b, "ABCDEF12345");
  b.ShiftLeft(32);
  AssignHexString(&c, "1234567890ABCDEF1234500000000");
  EXPECT_EQ(1, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567889");
  a.ShiftLeft(11 * 4 + 32);
  AssignHexString(&b, "ABCDEF12345");
  b.ShiftLeft(32);
  AssignHexString(&c, "1234567890ABCDEF1234500000000");
  EXPECT_EQ(-1, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567890");
  a.ShiftLeft(11 * 4 + 32);
  AssignHexString(&b, "ABCDEF12345");
  AssignHexString(&c, "123456789000000000ABCDEF12345");
  EXPECT_EQ(0, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567890");
  a.ShiftLeft(11 * 4 + 32);
  AssignHexString(&b, "ABCDEF12346");
  AssignHexString(&c, "123456789000000000ABCDEF12345");
  EXPECT_EQ(1, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567890");
  a.ShiftLeft(11 * 4 + 32);
  AssignHexString(&b, "ABCDEF12344");
  AssignHexString(&c, "123456789000000000ABCDEF12345");
  EXPECT_EQ(-1, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567890");
  a.ShiftLeft(11 * 4 + 32);
  AssignHexString(&b, "ABCDEF12345");
  b.ShiftLeft(16);
  AssignHexString(&c, "12345678900000ABCDEF123450000");
  EXPECT_EQ(0, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567890");
  a.ShiftLeft(11 * 4 + 32);
  AssignHexString(&b, "ABCDEF12344");
  b.ShiftLeft(16);
  AssignHexString(&c, "12345678900000ABCDEF123450000");
  EXPECT_EQ(-1, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567890");
  a.ShiftLeft(11 * 4 + 32);
  AssignHexString(&b, "ABCDEF12345");
  b.ShiftLeft(16);
  AssignHexString(&c, "12345678900000ABCDEF123450001");
  EXPECT_EQ(-1, Bignum::PlusCompare(a, b, c));

  AssignHexString(&a, "1234567890");
  a.ShiftLeft(11 * 4 + 32);
  AssignHexString(&b, "ABCDEF12346");
  b.ShiftLeft(16);
  AssignHexString(&c, "12345678900000ABCDEF123450000");
  EXPECT_EQ(+1, Bignum::PlusCompare(a, b, c));
}

TEST_F(BignumTest, Square) {
  Bignum bignum;
  char buffer[kBufferSize];

  bignum.AssignUInt16(1);
  bignum.Square();
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1", buffer));

  bignum.AssignUInt16(2);
  bignum.Square();
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("4", buffer));

  bignum.AssignUInt16(10);
  bignum.Square();
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("64", buffer));

  AssignHexString(&bignum, "FFFFFFF");
  bignum.Square();
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFFFFE0000001", buffer));

  AssignHexString(&bignum, "FFFFFFFFFFFFFF");
  bignum.Square();
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("FFFFFFFFFFFFFE00000000000001", buffer));
}

TEST_F(BignumTest, AssignPowerUInt16) {
  Bignum bignum;
  char buffer[kBufferSize];

  bignum.AssignPowerUInt16(1, 0);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1", buffer));

  bignum.AssignPowerUInt16(1, 1);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1", buffer));

  bignum.AssignPowerUInt16(1, 2);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1", buffer));

  bignum.AssignPowerUInt16(2, 0);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1", buffer));

  bignum.AssignPowerUInt16(2, 1);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("2", buffer));

  bignum.AssignPowerUInt16(2, 2);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("4", buffer));

  bignum.AssignPowerUInt16(16, 1);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("10", buffer));

  bignum.AssignPowerUInt16(16, 2);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("100", buffer));

  bignum.AssignPowerUInt16(16, 5);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("100000", buffer));

  bignum.AssignPowerUInt16(16, 8);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("100000000", buffer));

  bignum.AssignPowerUInt16(16, 16);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("10000000000000000", buffer));

  bignum.AssignPowerUInt16(16, 30);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1000000000000000000000000000000", buffer));

  bignum.AssignPowerUInt16(10, 0);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1", buffer));

  bignum.AssignPowerUInt16(10, 1);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("A", buffer));

  bignum.AssignPowerUInt16(10, 2);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("64", buffer));

  bignum.AssignPowerUInt16(10, 5);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("186A0", buffer));

  bignum.AssignPowerUInt16(10, 8);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("5F5E100", buffer));

  bignum.AssignPowerUInt16(10, 16);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("2386F26FC10000", buffer));

  bignum.AssignPowerUInt16(10, 30);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("C9F2C9CD04674EDEA40000000", buffer));

  bignum.AssignPowerUInt16(10, 31);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("7E37BE2022C0914B2680000000", buffer));

  bignum.AssignPowerUInt16(2, 0);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1", buffer));

  bignum.AssignPowerUInt16(2, 100);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("10000000000000000000000000", buffer));

  bignum.AssignPowerUInt16(17, 0);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1", buffer));

  bignum.AssignPowerUInt16(17, 99);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0, strcmp("1942BB9853FAD924A3D4DD92B89B940E0207BEF05DB9C26BC1B757"
                      "80BE0C5A2C2990E02A681224F34ED68558CE4C6E33760931",
                      buffer));

  bignum.AssignPowerUInt16(0xFFFF, 99);
  EXPECT_TRUE(bignum.ToHexString(buffer, kBufferSize));
  EXPECT_EQ(0,
            strcmp("FF9D12F09B886C54E77E7439C7D2DED2D34F669654C0C2B6B8C288250"
                   "5A2211D0E3DC9A61831349EAE674B11D56E3049D7BD79DAAD6C9FA2BA"
                   "528E3A794299F2EE9146A324DAFE3E88967A0358233B543E233E575B9"
                   "DD4E3AA7942146426C328FF55BFD5C45E0901B1629260AF9AE2F310C5"
                   "50959FAF305C30116D537D80CF6EBDBC15C5694062AF1AC3D956D0A41"
                   "B7E1B79FF11E21D83387A1CE1F5882B31E4B5D8DE415BDBE6854466DF"
                   "343362267A7E8833119D31D02E18DB5B0E8F6A64B0ED0D0062FFFF",
                   buffer));
}

}  // namespace test_bignum
}  // namespace base
}  // namespace v8

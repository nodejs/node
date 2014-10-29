// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Check all examples from table 10-1 of "Hacker's Delight".

#include "src/base/division-by-constant.h"

#include <ostream>  // NOLINT

#include "testing/gtest-support.h"

namespace v8 {
namespace base {

template <class T>
std::ostream& operator<<(std::ostream& os,
                         const MagicNumbersForDivision<T>& mag) {
  return os << "{ multiplier: " << mag.multiplier << ", shift: " << mag.shift
            << ", add: " << mag.add << " }";
}


// Some abbreviations...

typedef MagicNumbersForDivision<uint32_t> M32;
typedef MagicNumbersForDivision<uint64_t> M64;


static M32 s32(int32_t d) {
  return SignedDivisionByConstant<uint32_t>(static_cast<uint32_t>(d));
}


static M64 s64(int64_t d) {
  return SignedDivisionByConstant<uint64_t>(static_cast<uint64_t>(d));
}


static M32 u32(uint32_t d) { return UnsignedDivisionByConstant<uint32_t>(d); }
static M64 u64(uint64_t d) { return UnsignedDivisionByConstant<uint64_t>(d); }


TEST(DivisionByConstant, Signed32) {
  EXPECT_EQ(M32(0x99999999U, 1, false), s32(-5));
  EXPECT_EQ(M32(0x55555555U, 1, false), s32(-3));
  int32_t d = -1;
  for (unsigned k = 1; k <= 32 - 1; ++k) {
    d *= 2;
    EXPECT_EQ(M32(0x7FFFFFFFU, k - 1, false), s32(d));
  }
  for (unsigned k = 1; k <= 32 - 2; ++k) {
    EXPECT_EQ(M32(0x80000001U, k - 1, false), s32(1 << k));
  }
  EXPECT_EQ(M32(0x55555556U, 0, false), s32(3));
  EXPECT_EQ(M32(0x66666667U, 1, false), s32(5));
  EXPECT_EQ(M32(0x2AAAAAABU, 0, false), s32(6));
  EXPECT_EQ(M32(0x92492493U, 2, false), s32(7));
  EXPECT_EQ(M32(0x38E38E39U, 1, false), s32(9));
  EXPECT_EQ(M32(0x66666667U, 2, false), s32(10));
  EXPECT_EQ(M32(0x2E8BA2E9U, 1, false), s32(11));
  EXPECT_EQ(M32(0x2AAAAAABU, 1, false), s32(12));
  EXPECT_EQ(M32(0x51EB851FU, 3, false), s32(25));
  EXPECT_EQ(M32(0x10624DD3U, 3, false), s32(125));
  EXPECT_EQ(M32(0x68DB8BADU, 8, false), s32(625));
}


TEST(DivisionByConstant, Unsigned32) {
  EXPECT_EQ(M32(0x00000000U, 0, true), u32(1));
  for (unsigned k = 1; k <= 30; ++k) {
    EXPECT_EQ(M32(1U << (32 - k), 0, false), u32(1U << k));
  }
  EXPECT_EQ(M32(0xAAAAAAABU, 1, false), u32(3));
  EXPECT_EQ(M32(0xCCCCCCCDU, 2, false), u32(5));
  EXPECT_EQ(M32(0xAAAAAAABU, 2, false), u32(6));
  EXPECT_EQ(M32(0x24924925U, 3, true), u32(7));
  EXPECT_EQ(M32(0x38E38E39U, 1, false), u32(9));
  EXPECT_EQ(M32(0xCCCCCCCDU, 3, false), u32(10));
  EXPECT_EQ(M32(0xBA2E8BA3U, 3, false), u32(11));
  EXPECT_EQ(M32(0xAAAAAAABU, 3, false), u32(12));
  EXPECT_EQ(M32(0x51EB851FU, 3, false), u32(25));
  EXPECT_EQ(M32(0x10624DD3U, 3, false), u32(125));
  EXPECT_EQ(M32(0xD1B71759U, 9, false), u32(625));
}


TEST(DivisionByConstant, Signed64) {
  EXPECT_EQ(M64(0x9999999999999999ULL, 1, false), s64(-5));
  EXPECT_EQ(M64(0x5555555555555555ULL, 1, false), s64(-3));
  int64_t d = -1;
  for (unsigned k = 1; k <= 64 - 1; ++k) {
    d *= 2;
    EXPECT_EQ(M64(0x7FFFFFFFFFFFFFFFULL, k - 1, false), s64(d));
  }
  for (unsigned k = 1; k <= 64 - 2; ++k) {
    EXPECT_EQ(M64(0x8000000000000001ULL, k - 1, false), s64(1LL << k));
  }
  EXPECT_EQ(M64(0x5555555555555556ULL, 0, false), s64(3));
  EXPECT_EQ(M64(0x6666666666666667ULL, 1, false), s64(5));
  EXPECT_EQ(M64(0x2AAAAAAAAAAAAAABULL, 0, false), s64(6));
  EXPECT_EQ(M64(0x4924924924924925ULL, 1, false), s64(7));
  EXPECT_EQ(M64(0x1C71C71C71C71C72ULL, 0, false), s64(9));
  EXPECT_EQ(M64(0x6666666666666667ULL, 2, false), s64(10));
  EXPECT_EQ(M64(0x2E8BA2E8BA2E8BA3ULL, 1, false), s64(11));
  EXPECT_EQ(M64(0x2AAAAAAAAAAAAAABULL, 1, false), s64(12));
  EXPECT_EQ(M64(0xA3D70A3D70A3D70BULL, 4, false), s64(25));
  EXPECT_EQ(M64(0x20C49BA5E353F7CFULL, 4, false), s64(125));
  EXPECT_EQ(M64(0x346DC5D63886594BULL, 7, false), s64(625));
}


TEST(DivisionByConstant, Unsigned64) {
  EXPECT_EQ(M64(0x0000000000000000ULL, 0, true), u64(1));
  for (unsigned k = 1; k <= 64 - 2; ++k) {
    EXPECT_EQ(M64(1ULL << (64 - k), 0, false), u64(1ULL << k));
  }
  EXPECT_EQ(M64(0xAAAAAAAAAAAAAAABULL, 1, false), u64(3));
  EXPECT_EQ(M64(0xCCCCCCCCCCCCCCCDULL, 2, false), u64(5));
  EXPECT_EQ(M64(0xAAAAAAAAAAAAAAABULL, 2, false), u64(6));
  EXPECT_EQ(M64(0x2492492492492493ULL, 3, true), u64(7));
  EXPECT_EQ(M64(0xE38E38E38E38E38FULL, 3, false), u64(9));
  EXPECT_EQ(M64(0xCCCCCCCCCCCCCCCDULL, 3, false), u64(10));
  EXPECT_EQ(M64(0x2E8BA2E8BA2E8BA3ULL, 1, false), u64(11));
  EXPECT_EQ(M64(0xAAAAAAAAAAAAAAABULL, 3, false), u64(12));
  EXPECT_EQ(M64(0x47AE147AE147AE15ULL, 5, true), u64(25));
  EXPECT_EQ(M64(0x0624DD2F1A9FBE77ULL, 7, true), u64(125));
  EXPECT_EQ(M64(0x346DC5D63886594BULL, 7, false), u64(625));
}

}  // namespace base
}  // namespace v8

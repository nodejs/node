// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
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

#include "src/base/numbers/double.h"

#include <stdlib.h>

#include "src/base/numbers/diy-fp.h"
#include "src/common/globals.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {

using DoubleTest = ::testing::Test;

namespace base {

TEST_F(DoubleTest, Uint64Conversions) {
  // Start by checking the byte-order.
  uint64_t ordered = 0x0123'4567'89AB'CDEF;
  CHECK_EQ(3512700564088504e-318, Double(ordered).value());

  uint64_t min_double64 = 0x0000'0000'0000'0001;
  CHECK_EQ(5e-324, Double(min_double64).value());

  uint64_t max_double64 = 0x7FEF'FFFF'FFFF'FFFF;
  CHECK_EQ(1.7976931348623157e308, Double(max_double64).value());
}

TEST_F(DoubleTest, AsDiyFp) {
  uint64_t ordered = 0x0123'4567'89AB'CDEF;
  DiyFp diy_fp = Double(ordered).AsDiyFp();
  CHECK_EQ(0x12 - 0x3FF - 52, diy_fp.e());
  // The 52 mantissa bits, plus the implicit 1 in bit 52 as a UINT64.
  CHECK(0x0013'4567'89AB'CDEF == diy_fp.f());  // NOLINT

  uint64_t min_double64 = 0x0000'0000'0000'0001;
  diy_fp = Double(min_double64).AsDiyFp();
  CHECK_EQ(-0x3FF - 52 + 1, diy_fp.e());
  // This is a denormal; so no hidden bit.
  CHECK_EQ(1, diy_fp.f());

  uint64_t max_double64 = 0x7FEF'FFFF'FFFF'FFFF;
  diy_fp = Double(max_double64).AsDiyFp();
  CHECK_EQ(0x7FE - 0x3FF - 52, diy_fp.e());
  CHECK(0x001F'FFFF'FFFF'FFFF == diy_fp.f());  // NOLINT
}

TEST_F(DoubleTest, AsNormalizedDiyFp) {
  uint64_t ordered = 0x0123'4567'89AB'CDEF;
  DiyFp diy_fp = Double(ordered).AsNormalizedDiyFp();
  CHECK_EQ(0x12 - 0x3FF - 52 - 11, diy_fp.e());
  CHECK((uint64_t{0x0013'4567'89AB'CDEF} << 11) == diy_fp.f());  // NOLINT

  uint64_t min_double64 = 0x0000'0000'0000'0001;
  diy_fp = Double(min_double64).AsNormalizedDiyFp();
  CHECK_EQ(-0x3FF - 52 + 1 - 63, diy_fp.e());
  // This is a denormal; so no hidden bit.
  CHECK(0x8000'0000'0000'0000 == diy_fp.f());  // NOLINT

  uint64_t max_double64 = 0x7FEF'FFFF'FFFF'FFFF;
  diy_fp = Double(max_double64).AsNormalizedDiyFp();
  CHECK_EQ(0x7FE - 0x3FF - 52 - 11, diy_fp.e());
  CHECK((uint64_t{0x001F'FFFF'FFFF'FFFF} << 11) == diy_fp.f());
}

TEST_F(DoubleTest, IsDenormal) {
  uint64_t min_double64 = 0x0000'0000'0000'0001;
  CHECK(Double(min_double64).IsDenormal());
  uint64_t bits = 0x000F'FFFF'FFFF'FFFF;
  CHECK(Double(bits).IsDenormal());
  bits = 0x0010'0000'0000'0000;
  CHECK(!Double(bits).IsDenormal());
}

TEST_F(DoubleTest, IsSpecial) {
  CHECK(Double(V8_INFINITY).IsSpecial());
  CHECK(Double(-V8_INFINITY).IsSpecial());
  CHECK(Double(std::numeric_limits<double>::quiet_NaN()).IsSpecial());
  uint64_t bits = 0xFFF1'2345'0000'0000;
  CHECK(Double(bits).IsSpecial());
  // Denormals are not special:
  CHECK(!Double(5e-324).IsSpecial());
  CHECK(!Double(-5e-324).IsSpecial());
  // And some random numbers:
  CHECK(!Double(0.0).IsSpecial());
  CHECK(!Double(-0.0).IsSpecial());
  CHECK(!Double(1.0).IsSpecial());
  CHECK(!Double(-1.0).IsSpecial());
  CHECK(!Double(1000000.0).IsSpecial());
  CHECK(!Double(-1000000.0).IsSpecial());
  CHECK(!Double(1e23).IsSpecial());
  CHECK(!Double(-1e23).IsSpecial());
  CHECK(!Double(1.7976931348623157e308).IsSpecial());
  CHECK(!Double(-1.7976931348623157e308).IsSpecial());
}

TEST_F(DoubleTest, IsInfinite) {
  CHECK(Double(V8_INFINITY).IsInfinite());
  CHECK(Double(-V8_INFINITY).IsInfinite());
  CHECK(!Double(std::numeric_limits<double>::quiet_NaN()).IsInfinite());
  CHECK(!Double(0.0).IsInfinite());
  CHECK(!Double(-0.0).IsInfinite());
  CHECK(!Double(1.0).IsInfinite());
  CHECK(!Double(-1.0).IsInfinite());
  uint64_t min_double64 = 0x0000'0000'0000'0001;
  CHECK(!Double(min_double64).IsInfinite());
}

TEST_F(DoubleTest, Sign) {
  CHECK_EQ(1, Double(1.0).Sign());
  CHECK_EQ(1, Double(V8_INFINITY).Sign());
  CHECK_EQ(-1, Double(-V8_INFINITY).Sign());
  CHECK_EQ(1, Double(0.0).Sign());
  CHECK_EQ(-1, Double(-0.0).Sign());
  uint64_t min_double64 = 0x0000'0000'0000'0001;
  CHECK_EQ(1, Double(min_double64).Sign());
}

TEST_F(DoubleTest, NormalizedBoundaries) {
  DiyFp boundary_plus;
  DiyFp boundary_minus;
  DiyFp diy_fp = Double(1.5).AsNormalizedDiyFp();
  Double(1.5).NormalizedBoundaries(&boundary_minus, &boundary_plus);
  CHECK_EQ(diy_fp.e(), boundary_minus.e());
  CHECK_EQ(diy_fp.e(), boundary_plus.e());
  // 1.5 does not have a significand of the form 2^p (for some p).
  // Therefore its boundaries are at the same distance.
  CHECK(diy_fp.f() - boundary_minus.f() == boundary_plus.f() - diy_fp.f());
  CHECK((1 << 10) == diy_fp.f() - boundary_minus.f());

  diy_fp = Double(1.0).AsNormalizedDiyFp();
  Double(1.0).NormalizedBoundaries(&boundary_minus, &boundary_plus);
  CHECK_EQ(diy_fp.e(), boundary_minus.e());
  CHECK_EQ(diy_fp.e(), boundary_plus.e());
  // 1.0 does have a significand of the form 2^p (for some p).
  // Therefore its lower boundary is twice as close as the upper boundary.
  CHECK_GT(boundary_plus.f() - diy_fp.f(), diy_fp.f() - boundary_minus.f());
  CHECK((1 << 9) == diy_fp.f() - boundary_minus.f());
  CHECK((1 << 10) == boundary_plus.f() - diy_fp.f());

  uint64_t min_double64 = 0x0000'0000'0000'0001;
  diy_fp = Double(min_double64).AsNormalizedDiyFp();
  Double(min_double64).NormalizedBoundaries(&boundary_minus, &boundary_plus);
  CHECK_EQ(diy_fp.e(), boundary_minus.e());
  CHECK_EQ(diy_fp.e(), boundary_plus.e());
  // min-value does not have a significand of the form 2^p (for some p).
  // Therefore its boundaries are at the same distance.
  CHECK(diy_fp.f() - boundary_minus.f() == boundary_plus.f() - diy_fp.f());
  // Denormals have their boundaries much closer.
  CHECK((static_cast<uint64_t>(1) << 62) == diy_fp.f() - boundary_minus.f());

  uint64_t smallest_normal64 = 0x0010'0000'0000'0000;
  diy_fp = Double(smallest_normal64).AsNormalizedDiyFp();
  Double(smallest_normal64)
      .NormalizedBoundaries(&boundary_minus, &boundary_plus);
  CHECK_EQ(diy_fp.e(), boundary_minus.e());
  CHECK_EQ(diy_fp.e(), boundary_plus.e());
  // Even though the significand is of the form 2^p (for some p), its boundaries
  // are at the same distance. (This is the only exception).
  CHECK(diy_fp.f() - boundary_minus.f() == boundary_plus.f() - diy_fp.f());
  CHECK((1 << 10) == diy_fp.f() - boundary_minus.f());

  uint64_t largest_denormal64 = 0x000F'FFFF'FFFF'FFFF;
  diy_fp = Double(largest_denormal64).AsNormalizedDiyFp();
  Double(largest_denormal64)
      .NormalizedBoundaries(&boundary_minus, &boundary_plus);
  CHECK_EQ(diy_fp.e(), boundary_minus.e());
  CHECK_EQ(diy_fp.e(), boundary_plus.e());
  CHECK(diy_fp.f() - boundary_minus.f() == boundary_plus.f() - diy_fp.f());
  CHECK((1 << 11) == diy_fp.f() - boundary_minus.f());

  uint64_t max_double64 = 0x7FEF'FFFF'FFFF'FFFF;
  diy_fp = Double(max_double64).AsNormalizedDiyFp();
  Double(max_double64).NormalizedBoundaries(&boundary_minus, &boundary_plus);
  CHECK_EQ(diy_fp.e(), boundary_minus.e());
  CHECK_EQ(diy_fp.e(), boundary_plus.e());
  // max-value does not have a significand of the form 2^p (for some p).
  // Therefore its boundaries are at the same distance.
  CHECK(diy_fp.f() - boundary_minus.f() == boundary_plus.f() - diy_fp.f());
  CHECK((1 << 10) == diy_fp.f() - boundary_minus.f());
}

TEST_F(DoubleTest, NextDouble) {
  CHECK_EQ(4e-324, Double(0.0).NextDouble());
  CHECK_EQ(0.0, Double(-0.0).NextDouble());
  CHECK_EQ(-0.0, Double(-4e-324).NextDouble());
  Double d0(-4e-324);
  Double d1(d0.NextDouble());
  Double d2(d1.NextDouble());
  CHECK_EQ(-0.0, d1.value());
  CHECK_EQ(0.0, d2.value());
  CHECK_EQ(4e-324, d2.NextDouble());
  CHECK_EQ(-1.7976931348623157e308, Double(-V8_INFINITY).NextDouble());
  CHECK_EQ(V8_INFINITY, Double(uint64_t{0x7FEF'FFFF'FFFF'FFFF}).NextDouble());
}

}  // namespace base
}  // namespace v8

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "src/base/numbers/bignum.h"
#include "src/base/numbers/double.h"
#include "src/base/utils/random-number-generator.h"
#include "src/builtins/builtins-math-xsum.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using MathXsumTest = ::testing::Test;

TEST_F(MathXsumTest, XsumSimpleSum) {
  Xsum acc;
  acc.Add(1.0);
  acc.Add(2.0);
  EXPECT_EQ(3.0, acc.Round());
}

TEST_F(MathXsumTest, XsumPreciseSum) {
  Xsum acc;
  acc.Add(1e100);
  acc.Add(1.0);
  acc.Add(-1e100);
  EXPECT_EQ(1.0, acc.Round());
}

TEST_F(MathXsumTest, XsumManyAdds) {
  Xsum acc;
  for (int i = 0; i < 1000; ++i) {
    acc.Add(1.0);
  }
  EXPECT_EQ(1000.0, acc.Round());
}

TEST_F(MathXsumTest, XsumInfinity) {
  Xsum acc;
  acc.Add(std::numeric_limits<double>::infinity());
  acc.Add(1.0);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), acc.Round());

  Xsum acc2;
  acc2.Add(-std::numeric_limits<double>::infinity());
  acc2.Add(1.0);
  EXPECT_EQ(-std::numeric_limits<double>::infinity(), acc2.Round());
}

TEST_F(MathXsumTest, XsumNaN) {
  Xsum acc;
  acc.Add(std::numeric_limits<double>::quiet_NaN());
  acc.Add(1.0);
  EXPECT_TRUE(std::isnan(acc.Round()));
}

TEST_F(MathXsumTest, XsumInfMinusInf) {
  Xsum acc;
  acc.Add(std::numeric_limits<double>::infinity());
  acc.Add(-std::numeric_limits<double>::infinity());
  EXPECT_TRUE(std::isnan(acc.Round()));
}

TEST_F(MathXsumTest, ReproFailure) {
  Xsum acc;
  std::vector<double> inputs = {
      -1.1442589134409902e+308, 9.593842098384855e+138, 4.494232837155791e+307,
      -1.3482698511467367e+308, 4.494232837155792e+307};
  for (double d : inputs) {
    acc.Add(d);
  }
  // Expected: -1.5936821971565685e+308
  double expected = -1.5936821971565685e+308;
  double result = acc.Round();
  if (result != expected) {
    printf("Expected: %.20e\n", expected);
    printf("Actual:   %.20e\n", result);
  }
  EXPECT_EQ(expected, result);
}

TEST_F(MathXsumTest, RoundingEdgeCases) {
  // Test rounding to nearest, ties to even.
  auto check = [](double start, double add, double expected) {
    Xsum acc;
    acc.Add(start);
    acc.Add(add);
    EXPECT_EQ(expected, acc.Round()) << "Start: " << start << ", Add: " << add;
  };

  double one = 1.0;
  double ulp = std::pow(2.0, -52);  // ULP at 1.0
  double half_ulp = std::pow(2.0, -53);
  double tiny = std::pow(2.0, -100);

  // Positive ties to even
  // 1.0 (mantissa ...00) + 0.5 ULP -> 1.0 (Round down to even)
  check(one, half_ulp, one);

  // (1.0 + ULP) (mantissa ...01) + 0.5 ULP -> 1.0 + 2*ULP (Round up to even)
  check(one + ulp, half_ulp, one + 2 * ulp);

  // Negative ties to even
  // -1.0 - 0.5 ULP -> -1.0
  check(-one, -half_ulp, -one);

  // -(1.0 + ULP) - 0.5 ULP -> -(1.0 + 2*ULP)
  check(-(one + ulp), -half_ulp, -(one + 2 * ulp));

  // Positive with sticky bit (should round away / up)
  // 1.0 + 0.5 ULP + eps -> 1.0 + ULP
  // We can't easily add (half_ulp + tiny) as one double if tiny is too small.
  // Add them separately.
  {
    Xsum acc;
    acc.Add(one);
    acc.Add(half_ulp);
    acc.Add(tiny);
    EXPECT_EQ(one + ulp, acc.Round());
  }

  // Negative with sticky bit (should round away / down magnitude)
  // -1.0 - 0.5 ULP - eps -> -1.0 - ULP
  {
    Xsum acc;
    acc.Add(-one);
    acc.Add(-half_ulp);
    acc.Add(-tiny);
    EXPECT_EQ(-(one + ulp), acc.Round());
  }
}

// Helpers for Verification
//
// Compare implementation against a version using bignums. Original
// implementation by Kevin Gibbons in fuzz.c from xsum.

// Sufficiently large shift to treat all doubles as integers in Bignum.
const int kScaleShift = 2000;

void DoubleToBignum(double val, base::Bignum& b) {
  if (val == 0.0) {
    b.AssignUInt64(0);
    return;
  }
  base::Double d(val);
  uint64_t sig = d.Significand();
  int exp = d.Exponent();
  int shift = exp + kScaleShift;
  CHECK_GE(shift, 0);
  b.AssignUInt64(sig);
  b.ShiftLeft(shift);
}

// Converts a Bignum (representing a value * 2^kScaleShift) to a double,
// with correct rounding (nearest, ties to even).
double BignumToDouble(const base::Bignum& bignum_val, bool negative) {
  base::Bignum zero;
  if (base::Bignum::Compare(bignum_val, zero) == 0)
    return negative ? -0.0 : 0.0;

  char buffer[2048];
  bool ok = bignum_val.ToHexString(buffer, sizeof(buffer));
  CHECK(ok);

  size_t len = strlen(buffer);
  size_t first_nz = 0;
  while (first_nz < len && buffer[first_nz] == '0') first_nz++;
  if (first_nz == len) return negative ? -0.0 : 0.0;

  int first_digit_val = 0;
  char c = buffer[first_nz];
  if (c >= '0' && c <= '9')
    first_digit_val = c - '0';
  else if (c >= 'A' && c <= 'F')
    first_digit_val = c - 'A' + 10;
  else if (c >= 'a' && c <= 'f')
    first_digit_val = c - 'a' + 10;

  int msb_in_digit = 0;
  if (first_digit_val >= 8)
    msb_in_digit = 3;
  else if (first_digit_val >= 4)
    msb_in_digit = 2;
  else if (first_digit_val >= 2)
    msb_in_digit = 1;

  int msb_pos = (static_cast<int>(len) - 1 - static_cast<int>(first_nz)) * 4 +
                msb_in_digit;
  int exponent = msb_pos - kScaleShift;

  uint64_t f = 0;
  int bits_filled = 0;
  f |= static_cast<uint64_t>(first_digit_val) << (63 - msb_in_digit);
  bits_filled += (msb_in_digit + 1);

  size_t cur = first_nz + 1;
  while (cur < len && bits_filled < 64) {
    int hex_val = 0;
    char ch = buffer[cur];
    if (ch >= '0' && ch <= '9')
      hex_val = ch - '0';
    else if (ch >= 'A' && ch <= 'F')
      hex_val = ch - 'A' + 10;
    else if (ch >= 'a' && ch <= 'f')
      hex_val = ch - 'a' + 10;

    if (bits_filled <= 60) {
      f |= static_cast<uint64_t>(hex_val) << (60 - bits_filled);
      bits_filled += 4;
    } else {
      int bits_needed = 64 - bits_filled;
      int bits_unused = 4 - bits_needed;
      f |= static_cast<uint64_t>(hex_val >> bits_unused);
      bits_filled = 64;
    }
    cur++;
  }

  bool sticky = false;
  while (cur < len) {
    if (buffer[cur] != '0') {
      sticky = true;
      break;
    }
    cur++;
  }

  uint64_t mantissa = f >> 11;
  bool round_bit = (f >> 10) & 1;
  bool sticky_bit = sticky || ((f & 0x3FF) != 0);

  if (round_bit) {
    if (sticky_bit || (mantissa & 1)) {
      mantissa++;
      if (mantissa >= (uint64_t{1} << 53)) {
        mantissa >>= 1;
        exponent++;
      }
    }
  }

  int biased_exp = exponent + 1023;
  if (biased_exp >= 2047) {
    return negative ? -std::numeric_limits<double>::infinity()
                    : std::numeric_limits<double>::infinity();
  }
  if (biased_exp <= 0) {
    return negative ? -0.0 : 0.0;
  }

  uint64_t packed_mantissa = mantissa & 0x000F'FFFF'FFFF'FFFF;
  uint64_t result_bits =
      (static_cast<uint64_t>(biased_exp) << 52) | packed_mantissa;
  if (negative) result_bits |= 0x8000'0000'0000'0000;
  return base::bit_cast<double>(result_bits);
}

TEST_F(MathXsumTest, BignumVerificationSmoke) {
  auto check_ref = [](std::vector<double> addends, double expected) {
    base::Bignum pos, neg;
    pos.AssignUInt64(0);
    neg.AssignUInt64(0);
    for (double d : addends) {
      base::Bignum b;
      DoubleToBignum(std::abs(d), b);
      if (d >= 0)
        pos.AddBignum(b);
      else
        neg.AddBignum(b);
    }
    int cmp = base::Bignum::Compare(pos, neg);
    double res;
    if (cmp == 0) {
      res = 0.0;
    } else if (cmp > 0) {
      base::Bignum diff;
      diff.AssignBignum(pos);
      diff.SubtractBignum(neg);
      res = BignumToDouble(diff, false);
    } else {
      base::Bignum diff;
      diff.AssignBignum(neg);
      diff.SubtractBignum(pos);
      res = BignumToDouble(diff, true);
    }
    EXPECT_DOUBLE_EQ(expected, res);
  };

  check_ref({1.0, 2.0}, 3.0);
  check_ref({1e100, 1.0, -1e100}, 1.0);
  check_ref({1e100, -1e100}, 0.0);
  check_ref({1.1, 2.2}, 1.1 + 2.2);
}

TEST_F(MathXsumTest, FuzzAgainstBignum) {
  base::RandomNumberGenerator rng(GTEST_FLAG_GET(random_seed));
  base::Bignum pos_sum;
  base::Bignum neg_sum;

  for (int iter = 0; iter < 1000; ++iter) {
    Xsum sacc;
    pos_sum.AssignUInt64(0);
    neg_sum.AssignUInt64(0);

    int count = (rng.NextInt(7)) + 2;
    for (int j = 0; j < count; ++j) {
      double v = base::bit_cast<double>(rng.NextInt64());
      if (std::isnan(v) || std::isinf(v)) v = 0.0;

      sacc.Add(v);
      base::Bignum term;
      DoubleToBignum(std::abs(v), term);
      if (v >= 0)
        pos_sum.AddBignum(term);
      else
        neg_sum.AddBignum(term);
    }

    double result_s = sacc.Round();
    double result_ref;
    int cmp = base::Bignum::Compare(pos_sum, neg_sum);
    if (cmp == 0) {
      result_ref = 0.0;
    } else if (cmp > 0) {
      base::Bignum diff;
      diff.AssignBignum(pos_sum);
      diff.SubtractBignum(neg_sum);
      result_ref = BignumToDouble(diff, false);
    } else {
      base::Bignum diff;
      diff.AssignBignum(neg_sum);
      diff.SubtractBignum(pos_sum);
      result_ref = BignumToDouble(diff, true);
    }

    if (std::abs(result_s) < 1e-300 && std::abs(result_ref) < 1e-300) continue;

    EXPECT_DOUBLE_EQ(result_s, result_ref);
  }
}

}  // namespace internal
}  // namespace v8

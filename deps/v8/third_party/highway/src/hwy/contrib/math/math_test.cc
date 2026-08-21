// Copyright 2020 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdint.h>
#include <stdio.h>

#include <cfloat>  // FLT_MAX
#include <cmath>   // std::abs

// For faster tests. Not using AES, hence NEON_WITHOUT_AES is sufficient.
// SVE is mostly superseded by SVE2.
#ifndef HWY_DISABLED_TARGETS
#define HWY_DISABLED_TARGETS (HWY_NEON | HWY_SVE)
#endif  // HWY_DISABLED_TARGETS

#include "hwy/base.h"

// clang-format off
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "hwy/contrib/math/math_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/contrib/math/fast_math-inl.h"
#include "hwy/contrib/math/math-inl.h"
#include "hwy/tests/test_util-inl.h"
// clang-format on

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace {

// We have had test failures caused by excess precision due to keeping
// intermediate results in 80-bit x87 registers. One such failure mode is that
// Log1p computes a 1.0 which is not exactly equal to 1.0f, causing is_pole to
// incorrectly evaluate to false.
#undef HWY_MATH_TEST_EXCESS_PRECISION
#if HWY_ARCH_X86_32 && HWY_COMPILER_GCC_ACTUAL && \
    (HWY_TARGET == HWY_SCALAR || HWY_TARGET == HWY_EMU128)

// GCC 13+: because CMAKE_CXX_EXTENSIONS is OFF, we build with -std= and hence
// also -fexcess-precision=standard, so there is no problem. See #1708 and
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=323.
#if HWY_COMPILER_GCC_ACTUAL >= 1300
#define HWY_MATH_TEST_EXCESS_PRECISION 0

#else                  // HWY_COMPILER_GCC_ACTUAL < 1300

// The build system must enable SSE2, e.g. via HWY_CMAKE_SSE2 - see
// https://stackoverflow.com/questions/20869904/c-handling-of-excess-precision .
#if defined(__SSE2__)  // correct flag given, no problem
#define HWY_MATH_TEST_EXCESS_PRECISION 0
#else
#define HWY_MATH_TEST_EXCESS_PRECISION 1
#pragma message( \
    "Skipping scalar math_test on 32-bit x86 GCC <13 without HWY_CMAKE_SSE2")
#endif  // defined(__SSE2__)

#endif  // HWY_COMPILER_GCC_ACTUAL
#else   // not (x86-32, GCC, scalar target): running math_test normally
#define HWY_MATH_TEST_EXCESS_PRECISION 0
#endif  // HWY_ARCH_X86_32 etc

template <class T, class D>
HWY_NOINLINE void TestMath(const char* name, T (*fx1)(T),
                           Vec<D> (*fxN)(D, VecArg<Vec<D>>), D d, T min, T max,
                           uint64_t max_error_ulp) {
  if (HWY_MATH_TEST_EXCESS_PRECISION) {
    static bool once = true;
    if (once) {
      once = false;
      HWY_WARN("Skipping math_test due to GCC issue with excess precision.\n");
    }
    return;
  }

  using UintT = MakeUnsigned<T>;

  const UintT min_bits = BitCastScalar<UintT>(min);
  const UintT max_bits = BitCastScalar<UintT>(max);

  // If min is negative and max is positive, the range needs to be broken into
  // two pieces, [+0, max] and [-0, min], otherwise [min, max].
  int range_count = 1;
  UintT ranges[2][2] = {{min_bits, max_bits}, {0, 0}};
  if ((min < T{0}) && (max > T{0})) {
    ranges[0][0] = BitCastScalar<UintT>(ConvertScalarTo<T>(+0.0));
    ranges[0][1] = max_bits;
    ranges[1][0] = BitCastScalar<UintT>(ConvertScalarTo<T>(-0.0));
    ranges[1][1] = min_bits;
    range_count = 2;
  } else {
    // If not splitting, ensure we iterate from smaller uint to larger uint.
    // For negative numbers, min (e.g. -1000) has larger uint representation
    // than max (e.g. -1).
    if (ranges[0][0] > ranges[0][1]) {
      auto tmp = ranges[0][0];
      ranges[0][0] = ranges[0][1];
      ranges[0][1] = tmp;
    }
  }

  uint64_t max_ulp = 0;
  // Emulation is slower, so cannot afford as many.
  constexpr UintT kSamplesPerRange =
      static_cast<UintT>(AdjustedReps(static_cast<size_t>(1000)));
  for (int range_index = 0; range_index < range_count; ++range_index) {
    const UintT start = ranges[range_index][0];
    const UintT stop = ranges[range_index][1];
    const UintT step = HWY_MAX(1, ((stop - start) / kSamplesPerRange));
    for (UintT value_bits = start; value_bits <= stop; value_bits += step) {
      // For reasons unknown, the HWY_MAX is necessary on RVV, otherwise
      // value_bits can be less than start, and thus possibly NaN.
      const T value =
          BitCastScalar<T>(HWY_MIN(HWY_MAX(start, value_bits), stop));
      const T actual = GetLane(fxN(d, Set(d, value)));
      const T expected = fx1(value);

      // Skip small inputs and outputs on armv7, it flushes subnormals to zero.
#if HWY_TARGET <= HWY_NEON_WITHOUT_AES && HWY_ARCH_ARM_V7
      if ((std::abs(value) < 1e-37f) || (std::abs(expected) < 1e-37f)) {
        continue;
      }
#endif

      const auto ulp = hwy::detail::ComputeUlpDelta(actual, expected);
      max_ulp = HWY_MAX(max_ulp, ulp);
      if (ulp > max_error_ulp) {
        fprintf(stderr, "%s: %s(%f) expected %E actual %E ulp %g max ulp %u\n",
                hwy::TypeName(T(), Lanes(d)).c_str(), name, value,
                static_cast<double>(expected), static_cast<double>(actual),
                static_cast<double>(ulp), static_cast<uint32_t>(max_error_ulp));
      }
    }
  }
  fprintf(stderr, "%s: %s max_ulp %g\n", hwy::TypeName(T(), Lanes(d)).c_str(),
          name, static_cast<double>(max_ulp));
  HWY_ASSERT(max_ulp <= max_error_ulp);
}

#define DEFINE_MATH_TEST_FUNC(NAME)                     \
  HWY_NOINLINE void TestAll##NAME() {                   \
    ForFloat3264Types(ForPartialVectors<Test##NAME>()); \
  }

#undef DEFINE_MATH_TEST
#define DEFINE_MATH_TEST(NAME, F32x1, F32xN, F32_MIN, F32_MAX, F32_ERROR,     \
                         F64x1, F64xN, F64_MIN, F64_MAX, F64_ERROR)           \
  struct Test##NAME {                                                         \
    template <class T, class D, HWY_IF_T_SIZE(T, 4)>                          \
    HWY_NOINLINE void operator()(T, D d) {                                    \
      TestMath<T, D>(HWY_STR(NAME), F32x1, F32xN, d, F32_MIN, F32_MAX,        \
                     F32_ERROR);                                              \
    }                                                                         \
    template <class T, class D, HWY_IF_T_SIZE(T, 8)>                          \
    HWY_NOINLINE void operator()(T, D d) {                                    \
      TestMath<T, D>(HWY_STR(NAME), F64x1, F64xN, d, static_cast<T>(F64_MIN), \
                     static_cast<T>(F64_MAX), F64_ERROR);                     \
    }                                                                         \
  };                                                                          \
  DEFINE_MATH_TEST_FUNC(NAME)

// clang-format off
DEFINE_MATH_TEST(Exp,
  std::exp,   CallExp,   -FLT_MAX,   +104.0f,     1,
  std::exp,   CallExp,   -DBL_MAX,   +104.0,      1)
DEFINE_MATH_TEST(Exp2,
  std::exp2,  CallExp2,  -FLT_MAX,   +128.0f,     2,
  std::exp2,  CallExp2,  -DBL_MAX,   +128.0,      2)
DEFINE_MATH_TEST(Expm1,
  std::expm1, CallExpm1, -FLT_MAX,   +104.0f,     4,
  std::expm1, CallExpm1, -DBL_MAX,   +104.0,      4)
DEFINE_MATH_TEST(Log,
  std::log,   CallLog,   +FLT_MIN,   +FLT_MAX,    1,
  std::log,   CallLog,   +DBL_MIN,   +DBL_MAX,    1)
DEFINE_MATH_TEST(Log10,
  std::log10, CallLog10, +FLT_MIN,   +FLT_MAX,    2,
  std::log10, CallLog10, +DBL_MIN,   +DBL_MAX,    2)
DEFINE_MATH_TEST(Log1p,
  std::log1p, CallLog1p, +0.0f,      +FLT_MAX,    3,  // NEON is 3 instead of 2
  std::log1p, CallLog1p, +0.0,       +DBL_MAX,    2)
DEFINE_MATH_TEST(Log2,
  std::log2,  CallLog2,  +FLT_MIN,   +FLT_MAX,    2,
  std::log2,  CallLog2,  +DBL_MIN,   +DBL_MAX,    2)
// clang-format on

template <class T, class D>
HWY_NOINLINE void TestMathRelative(const char* name, T (*fx1)(T),
                                   Vec<D> (*fxN)(D, VecArg<Vec<D>>), D d, T min,
                                   T max, double max_relative_error,
                                   uint64_t samples = 4000) {
  if (HWY_MATH_TEST_EXCESS_PRECISION) {
    static bool once = true;
    if (once) {
      once = false;
      HWY_WARN("Skipping math_test due to GCC issue with excess precision.\n");
    }
    return;
  }

  using UintT = MakeUnsigned<T>;

  const UintT min_bits = BitCastScalar<UintT>(min);
  const UintT max_bits = BitCastScalar<UintT>(max);

  // If min is negative and max is positive, the range needs to be broken into
  // two pieces, [+0, max] and [-0, min], otherwise [min, max].
  int range_count = 1;
  UintT ranges[2][2] = {{min_bits, max_bits}, {0, 0}};
  if ((min < 0.0) && (max > 0.0)) {
    ranges[0][0] = BitCastScalar<UintT>(ConvertScalarTo<T>(+0.0));
    ranges[0][1] = max_bits;
    ranges[1][0] = BitCastScalar<UintT>(ConvertScalarTo<T>(-0.0));
    ranges[1][1] = min_bits;
    range_count = 2;
  } else {
    // If not splitting, ensure we iterate from smaller uint to larger uint.
    // For negative numbers, min (e.g. -1000) has larger uint representation
    // than max (e.g. -1).
    if (ranges[0][0] > ranges[0][1]) {
      auto tmp = ranges[0][0];
      ranges[0][0] = ranges[0][1];
      ranges[0][1] = tmp;
    }
  }

  double max_actual_rel_error = 0.0;
  double max_error_value = 0.0;
  double sum_rel_error = 0.0;
  uint64_t count = 0;
  // Emulation is slower, so cannot afford as many.
  const UintT kSamplesPerRange =
      static_cast<UintT>(AdjustedReps(static_cast<size_t>(samples)));
  for (int range_index = 0; range_index < range_count; ++range_index) {
    const UintT start = ranges[range_index][0];
    const UintT stop = ranges[range_index][1];
    const UintT step = HWY_MAX(1, ((stop - start) / kSamplesPerRange));
    for (UintT value_bits = start; value_bits <= stop; value_bits += step) {
      // For reasons unknown, the HWY_MAX is necessary on RVV, otherwise
      // value_bits can be less than start, and thus possibly NaN.
      const T value =
          BitCastScalar<T>(HWY_MIN(HWY_MAX(start, value_bits), stop));
      const T actual = GetLane(fxN(d, Set(d, value)));
      const T expected = fx1(value);

      // Skip small inputs and outputs on armv7, it flushes subnormals to zero.
#if HWY_TARGET <= HWY_NEON_WITHOUT_AES && HWY_ARCH_ARM_V7
      if ((std::abs(value) < 1e-37f) || (std::abs(expected) < 1e-37f)) {
        continue;
      }
#endif

      if (std::abs(expected) > 0.0) {
        double rel = std::abs(static_cast<double>(actual) -
                              static_cast<double>(expected)) /
                     std::abs(static_cast<double>(expected));
        if (ScalarIsNaN(rel) || rel > max_actual_rel_error) {
          max_actual_rel_error = rel;
          max_error_value = static_cast<double>(value);
        }
        sum_rel_error += rel;
        count++;
        if (rel > max_relative_error) {
          static int print_count = 0;
          if (print_count < 10) {
            fprintf(stderr,
                    "%s: %s(%f) expected %E actual %E rel %E max rel %E\n",
                    hwy::TypeName(T(), Lanes(d)).c_str(), name,
                    static_cast<double>(value), static_cast<double>(expected),
                    static_cast<double>(actual), rel, max_relative_error);
            print_count++;
          }
        }
      }
    }
  }
  fprintf(stderr, "%s: %s max_rel_error %E at %E\n",
          hwy::TypeName(T(), Lanes(d)).c_str(), name, max_actual_rel_error,
          max_error_value);
  if (count > 0) {
    fprintf(stderr, "%s: %s avg_rel_error %E\n",
            hwy::TypeName(T(), Lanes(d)).c_str(), name,
            sum_rel_error / static_cast<double>(count));
  }
  HWY_ASSERT(max_actual_rel_error <= max_relative_error);
}

struct TestFastLog {
  template <class T, class D>
  HWY_NOINLINE void operator()(T, D d) {
    const double max_relative_error = 9.5E-5;  // SVE: 8.99
    const uint64_t samples = 1000000;
    if (sizeof(T) == 4) {
      TestMathRelative<T, D>("FastLog", std::log, CallFastLog, d,
                             static_cast<T>(FLT_MIN), static_cast<T>(FLT_MAX),
                             max_relative_error, samples);
      TestMathRelative<T, D>("FastLogPositiveNormal", std::log,
                             CallFastLogPositiveNormal, d,
                             static_cast<T>(1.18e-38f), static_cast<T>(FLT_MAX),
                             max_relative_error, samples);

    } else {
      TestMathRelative<T, D>("FastLog", std::log, CallFastLog, d,
                             static_cast<T>(DBL_MIN), static_cast<T>(DBL_MAX),
                             max_relative_error, samples);
      TestMathRelative<T, D>("FastLogPositiveNormal", std::log,
                             CallFastLogPositiveNormal, d,
                             static_cast<T>(2.23e-308), static_cast<T>(DBL_MAX),
                             max_relative_error, samples);
    }
  }
};

struct TestFastExp {
  template <class T, class D>
  HWY_NOINLINE void operator()(T, D d) {
    if (sizeof(T) == 4) {
      // Float Normal Range: [-87.0, +88.0]
      // exp(-87) ~= 1.6e-38 (just above min normal 1.17e-38)
      TestMathRelative<T, D>("FastExpNormal", std::exp, CallFastExp, d,
                             static_cast<T>(-87.0), static_cast<T>(88.0),
                             0.000008, 10'000'000);

      // Float Subnormal Range: [-104.0, -87.0]
      // exp(-104) is very small. Quantization error is expected.
      TestMathRelative<T, D>("FastExpSubnormal", std::exp, CallFastExp, d,
                             static_cast<T>(-104.0), static_cast<T>(-87.0),
                             0.03);
    } else {
      // Double Normal Range: [-708.0, +706.0]
      // exp(-708) ~= 2.2e-308 (min normal 2.22e-308)
      TestMathRelative<T, D>("FastExpNormal", std::exp, CallFastExp, d,
                             static_cast<T>(-708.0), static_cast<T>(706.0),
                             0.000008, 10'000'000);

      // Double Subnormal Range: [-744.0, -708.0]
      // exp(-744) is very small. Quantization error is expected.
      TestMathRelative<T, D>("FastExpSubnormal", std::exp, CallFastExp, d,
                             static_cast<T>(-744.0), static_cast<T>(-708.0),
                             1.4E-4);
    }
  }
};

struct TestFastExp2 {
  template <class T, class D>
  HWY_NOINLINE void operator()(T, D d) {
    if (sizeof(T) == 4) {
      // Float Normal Range: [-126.0, +127.0]
      // exp2(-126) is min normal
      TestMathRelative<T, D>("FastExp2Normal", std::exp2, CallFastExp2, d,
                             static_cast<T>(-126.0), static_cast<T>(127.0),
                             0.000008, 10'000'000);

      // Float Subnormal Range: [-150.0, -126.0]
      TestMathRelative<T, D>("FastExp2Subnormal", std::exp2, CallFastExp2, d,
                             static_cast<T>(-150.0), static_cast<T>(-126.0),
                             0.0009);
    } else {
      // Double Normal Range: [-1022.0, +1023.0]
      TestMathRelative<T, D>("FastExp2Normal", std::exp2, CallFastExp2, d,
                             static_cast<T>(-1022.0), static_cast<T>(1023.0),
                             0.000008, 10'000'000);

      // Double Subnormal Range: [-1075.0, -1022.0]
      TestMathRelative<T, D>("FastExp2Subnormal", std::exp2, CallFastExp2, d,
                             static_cast<T>(-1075.0), static_cast<T>(-1022.0),
                             0.0004);
    }
  }
};

struct TestFastExpMinusOrZero {
  template <class T, class D>
  HWY_NOINLINE void operator()(T, D d) {
    if (sizeof(T) == 4) {
      // Float Normal Range: [-87.0, 0.0]
      TestMathRelative<T, D>("FastExpMinusOrZeroNormal", std::exp,
                             CallFastExpMinusOrZero, d, static_cast<T>(-87.0),
                             static_cast<T>(-0.0), 0.000008, 10'000'000);
    } else {
      // Double Normal Range: [-708.0, 0.0]
      TestMathRelative<T, D>("FastExpMinusOrZeroNormal", std::exp,
                             CallFastExpMinusOrZero, d, static_cast<T>(-708.0),
                             static_cast<T>(-0.0), 0.000008, 10'000'000);
    }
  }
};

struct TestFastLog2 {
  template <class T, class D>
  HWY_NOINLINE void operator()(T, D d) {
    const double max_relative_error = 0.000082;
    const uint64_t samples = 1000000;
    if (sizeof(T) == 4) {
      TestMathRelative<T, D>("FastLog2", std::log2, CallFastLog2, d,
                             static_cast<T>(FLT_MIN), static_cast<T>(FLT_MAX),
                             max_relative_error, samples);
      TestMathRelative<T, D>("FastLog2PositiveNormal", std::log2,
                             CallFastLog2PositiveNormal, d,
                             static_cast<T>(1.18e-38f), static_cast<T>(FLT_MAX),
                             max_relative_error, samples);
    } else {
      TestMathRelative<T, D>("FastLog2", std::log2, CallFastLog2, d,
                             static_cast<T>(DBL_MIN), static_cast<T>(DBL_MAX),
                             max_relative_error, samples);
      TestMathRelative<T, D>("FastLog2PositiveNormal", std::log2,
                             CallFastLog2PositiveNormal, d,
                             static_cast<T>(2.23e-308), static_cast<T>(DBL_MAX),
                             max_relative_error, samples);
    }
  }
};

struct TestFastLog10 {
  template <class T, class D>
  HWY_NOINLINE void operator()(T, D d) {
    const double max_relative_error = 0.000082;
    const uint64_t samples = 1000000;
    if (sizeof(T) == 4) {
      TestMathRelative<T, D>("FastLog10", std::log10, CallFastLog10, d,
                             static_cast<T>(FLT_MIN), static_cast<T>(FLT_MAX),
                             max_relative_error, samples);
      TestMathRelative<T, D>("FastLog10PositiveNormal", std::log10,
                             CallFastLog10PositiveNormal, d,
                             static_cast<T>(1.18e-38f), static_cast<T>(FLT_MAX),
                             max_relative_error, samples);
    } else {
      TestMathRelative<T, D>("FastLog10", std::log10, CallFastLog10, d,
                             static_cast<T>(DBL_MIN), static_cast<T>(DBL_MAX),
                             max_relative_error, samples);
      TestMathRelative<T, D>("FastLog10PositiveNormal", std::log10,
                             CallFastLog10PositiveNormal, d,
                             static_cast<T>(2.23e-308), static_cast<T>(DBL_MAX),
                             max_relative_error, samples);
    }
  }
};

struct TestFastLog1p {
  template <class T, class D>
  HWY_NOINLINE void operator()(T, D d) {
    const double max_relative_error = 0.00009;
    const uint64_t samples = 1000000;
    if (sizeof(T) == 4) {
      TestMathRelative<T, D>("FastLog1p", std::log1p, CallFastLog1p, d,
                             static_cast<T>(-0.9f), static_cast<T>(FLT_MAX),
                             max_relative_error, samples);
      TestMathRelative<T, D>("FastLog1pPositiveNormal", std::log1p,
                             CallFastLog1pPositiveNormal, d,
                             static_cast<T>(0.0f), static_cast<T>(FLT_MAX),
                             max_relative_error, samples);
    } else {
      TestMathRelative<T, D>("FastLog1p", std::log1p, CallFastLog1p, d,
                             static_cast<T>(-0.9), static_cast<T>(DBL_MAX),
                             max_relative_error, samples);
      TestMathRelative<T, D>("FastLog1pPositiveNormal", std::log1p,
                             CallFastLog1pPositiveNormal, d,
                             static_cast<T>(0.0), static_cast<T>(DBL_MAX),
                             max_relative_error, samples);
    }
  }
};

HWY_NOINLINE void TestAllFastExp() {
  ForFloat3264Types(ForPartialVectors<TestFastExp>());
}

HWY_NOINLINE void TestAllFastExp2() {
  ForFloat3264Types(ForPartialVectors<TestFastExp2>());
}

HWY_NOINLINE void TestAllFastExpMinusOrZero() {
  ForFloat3264Types(ForPartialVectors<TestFastExpMinusOrZero>());
}

HWY_NOINLINE void TestAllFastLog() {
  ForFloat3264Types(ForPartialVectors<TestFastLog>());
}

HWY_NOINLINE void TestAllFastLog2() {
  ForFloat3264Types(ForPartialVectors<TestFastLog2>());
}

HWY_NOINLINE void TestAllFastLog10() {
  ForFloat3264Types(ForPartialVectors<TestFastLog10>());
}

HWY_NOINLINE void TestAllFastLog1p() {
  ForFloat3264Types(ForPartialVectors<TestFastLog1p>());
}

struct TestFastPow {
  template <class T, class D>
  HWY_NOINLINE void operator()(T, D d) {
    if (HWY_MATH_TEST_EXCESS_PRECISION) {
      return;
    }

    const T bases[] = {
        static_cast<T>(0.1),   static_cast<T>(0.5),     static_cast<T>(0.99),
        static_cast<T>(1.0),   static_cast<T>(1.0001),  static_cast<T>(1.5),
        static_cast<T>(2.0),   static_cast<T>(2.71828), static_cast<T>(10.0),
        static_cast<T>(100.0), static_cast<T>(1.0e-10), static_cast<T>(1.0e10),
    };

    double max_actual_rel_error = 0.0;
    double max_error_base = 0.0;
    double max_error_exp = 0.0;

    for (T base : bases) {
      T logb = std::log(base);
      T limit = (sizeof(T) == 8) ? static_cast<T>(25.0) : static_cast<T>(25.0);
      T min_exp_val = -limit / logb;
      T max_exp_val = limit / logb;

      if (min_exp_val > max_exp_val) {
        T tmp = min_exp_val;
        min_exp_val = max_exp_val;
        max_exp_val = tmp;
      }

      using UintT = MakeUnsigned<T>;
      const UintT min_bits = BitCastScalar<UintT>(min_exp_val);
      const UintT max_bits = BitCastScalar<UintT>(max_exp_val);

      int range_count = 1;
      UintT ranges[2][2] = {{min_bits, max_bits}, {0, 0}};
      if ((min_exp_val < 0.0) && (max_exp_val > 0.0)) {
        ranges[0][0] = BitCastScalar<UintT>(ConvertScalarTo<T>(+0.0));
        ranges[0][1] = max_bits;
        ranges[1][0] = BitCastScalar<UintT>(ConvertScalarTo<T>(-0.0));
        ranges[1][1] = min_bits;
        range_count = 2;
      } else {
        if (ranges[0][0] > ranges[0][1]) {
          auto tmp = ranges[0][0];
          ranges[0][0] = ranges[0][1];
          ranges[0][1] = tmp;
        }
      }

      const UintT kSamplesPerRange =
          static_cast<UintT>(AdjustedReps(static_cast<size_t>(10000)));
      for (int range_index = 0; range_index < range_count; ++range_index) {
        const UintT start = ranges[range_index][0];
        const UintT stop = ranges[range_index][1];
        const UintT step = HWY_MAX(1, ((stop - start) / kSamplesPerRange));
        for (UintT value_bits = start; value_bits <= stop; value_bits += step) {
          const T exp_val =
              BitCastScalar<T>(HWY_MIN(HWY_MAX(start, value_bits), stop));
          const T actual =
              GetLane(CallFastPow(d, Set(d, base), Set(d, exp_val)));
          const T expected = std::pow(base, exp_val);

#if HWY_TARGET <= HWY_NEON_WITHOUT_AES && HWY_ARCH_ARM_V7
          if ((std::abs(exp_val) < 1e-37f) || (std::abs(expected) < 1e-37f)) {
            continue;
          }
#endif

          if (std::abs(expected) > 0.0) {
            double rel = std::abs(static_cast<double>(actual) -
                                  static_cast<double>(expected)) /
                         std::abs(static_cast<double>(expected));

            if (ScalarIsNaN(rel) || rel > max_actual_rel_error) {
              max_actual_rel_error = rel;
              max_error_base = static_cast<double>(base);
              max_error_exp = static_cast<double>(exp_val);
            }
            if (rel > 0.006) {
              static int print_count = 0;
              if (print_count < 10) {
                fprintf(stderr,
                        "%s: FastPow(%f, %f) expected %E actual %E rel %E max "
                        "rel %E\n",
                        hwy::TypeName(T(), Lanes(d)).c_str(),
                        static_cast<double>(base), static_cast<double>(exp_val),
                        static_cast<double>(expected),
                        static_cast<double>(actual), rel, 0.006);
                print_count++;
              }
            }
          }
        }
      }
    }
    fprintf(stderr, "%s: FastPow max_rel_error %E at base=%E exp=%E\n",
            hwy::TypeName(T(), Lanes(d)).c_str(), max_actual_rel_error,
            max_error_base, max_error_exp);
    HWY_ASSERT(max_actual_rel_error <= 0.006);
  }
};

HWY_NOINLINE void TestAllFastPow() {
  ForFloat3264Types(ForPartialVectors<TestFastPow>());
}

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
namespace {
HWY_BEFORE_TEST(HwyMathTest);
HWY_EXPORT_AND_TEST_P(HwyMathTest, TestAllExp);
HWY_EXPORT_AND_TEST_P(HwyMathTest, TestAllExp2);
HWY_EXPORT_AND_TEST_P(HwyMathTest, TestAllExpm1);
HWY_EXPORT_AND_TEST_P(HwyMathTest, TestAllLog);
HWY_EXPORT_AND_TEST_P(HwyMathTest, TestAllLog10);
HWY_EXPORT_AND_TEST_P(HwyMathTest, TestAllLog1p);
HWY_EXPORT_AND_TEST_P(HwyMathTest, TestAllLog2);
HWY_EXPORT_AND_TEST_P(HwyMathTest, TestAllFastLog);
HWY_EXPORT_AND_TEST_P(HwyMathTest, TestAllFastExp);
HWY_EXPORT_AND_TEST_P(HwyMathTest, TestAllFastExp2);
HWY_EXPORT_AND_TEST_P(HwyMathTest, TestAllFastExpMinusOrZero);
HWY_EXPORT_AND_TEST_P(HwyMathTest, TestAllFastLog2);
HWY_EXPORT_AND_TEST_P(HwyMathTest, TestAllFastLog10);
HWY_EXPORT_AND_TEST_P(HwyMathTest, TestAllFastLog1p);
HWY_EXPORT_AND_TEST_P(HwyMathTest, TestAllFastPow);
HWY_AFTER_TEST();
}  // namespace
}  // namespace hwy
HWY_TEST_MAIN();
#endif  // HWY_ONCE

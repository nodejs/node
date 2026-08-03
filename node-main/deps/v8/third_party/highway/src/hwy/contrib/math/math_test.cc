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

#include "hwy/base.h"

// clang-format off
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "hwy/contrib/math/math_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
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
  if ((min < 0.0) && (max > 0.0)) {
    ranges[0][0] = BitCastScalar<UintT>(ConvertScalarTo<T>(+0.0));
    ranges[0][1] = max_bits;
    ranges[1][0] = BitCastScalar<UintT>(ConvertScalarTo<T>(-0.0));
    ranges[1][1] = min_bits;
    range_count = 2;
  }

  uint64_t max_ulp = 0;
  // Emulation is slower, so cannot afford as many.
  constexpr UintT kSamplesPerRange = static_cast<UintT>(AdjustedReps(4000));
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
                hwy::TypeName(T(), Lanes(d)).c_str(), name, value, expected,
                actual, static_cast<double>(ulp),
                static_cast<uint32_t>(max_error_ulp));
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
#define DEFINE_MATH_TEST(NAME, F32x1, F32xN, F32_MIN, F32_MAX, F32_ERROR, \
                         F64x1, F64xN, F64_MIN, F64_MAX, F64_ERROR)       \
  struct Test##NAME {                                                     \
    template <class T, class D>                                           \
    HWY_NOINLINE void operator()(T, D d) {                                \
      if (sizeof(T) == 4) {                                               \
        TestMath<T, D>(HWY_STR(NAME), F32x1, F32xN, d, F32_MIN, F32_MAX,  \
                       F32_ERROR);                                        \
      } else {                                                            \
        TestMath<T, D>(HWY_STR(NAME), F64x1, F64xN, d,                    \
                       static_cast<T>(F64_MIN), static_cast<T>(F64_MAX),  \
                       F64_ERROR);                                        \
      }                                                                   \
    }                                                                     \
  };                                                                      \
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
  std::log1p, CallLog1p, +0.0f,      +1e37f,      3,  // NEON is 3 instead of 2
  std::log1p, CallLog1p, +0.0,       +DBL_MAX,    2)
DEFINE_MATH_TEST(Log2,
  std::log2,  CallLog2,  +FLT_MIN,   +FLT_MAX,    2,
  std::log2,  CallLog2,  +DBL_MIN,   +DBL_MAX,    2)
// clang-format on

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
HWY_AFTER_TEST();
}  // namespace
}  // namespace hwy
HWY_TEST_MAIN();
#endif  // HWY_ONCE

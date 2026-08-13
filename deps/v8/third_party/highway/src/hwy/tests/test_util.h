// Copyright 2021 Google LLC
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

#ifndef HWY_TESTS_TEST_UTIL_H_
#define HWY_TESTS_TEST_UTIL_H_

// Target-independent helper functions for use by *_test.cc.

#include <stdio.h>
#include <string.h>

#include <cmath>  // std::isnan
#include <string>

#include "hwy/base.h"
#include "hwy/nanobenchmark.h"
#include "hwy/print.h"

namespace hwy {

// The maximum vector size used in tests when defining test data. DEPRECATED.
HWY_MAYBE_UNUSED constexpr size_t kTestMaxVectorSize = 64;

// For tests that involve loops, adjust the trip count so that emulated tests
// finish quickly (but always at least 2 iterations to ensure some diversity).
constexpr size_t AdjustedReps(size_t max_reps) {
#if HWY_ARCH_RISCV
  return HWY_MAX(max_reps / 32, 2);
#elif HWY_IS_DEBUG_BUILD
  return HWY_MAX(max_reps / 8, 2);
#elif HWY_ARCH_ARM
  return HWY_MAX(max_reps / 6, 2);
#elif HWY_COMPILER_MSVC
  return HWY_MAX(max_reps / 2, 2);
#else
  return HWY_MAX(max_reps, 2);
#endif
}

// Same as above, but the loop trip count will be 1 << max_pow2.
constexpr size_t AdjustedLog2Reps(size_t max_pow2) {
  // If "negative" (unsigned wraparound), use original.
#if HWY_ARCH_RISCV
  return HWY_MIN(max_pow2 - 4, max_pow2);
#elif HWY_IS_DEBUG_BUILD
  return HWY_MIN(max_pow2 - 1, max_pow2);
#elif HWY_ARCH_ARM
  return HWY_MIN(max_pow2 - 1, max_pow2);
#else
  return max_pow2;
#endif
}

// 64-bit random generator (Xorshift128+). Much smaller state than std::mt19937,
// which triggers a compiler bug.
class RandomState {
 public:
  explicit RandomState(
      const uint64_t seed = uint64_t{0x123456789} *
                            static_cast<uint64_t>(hwy::Unpredictable1())) {
    s0_ = SplitMix64(seed + 0x9E3779B97F4A7C15ull);
    s1_ = SplitMix64(s0_);
  }

  HWY_INLINE uint64_t operator()() {
    uint64_t s1 = s0_;
    const uint64_t s0 = s1_;
    const uint64_t bits = s1 + s0;
    s0_ = s0;
    s1 ^= s1 << 23;
    s1 ^= s0 ^ (s1 >> 18) ^ (s0 >> 5);
    s1_ = s1;
    return bits;
  }

 private:
  static uint64_t SplitMix64(uint64_t z) {
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }

  uint64_t s0_;
  uint64_t s1_;
};

static HWY_INLINE uint32_t Random32(RandomState* rng) {
  return static_cast<uint32_t>((*rng)());
}

static HWY_INLINE uint64_t Random64(RandomState* rng) { return (*rng)(); }

template <class T, HWY_IF_FLOAT_OR_SPECIAL(T)>
static HWY_INLINE T RandomFiniteValue(RandomState* rng) {
  const uint64_t rand_bits = Random64(rng);

  using TU = MakeUnsigned<T>;
  constexpr TU kExponentMask = ExponentMask<T>();
  constexpr TU kSignMantMask = static_cast<TU>(~kExponentMask);
  constexpr TU kMaxExpField = static_cast<TU>(MaxExponentField<T>());
  constexpr int kNumOfMantBits = MantissaBits<T>();

  const TU orig_exp_field_val =
      static_cast<TU>((rand_bits >> kNumOfMantBits) & kMaxExpField);

  const TU sign_mant_bits = static_cast<TU>(rand_bits & kSignMantMask);
  const TU exp_bits =
      static_cast<TU>(HWY_MIN(HWY_MAX(orig_exp_field_val, 1), kMaxExpField - 1)
                      << kNumOfMantBits);

  return BitCastScalar<T>(static_cast<TU>(sign_mant_bits | exp_bits));
}

template <class T, HWY_IF_NOT_FLOAT_NOR_SPECIAL(T)>
static HWY_INLINE T RandomFiniteValue(RandomState* rng) {
  using TU = MakeUnsigned<T>;
  return static_cast<T>(Random64(rng) & LimitsMax<TU>());
}

HWY_TEST_DLLEXPORT bool BytesEqual(const void* p1, const void* p2, size_t size,
                                   size_t* pos = nullptr);

void AssertStringEqual(const char* expected, const char* actual,
                       const char* target_name, const char* filename, int line);

namespace detail {

template <typename T, typename TU = MakeUnsigned<T>>
TU ComputeUlpDelta(const T expected, const T actual) {
  // Handle -0 == 0 and infinities.
  if (expected == actual) return 0;

  // Consider "equal" if both are NaN, so we can verify an expected NaN.
  // Needs a special case because there are many possible NaN representations.
  if (std::isnan(expected) && std::isnan(actual)) return 0;

  // Compute the difference in units of last place. We do not need to check for
  // differing signs; they will result in large differences, which is fine.
  TU ux, uy;
  CopySameSize(&expected, &ux);
  CopySameSize(&actual, &uy);

  // Avoid unsigned->signed cast: 2's complement is only guaranteed by C++20.
  const TU ulp = HWY_MAX(ux, uy) - HWY_MIN(ux, uy);
  return ulp;
}

HWY_TEST_DLLEXPORT bool IsEqual(const TypeInfo& info, const void* expected_ptr,
                                const void* actual_ptr);

HWY_TEST_DLLEXPORT HWY_NORETURN void PrintMismatchAndAbort(
    const TypeInfo& info, const void* expected_ptr, const void* actual_ptr,
    const char* target_name, const char* filename, int line, size_t lane = 0,
    size_t num_lanes = 1);

HWY_TEST_DLLEXPORT void AssertArrayEqual(const TypeInfo& info,
                                         const void* expected_void,
                                         const void* actual_void, size_t N,
                                         const char* target_name,
                                         const char* filename, int line);

}  // namespace detail

#if HWY_ARCH_ARM_A64 && \
    (HWY_COMPILER_GCC_ACTUAL && HWY_COMPILER_GCC_ACTUAL < 1600)
// The N argument to TypeName from Lanes() is a "poly constant" which triggers
// a GCC bug that can be worked around by disabling cloning. See #2813.
#define HWY_TYPENAME_ATTR __attribute__((noclone))
#else
#define HWY_TYPENAME_ATTR
#endif  // HWY_ARCH_ARM_A64 && HWY_COMPILER_GCC_ACTUAL

// Returns a name for the vector/part/scalar. The type prefix is u/i/f for
// unsigned/signed/floating point, followed by the number of bits per lane;
// then 'x' followed by the number of lanes. Example: u8x16. This is useful for
// understanding which instantiation of a generic test failed.
template <typename T>
HWY_TYPENAME_ATTR std::string TypeName(T /*unused*/, size_t N) {
  char string100[100];
  detail::TypeName(detail::MakeTypeInfo<T>(), N, string100);
  return string100;
}

// Type large enough to hold either value, to which we cast for comparison.
template <typename T1, typename T2>
using LargestType = If<IsFloat<T1>() || IsFloat<T2>(),
                       FloatFromSize<HWY_MAX(sizeof(T1), sizeof(T2))>,
                       If<IsSigned<T1>() || IsSigned<T2>(),
                          SignedFromSize<HWY_MAX(sizeof(T1), sizeof(T2))>,
                          UnsignedFromSize<HWY_MAX(sizeof(T1), sizeof(T2))>>>;

// TTo is the lane type of the actual value and T is an often but not
// necessarily larger type of the expected value. Especially for 8-bit lanes
// initialized via Iota, the actual value often wraps around. To ensure it still
// compares equal to the expected value, wrap integers.
// 1) < 64-bit integer: mask
template <typename TTo, typename T, HWY_IF_NOT_FLOAT_NOR_SPECIAL(TTo),
          HWY_IF_T_SIZE_LE(TTo, 4)>
T WrapTo(T value) {
  return static_cast<T>(static_cast<uint64_t>(value) &
                        ((uint64_t{1} << (sizeof(TTo) * 8)) - 1));
}
// 2) 64-bit integer: no mask (shift would overflow)
template <typename TTo, typename T, HWY_IF_NOT_FLOAT_NOR_SPECIAL(TTo),
          HWY_IF_T_SIZE_GT(TTo, 4)>
T WrapTo(T value) {
  return value;
}
// 3) float or special: do nothing because their value range is sufficient for
// Iota for any vector length.
template <typename TTo, typename T, HWY_IF_FLOAT_OR_SPECIAL(TTo)>
T WrapTo(T value) {
  return value;
}

// Compare non-vector, non-string T, after promoting to the largest type.
template <typename TExpected, typename TActual>
HWY_INLINE bool IsEqual(const TExpected texpected, const TActual actual) {
  const TActual expected = ConvertScalarTo<TActual>(WrapTo<TActual>(texpected));
  const auto info = detail::MakeTypeInfo<TActual>();
  return detail::IsEqual(info, &expected, &actual);
}

template <typename TExpected, typename TActual>
HWY_INLINE void AssertEqual(const TExpected texpected, const TActual actual,
                            const char* target_name, const char* filename,
                            int line, size_t lane = 0) {
  const TActual expected = ConvertScalarTo<TActual>(WrapTo<TActual>(texpected));
  const auto info = detail::MakeTypeInfo<TActual>();
  if (!detail::IsEqual(info, &expected, &actual)) {
    detail::PrintMismatchAndAbort(info, &expected, &actual, target_name,
                                  filename, line, lane);
  }
}

template <typename T>
HWY_INLINE void AssertArrayEqual(const T* expected, const T* actual,
                                 size_t count, const char* target_name,
                                 const char* filename, int line) {
  const auto info = hwy::detail::MakeTypeInfo<T>();
  detail::AssertArrayEqual(info, expected, actual, count, target_name, filename,
                           line);
}

namespace internal {
// Returns number of mismatches.
template <typename T>
HWY_INLINE bool CompareArraySimilarAndMaybeAbort(const T* expected,
                                                 const T* actual, size_t count,
                                                 double tolerance,
                                                 const char* target_name,
                                                 const char* filename, int line,
                                                 bool abort_if_mismatch) {
  size_t num_mismatches = 0;
  for (size_t i = 0; i < count; ++i) {
    const double exp = ConvertScalarTo<double>(expected[i]);
    const double act = ConvertScalarTo<double>(actual[i]);
    const double l1 = ScalarAbs(act - exp);
    const double max_l1 = HWY_MAX(tolerance, tolerance * ScalarAbs(exp));
    if (l1 > max_l1) {
      if (++num_mismatches == 1) {  // Only print once to reduce clutter.
        const size_t begin = i >= 3 ? i - 3 : 0;
        const size_t end = HWY_MIN(i + 4, count);
        fprintf(stderr, "\nFirst mismatch at %zu of %zu:", i, count);
        fprintf(stderr, "\nExpected [%zu, %zu): ", begin, end);
        for (size_t k = begin; k < end; ++k) {
          fprintf(stderr, "%f ", ConvertScalarTo<double>(expected[k]));
        }
        fprintf(stderr, "\n  Actual [%zu, %zu): ", begin, end);
        for (size_t k = begin; k < end; ++k) {
          fprintf(stderr, "%f ", ConvertScalarTo<double>(actual[k]));
        }
        fprintf(stderr, "\n");

        const char* format = "%s %s:%d %s: %E != %E (l1 %E tol %E max_l1 %E)\n";
        if (abort_if_mismatch) {
          HWY_ABORT(format, target_name, filename, line,
                    TypeName(T(), 1).c_str(), exp, act, l1, tolerance, max_l1);
        } else {
          HWY_WARN(format, target_name, filename, line,
                   TypeName(T(), 1).c_str(), exp, act, l1, tolerance, max_l1);
        }
      }  // first mismatch
    }  // mismatch
  }  // for i
  return num_mismatches;
}
}  // namespace internal

// Compares with internally chosen tolerance (maximum L1 or relative error).
template <typename T>
HWY_INLINE void AssertArraySimilar(const T* expected, const T* actual,
                                   size_t count, const char* target_name,
                                   const char* filename, int line) {
  const double mul = hwy::IsSame<RemoveCvRef<T>, float16_t>() ? 128.0 : 1.0;
  const double tolerance = mul * ConvertScalarTo<double>(Epsilon<T>());

  (void)internal::CompareArraySimilarAndMaybeAbort(
      expected, actual, count, tolerance, target_name, filename, line, true);
}

// Compares with specified tolerance (maximum L1 or relative error): a good
// starting point is 10 * Epsilon<T>() for simple arithmetic, or perhaps
// 100 * Epsilon<T>() more complex algorithms or transcendentals.
template <typename T>
HWY_INLINE bool CompareArraySimilar(const T* expected, const T* actual,
                                    size_t count, double tolerance,
                                    const char* target_name,
                                    const char* filename, int line) {
  const size_t num_mismatches = internal::CompareArraySimilarAndMaybeAbort(
      expected, actual, count, tolerance, target_name, filename, line, false);
  if (num_mismatches > 0) {
    fprintf(stderr, "CompareArraySimilar: %zu mismatches\n", num_mismatches);
    return false;
  }
  return true;
}

}  // namespace hwy

#endif  // HWY_TESTS_TEST_UTIL_H_

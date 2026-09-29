//===-- Common header for fmod implementations ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_GENERIC_FMOD_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_GENERIC_FMOD_H

#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY

namespace LIBC_NAMESPACE_DECL {
namespace fputil {
namespace generic {

//  Objective:
//    The  algorithm uses  integer arithmetic  (max uint64_t)  for general case.
//    Some common  cases, like  abs(x) < abs(y)  or  abs(x) < 1000 *  abs(y) are
//    treated specially to increase  performance.  The part of checking  special
//    cases, numbers NaN, INF etc. treated separately.
//
//  Objective:
//    1) FMod definition (https://cplusplus.com/reference/cmath/fmod/):
//       fmod = numer - tquot * denom, where tquot is the truncated
//       (i.e., rounded towards zero) result of: numer/denom.
//    2) FMod with negative x and/or y can be trivially converted to fmod for
//       positive x and y. Therefore the algorithm below works only with
//       positive numbers.
//    3) All positive floating point numbers can be represented as m * 2^e,
//       where "m" is positive integer and "e" is signed.
//    4) FMod function can be calculated in integer numbers (x > y):
//         fmod = m_x * 2^e_x - tquot * m_y * 2^e_y
//              = 2^e_y * (m_x * 2^(e_x - e^y) - tquot * m_y).
//       All variables in parentheses are unsigned integers.
//
//  Mathematical background:
//    Input x,y in the algorithm is represented (mathematically) like m_x*2^e_x
//    and m_y*2^e_y. This is an ambiguous number representation. For example:
//      m * 2^e = (2 * m) * 2^(e-1)
//    The algorithm uses the facts that
//      r = a % b = (a % (N * b)) % b,
//      (a * c) % (b * c) = (a % b) * c
//    where N is positive  integer number. a, b and c - positive. Let's  adopt
//    the formula for representation above.
//      a = m_x * 2^e_x, b = m_y * 2^e_y, N = 2^k
//      r(k) = a % b = (m_x * 2^e_x) % (2^k * m_y * 2^e_y)
//           = 2^(e_y + k) * (m_x * 2^(e_x - e_y - k) % m_y)
//      r(k) = m_r * 2^e_r = (m_x % m_y) * 2^(m_y + k)
//           = (2^p * (m_x % m_y) * 2^(e_y + k - p))
//        m_r = 2^p * (m_x % m_y), e_r = m_y + k - p
//
//  Algorithm description:
//  First, let write x = m_x * 2^e_x and y = m_y * 2^e_y with m_x, m_y, e_x, e_y
//  are integers (m_x amd m_y positive).
//  Then the naive  implementation of the fmod function with a simple
//  for/while loop:
//      while (e_x > e_y) {
//        m_x *= 2; --e_x; //  m_x * 2^e_x == 2 * m_x * 2^(e_x - 1)
//        m_x %= m_y;
//      }
//  On the other hand, the algorithm exploits the fact that m_x, m_y are the
//  mantissas of floating point numbers, which use less bits than the storage
//  integers: 24 / 32 for floats and 53 / 64 for doubles, so if in each step of
//  the iteration, we can left shift m_x as many bits as the storage integer
//  type can hold, the exponent reduction per step will be at least 32 - 24 = 8
//  for floats and 64 - 53 = 11 for doubles (double example below):
//      while (e_x > e_y) {
//        m_x <<= 11; e_x -= 11; //  m_x * 2^e_x == 2^11 * m_x * 2^(e_x - 11)
//        m_x %= m_y;
//      }
//  Some extra improvements are done:
//    1) Shift m_y maximum to the right, which can significantly improve
//       performance for small integer numbers (y = 3 for example).
//       The m_x shift in the loop can be 62 instead of 11 for double.
//    2) For some architectures with very slow division, it can be better to
//       calculate inverse value ones, and after do multiplication in the loop.
//    3) "likely" special cases are treated specially to improve performance.
//
//  Simple example:
//  The examples below use byte for simplicity.
//    1) Shift hy maximum to right without losing bits and increase iy value
//       m_y = 0b00101100 e_y = 20 after shift m_y = 0b00001011 e_y = 22.
//    2) m_x = m_x % m_y.
//    3) Move m_x maximum to left. Note that after (m_x = m_x % m_y) CLZ in m_x
//    is not lower than CLZ in m_y. m_x=0b00001001 e_x = 100, m_x=0b10010000,
//       e_x = 100-4 = 96.
//    4) Repeat (2) until e_x == e_y.
//
//  Complexity analysis (double):
//    Converting x,y to (m_x,e_x),(m_y, e_y): CTZ/shift/AND/OR/if. Loop  count:
//      (m_x - m_y) / (64 -  "length of m_y").
//      max("length of m_y")  = 53,
//      max(e_x - e_y)  = 2048
//    Maximum operation is  186. For rare "unrealistic" cases.
//
//  Special cases (double):
//    Supposing  that  case  where |y| > 1e-292 and |x/y|<2000  is  very  common
//    special processing is implemented. No m_y alignment, no loop:
//      result = (m_x * 2^(e_x - e_y)) % m_y.
//    When x and y are both subnormal (rare case but...) the
//      result = m_x % m_y.
//    Simplified conversion back to double.

// Exceptional cases handler according to cppreference.com
//    https://en.cppreference.com/w/cpp/numeric/math/fmod
// and POSIX standard described in Linux man
//   https://man7.org/linux/man-pages/man3/fmod.3p.html
// C standard for the function is not full, so not by default (although it can
// be implemented in another handler.
// Signaling NaN converted to quiet NaN with FE_INVALID exception.
//    https://www.open-std.org/JTC1/SC22/WG14/www/docs/n1011.htm
template <typename T> struct FModDivisionSimpleHelper {
  LIBC_INLINE constexpr static T execute(int exp_diff, int sides_zeroes_count,
                                         T m_x, T m_y) {
    while (exp_diff > sides_zeroes_count) {
      exp_diff -= sides_zeroes_count;
      m_x <<= sides_zeroes_count;
      m_x %= m_y;
    }
    m_x <<= exp_diff;
    m_x %= m_y;
    return m_x;
  }
};

template <typename T> struct FModDivisionInvMultHelper {
  LIBC_INLINE constexpr static T execute(int exp_diff, int sides_zeroes_count,
                                         T m_x, T m_y) {
    constexpr int LENGTH = sizeof(T) * CHAR_BIT;
    if (exp_diff > sides_zeroes_count) {
      T inv_hy = (cpp::numeric_limits<T>::max() / m_y);
      while (exp_diff > sides_zeroes_count) {
        exp_diff -= sides_zeroes_count;
        T hd = (m_x * inv_hy) >> (LENGTH - sides_zeroes_count);
        m_x <<= sides_zeroes_count;
        m_x -= hd * m_y;
        while (LIBC_UNLIKELY(m_x > m_y))
          m_x -= m_y;
      }
      T hd = (m_x * inv_hy) >> (LENGTH - exp_diff);
      m_x <<= exp_diff;
      m_x -= hd * m_y;
      while (LIBC_UNLIKELY(m_x > m_y))
        m_x -= m_y;
    } else {
      m_x <<= exp_diff;
      m_x %= m_y;
    }
    return m_x;
  }
};

template <typename T, typename U = typename FPBits<T>::StorageType,
          typename DivisionHelper = FModDivisionSimpleHelper<U>>
class FMod {
  static_assert(cpp::is_floating_point_v<T> &&
                    is_unsigned_integral_or_big_int_v<U> &&
                    (sizeof(U) * CHAR_BIT > FPBits<T>::FRACTION_LEN),
                "FMod instantiated with invalid type.");

private:
  using FPB = FPBits<T>;
  using StorageType = typename FPB::StorageType;

  LIBC_INLINE static constexpr bool pre_check(T x, T y, T &out) {
    const T quiet_nan = FPB::quiet_nan().get_val();
    FPB sx(x), sy(y);
    if (LIBC_LIKELY(!sy.is_zero() && !sy.is_inf_or_nan() &&
                    !sx.is_inf_or_nan()))
      return false;

    if (sx.is_nan() || sy.is_nan()) {
      if (sx.is_signaling_nan() || sy.is_signaling_nan())
        fputil::raise_except_if_required(FE_INVALID);
      out = quiet_nan;
      return true;
    }

    if (sx.is_inf() || sy.is_zero()) {
      fputil::raise_except_if_required(FE_INVALID);
      fputil::set_errno_if_required(EDOM);
      out = quiet_nan;
      return true;
    }

    out = x;
    return true;
  }

  LIBC_INLINE static constexpr FPB eval_internal(FPB sx, FPB sy) {

    if (LIBC_LIKELY(sx.uintval() <= sy.uintval())) {
      if (sx.uintval() < sy.uintval())
        return sx;        // |x|<|y| return x
      return FPB::zero(); // |x|=|y| return 0.0
    }

    int e_x = sx.get_biased_exponent();
    int e_y = sy.get_biased_exponent();

    // Most common case where |y| is "very normal" and |x/y| < 2^EXP_LEN
    if (LIBC_LIKELY(e_y > int(FPB::FRACTION_LEN) &&
                    e_x - e_y <= int(FPB::EXP_LEN))) {
      StorageType m_x = sx.get_explicit_mantissa();
      StorageType m_y = sy.get_explicit_mantissa();
      StorageType d = (e_x == e_y)
                          ? (m_x - m_y)
                          : static_cast<StorageType>(m_x << (e_x - e_y)) % m_y;
      if (d == 0)
        return FPB::zero();
      // iy - 1 because of "zero power" for number with power 1
      return FPB::make_value(d, e_y - 1);
    }
    // Both subnormal special case.
    if (LIBC_UNLIKELY(e_x == 0 && e_y == 0)) {
      FPB d;
      d.set_mantissa(sx.uintval() % sy.uintval());
      return d;
    }

    // Note that hx is not subnormal by conditions above.
    U m_x = static_cast<U>(sx.get_explicit_mantissa());
    e_x--;

    U m_y = static_cast<U>(sy.get_explicit_mantissa());
    constexpr int DEFAULT_LEAD_ZEROS =
        sizeof(U) * CHAR_BIT - FPB::FRACTION_LEN - 1;
    int lead_zeros_m_y = DEFAULT_LEAD_ZEROS;
    if (LIBC_LIKELY(e_y > 0)) {
      e_y--;
    } else {
      m_y = static_cast<U>(sy.get_mantissa());
      lead_zeros_m_y = cpp::countl_zero(m_y);
    }

    // Assume hy != 0
    int tail_zeros_m_y = cpp::countr_zero(m_y);
    int sides_zeroes_count = lead_zeros_m_y + tail_zeros_m_y;
    // n > 0 by conditions above
    int exp_diff = e_x - e_y;
    {
      // Shift hy right until the end or n = 0
      int right_shift = exp_diff < tail_zeros_m_y ? exp_diff : tail_zeros_m_y;
      m_y >>= right_shift;
      exp_diff -= right_shift;
      e_y += right_shift;
    }

    {
      // Shift hx left until the end or n = 0
      int left_shift =
          exp_diff < DEFAULT_LEAD_ZEROS ? exp_diff : DEFAULT_LEAD_ZEROS;
      m_x <<= left_shift;
      exp_diff -= left_shift;
    }

    m_x %= m_y;
    if (LIBC_UNLIKELY(m_x == 0))
      return FPB::zero();

    if (exp_diff == 0)
      return FPB::make_value(static_cast<StorageType>(m_x), e_y);

    // hx next can't be 0, because hx < hy, hy % 2 == 1 hx * 2^i % hy != 0
    m_x = DivisionHelper::execute(exp_diff, sides_zeroes_count, m_x, m_y);
    return FPB::make_value(static_cast<StorageType>(m_x), e_y);
  }

public:
  LIBC_INLINE static constexpr T eval(T x, T y) {
    if (T out{}; LIBC_UNLIKELY(pre_check(x, y, out)))
      return out;
    FPB sx(x), sy(y);
    Sign sign = sx.sign();
    sx.set_sign(Sign::POS);
    sy.set_sign(Sign::POS);
    FPB result = eval_internal(sx, sy);
    result.set_sign(sign);
    return result.get_val();
  }
};

} // namespace generic
} // namespace fputil
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_GENERIC_FMOD_H

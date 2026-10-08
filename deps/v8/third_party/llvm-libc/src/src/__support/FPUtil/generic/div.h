//===-- Division of IEEE 754 floating-point numbers -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_GENERIC_DIV_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_GENERIC_DIV_H

#include "hdr/errno_macros.h"
#include "hdr/fenv_macros.h"
#include "src/__support/CPP/algorithm.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/FPUtil/BasicOperations.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/dyadic_float.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {
namespace fputil::generic {

template <typename OutType, typename InType>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_floating_point_v<OutType> &&
                                           cpp::is_floating_point_v<InType> &&
                                           sizeof(OutType) <= sizeof(InType),
                                       OutType>
div(InType x, InType y) {
  using OutFPBits = FPBits<OutType>;
  using OutStorageType = typename OutFPBits::StorageType;
  using InFPBits = FPBits<InType>;
  using InStorageType = typename InFPBits::StorageType;
  using DyadicFloat = DyadicFloat<cpp::max(
      static_cast<size_t>(16),
      cpp::bit_ceil(static_cast<size_t>(InFPBits::SIG_LEN + 1)))>;

  InFPBits x_bits(x);
  InFPBits y_bits(y);

  Sign result_sign = x_bits.sign() == y_bits.sign() ? Sign::POS : Sign::NEG;

  if (LIBC_UNLIKELY(x_bits.is_inf_or_nan() || y_bits.is_inf_or_nan() ||
                    x_bits.is_zero() || y_bits.is_zero())) {
    if (x_bits.is_nan() || y_bits.is_nan()) {
      if (x_bits.is_signaling_nan() || y_bits.is_signaling_nan())
        raise_except_if_required(FE_INVALID);

      if (x_bits.is_quiet_nan()) {
        InStorageType x_payload = x_bits.get_mantissa();
        x_payload >>= InFPBits::FRACTION_LEN - OutFPBits::FRACTION_LEN;
        return OutFPBits::quiet_nan(x_bits.sign(),
                                    static_cast<OutStorageType>(x_payload))
            .get_val();
      }

      if (y_bits.is_quiet_nan()) {
        InStorageType y_payload = y_bits.get_mantissa();
        y_payload >>= InFPBits::FRACTION_LEN - OutFPBits::FRACTION_LEN;
        return OutFPBits::quiet_nan(y_bits.sign(),
                                    static_cast<OutStorageType>(y_payload))
            .get_val();
      }

      return OutFPBits::quiet_nan().get_val();
    }

    if (x_bits.is_inf()) {
      if (y_bits.is_inf()) {
        set_errno_if_required(EDOM);
        raise_except_if_required(FE_INVALID);
        return OutFPBits::quiet_nan().get_val();
      }

      return OutFPBits::inf(result_sign).get_val();
    }

    if (y_bits.is_inf())
      return OutFPBits::zero(result_sign).get_val();

    if (y_bits.is_zero()) {
      if (x_bits.is_zero()) {
        raise_except_if_required(FE_INVALID);
        return OutFPBits::quiet_nan().get_val();
      }

      raise_except_if_required(FE_DIVBYZERO);
      return OutFPBits::inf(result_sign).get_val();
    }

    if (x_bits.is_zero())
      return OutFPBits::zero(result_sign).get_val();
  }

  DyadicFloat xd(x);
  DyadicFloat yd(y);

  // Number of iterations = full output precision + 1 rounding bit + 1 potential
  // leading 0.
  constexpr int NUM_ITERS = OutFPBits::FRACTION_LEN + 3;
  int result_exp = xd.exponent - yd.exponent - (NUM_ITERS - 1);

  InStorageType q = 0;
  InStorageType r = static_cast<InStorageType>(xd.mantissa >> 2);
  InStorageType yd_mant_in = static_cast<InStorageType>(yd.mantissa >> 1);

  for (int i = 0; i < NUM_ITERS; ++i) {
    q <<= 1;
    r <<= 1;
    if (r >= yd_mant_in) {
      q += 1;
      r -= yd_mant_in;
    }
  }

  DyadicFloat result(result_sign, result_exp, q);
  result.mantissa |= static_cast<unsigned int>(r != 0);
  return result.template as<OutType, /*ShouldSignalExceptions=*/true>();
}

} // namespace fputil::generic
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_GENERIC_DIV_H

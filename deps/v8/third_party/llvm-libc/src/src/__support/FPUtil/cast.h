//===-- Conversion between floating-point types -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_CAST_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_CAST_H

#include "FPBits.h"
#include "dyadic_float.h"
#include "hdr/fenv_macros.h"
#include "src/__support/CPP/algorithm.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/macros/properties/types.h"

namespace LIBC_NAMESPACE::fputil {

// TODO: Add optimization for known good targets with fast
// float to float16 conversion:
// https://github.com/llvm/llvm-project/issues/133517
template <typename OutType, typename InType>
LIBC_INLINE constexpr cpp::enable_if_t<cpp::is_floating_point_v<OutType> &&
                                           cpp::is_floating_point_v<InType>,
                                       OutType>
cast(InType x) {
  // Casting to the same type is a no-op.
  if constexpr (cpp::is_same_v<InType, OutType>) {
    return x;
  } else {
    if constexpr (cpp::is_same_v<OutType, bfloat16> ||
                  cpp::is_same_v<InType, bfloat16>
#if defined(LIBC_TYPES_HAS_FLOAT16) && !defined(__LIBC_USE_FLOAT16_CONVERSION)
                  || cpp::is_same_v<OutType, float16> ||
                  cpp::is_same_v<InType, float16>
#endif
    ) {
      using InFPBits = FPBits<InType>;
      using InStorageType = typename InFPBits::StorageType;
      using OutFPBits = FPBits<OutType>;
      using OutStorageType = typename OutFPBits::StorageType;

      InFPBits x_bits(x);

      if (x_bits.is_nan()) {
        if (x_bits.is_signaling_nan()) {
          raise_except_if_required(FE_INVALID);
          return OutFPBits::quiet_nan().get_val();
        }

        InStorageType x_mant = x_bits.get_mantissa();
        if (InFPBits::FRACTION_LEN > OutFPBits::FRACTION_LEN)
          x_mant >>= InFPBits::FRACTION_LEN - OutFPBits::FRACTION_LEN;
        return OutFPBits::quiet_nan(x_bits.sign(),
                                    static_cast<OutStorageType>(x_mant))
            .get_val();
      }

      if (x_bits.is_inf())
        return OutFPBits::inf(x_bits.sign()).get_val();

      constexpr size_t MAX_FRACTION_LEN =
          cpp::max(OutFPBits::FRACTION_LEN, InFPBits::FRACTION_LEN);
      DyadicFloat<cpp::bit_ceil(MAX_FRACTION_LEN)> xd(x);
      return xd.template as<OutType, /*ShouldSignalExceptions=*/true>();
    } else {
      return static_cast<OutType>(x);
    }
  }
}

} // namespace LIBC_NAMESPACE::fputil

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_CAST_H

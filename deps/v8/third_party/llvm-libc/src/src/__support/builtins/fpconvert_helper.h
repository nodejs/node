//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Shared float-to-float extend/truncate conversions, rounding to nearest
/// (ties to even) when narrowing.  These mirror compiler-rt's __extend<a><b>2 /
/// __trunc<a><b>2 builtins, so they can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BUILTINS_FPCONVERT_HELPER_H
#define LLVM_LIBC_SRC___SUPPORT_BUILTINS_FPCONVERT_HELPER_H

#include "hdr/fenv_macros.h"
#include "src/__support/CPP/algorithm.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/dyadic_float.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace builtins {

// TODO: use fputil::cast after adding Float128/64/32/16 classes currently, we
// are using this helper to avoid an infinity loop that happens because
// fputil::cast is calling the builtins if the target doesn't have FPU.
//
// assembly (each body below compiles to a self-call):
//   // double f(float128 x) { return cast<double>(x); }
//   trunc_tf_df:  callq __trunctfdf2@PLT
//   // float128 g(double x) { return cast<float128>(x); }
//   ext_df_tf:    jmp   __extenddftf2@PLT   // TAILCALL

// Convert the floating-point value x from From to To (extend or truncate).
// Narrowing rounds to nearest, ties to even; mirrors compiler-rt __extend* /
// __trunc*.
template <typename To, typename From>
LIBC_INLINE constexpr To fpconvert(From x) {
  using FromBits = fputil::FPBits<From>;
  using ToBits = fputil::FPBits<To>;
  using ToStorageType = typename ToBits::StorageType;

  FromBits x_bits(x);

  if constexpr (cpp::is_same_v<To, From>)
    return x;

  if (x_bits.is_nan()) {
    typename FromBits::StorageType x_frac = x_bits.get_mantissa();
    if constexpr (ToBits::FRACTION_LEN >= FromBits::FRACTION_LEN) {
      ToStorageType to_frac =
          static_cast<ToStorageType>(x_frac)
          << (ToBits::FRACTION_LEN - FromBits::FRACTION_LEN);
      return ToBits::signaling_nan(x_bits.sign(), to_frac).get_val();
    }
    ToStorageType to_frac = static_cast<ToStorageType>(
        x_frac >> (FromBits::FRACTION_LEN - ToBits::FRACTION_LEN));
    return ToBits::quiet_nan(x_bits.sign(), to_frac).get_val();
  }

  if (x_bits.is_inf())
    return ToBits::inf(x_bits.sign()).get_val();

  // Zero and subnormals fall through: DyadicFloat(x) gives a zero mantissa for
  // zero, which as<To>() maps back to a correctly-signed zero.
  constexpr size_t MAX_FRACTION_LEN =
      cpp::max(ToBits::FRACTION_LEN, FromBits::FRACTION_LEN);
  fputil::DyadicFloat<cpp::bit_ceil(MAX_FRACTION_LEN)> xd(x);
  return xd.template as<To, /*ShouldSignalExceptions=*/true>();
}

} // namespace builtins
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_BUILTINS_FPCONVERT_HELPER_H

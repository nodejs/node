//===-- Implementation header for exp10m1f ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_EXP10M1F_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_EXP10M1F_H

#include "exp10f_utils.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/PolyEval.h"
#include "src/__support/FPUtil/except_value_utils.h"
#include "src/__support/FPUtil/multiply_add.h"
#include "src/__support/FPUtil/rounding_mode.h"
#include "src/__support/common.h"
#include "src/__support/libc_errno.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {

namespace math {

namespace exp10m1f_internal {

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
LIBC_INLINE_VAR constexpr size_t N_EXCEPTS_LO = 11;

LIBC_INLINE_VAR constexpr fputil::ExceptValues<float, N_EXCEPTS_LO>
    EXP10M1F_EXCEPTS_LO = {{
        // x = 0x1.0fe54ep-11, exp10m1f(x) = 0x1.3937eep-10 (RZ)
        {0x3a07'f2a7U, 0x3a9c'9bf7U, 1U, 0U, 1U},
        // x = 0x1.80e6eap-11, exp10m1f(x) = 0x1.bb8272p-10 (RZ)
        {0x3a40'7375U, 0x3add'c139U, 1U, 0U, 1U},
        // x = -0x1.2a33bcp-51, exp10m1f(x) = -0x1.57515ep-50 (RZ)
        {0xa615'19deU, 0xa6ab'a8afU, 0U, 1U, 0U},
        // x = -0x0p+0, exp10m1f(x) = -0x0p+0 (RZ)
        {0x8000'0000U, 0x8000'0000U, 0U, 0U, 0U},
        // x = -0x1.b59e08p-31, exp10m1f(x) = -0x1.f7d356p-30 (RZ)
        {0xb05a'cf04U, 0xb0fb'e9abU, 0U, 1U, 1U},
        // x = -0x1.bf342p-12, exp10m1f(x) = -0x1.014e02p-10 (RZ)
        {0xb9df'9a10U, 0xba80'a701U, 0U, 1U, 0U},
        // x = -0x1.6207fp-11, exp10m1f(x) = -0x1.9746cap-10 (RZ)
        {0xba31'03f8U, 0xbacb'a365U, 0U, 1U, 1U},
        // x = -0x1.bd0c66p-11, exp10m1f(x) = -0x1.ffe168p-10 (RZ)
        {0xba5e'8633U, 0xbaff'f0b4U, 0U, 1U, 1U},
        // x = -0x1.ffd84cp-10, exp10m1f(x) = -0x1.25faf2p-8 (RZ)
        {0xbaff'ec26U, 0xbb92'fd79U, 0U, 1U, 0U},
        // x = -0x1.a74172p-9, exp10m1f(x) = -0x1.e57be2p-8 (RZ)
        {0xbb53'a0b9U, 0xbbf2'bdf1U, 0U, 1U, 1U},
        // x = -0x1.cb694cp-9, exp10m1f(x) = -0x1.0764e4p-7 (RZ)
        {0xbb65'b4a6U, 0xbc03'b272U, 0U, 1U, 0U},
    }};

LIBC_INLINE_VAR constexpr size_t N_EXCEPTS_HI = 19;

LIBC_INLINE_VAR constexpr fputil::ExceptValues<float, N_EXCEPTS_HI>
    EXP10M1F_EXCEPTS_HI = {{
        // (input, RZ output, RU offset, RD offset, RN offset)
        // x = 0x1.8d31eep-8, exp10m1f(x) = 0x1.cc7e4cp-7 (RZ)
        {0x3bc6'98f7U, 0x3c66'3f26U, 1U, 0U, 1U},
        // x = 0x1.915fcep-8, exp10m1f(x) = 0x1.d15f72p-7 (RZ)
        {0x3bc8'afe7U, 0x3c68'afb9U, 1U, 0U, 0U},
        // x = 0x1.bcf982p-8, exp10m1f(x) = 0x1.022928p-6 (RZ)
        {0x3bde'7cc1U, 0x3c81'1494U, 1U, 0U, 1U},
        // x = 0x1.99ff0ap-7, exp10m1f(x) = 0x1.dee416p-6 (RZ)
        {0x3c4c'ff85U, 0x3cef'720bU, 1U, 0U, 0U},
        // x = 0x1.75ea14p-6, exp10m1f(x) = 0x1.b9ff16p-5 (RZ)
        {0x3cba'f50aU, 0x3d5c'ff8bU, 1U, 0U, 0U},
        // x = 0x1.f81b64p-6, exp10m1f(x) = 0x1.2cb6bcp-4 (RZ)
        {0x3cfc'0db2U, 0x3d96'5b5eU, 1U, 0U, 0U},
        // x = 0x1.fafecp+3, exp10m1f(x) = 0x1.8c880ap+52 (RZ)
        {0x417d'7f60U, 0x59c6'4405U, 1U, 0U, 0U},
        // x = -0x1.3bf094p-8, exp10m1f(x) = -0x1.69ba4ap-7 (RZ)
        {0xbb9d'f84aU, 0xbc34'dd25U, 0U, 1U, 0U},
        // x = -0x1.4558bcp-8, exp10m1f(x) = -0x1.746fb8p-7 (RZ)
        {0xbba2'ac5eU, 0xbc3a'37dcU, 0U, 1U, 1U},
        // x = -0x1.4bb43p-8, exp10m1f(x) = -0x1.7babe4p-7 (RZ)
        {0xbba5'da18U, 0xbc3d'd5f2U, 0U, 1U, 1U},
        // x = -0x1.776cc8p-8, exp10m1f(x) = -0x1.ad62c4p-7 (RZ)
        {0xbbbb'b664U, 0xbc56'b162U, 0U, 1U, 0U},
        // x = -0x1.f024cp-8, exp10m1f(x) = -0x1.1b20d6p-6 (RZ)
        {0xbbf8'1260U, 0xbc8d'906bU, 0U, 1U, 1U},
        // x = -0x1.f510eep-8, exp10m1f(x) = -0x1.1de9aap-6 (RZ)
        {0xbbfa'8877U, 0xbc8e'f4d5U, 0U, 1U, 0U},
        // x = -0x1.0b43c4p-7, exp10m1f(x) = -0x1.30d418p-6 (RZ)
        {0xbc05'a1e2U, 0xbc98'6a0cU, 0U, 1U, 0U},
        // x = -0x1.245ee4p-7, exp10m1f(x) = -0x1.4d2b86p-6 (RZ)
        {0xbc12'2f72U, 0xbca6'95c3U, 0U, 1U, 0U},
        // x = -0x1.f9f2dap-7, exp10m1f(x) = -0x1.1e2186p-5 (RZ)
        {0xbc7c'f96dU, 0xbd0f'10c3U, 0U, 1U, 0U},
        // x = -0x1.08e42p-6, exp10m1f(x) = -0x1.2b5c4p-5 (RZ)
        {0xbc84'7210U, 0xbd15'ae20U, 0U, 1U, 1U},
        // x = -0x1.0cdc44p-5, exp10m1f(x) = -0x1.2a2152p-4 (RZ)
        {0xbd06'6e22U, 0xbd95'10a9U, 0U, 1U, 1U},
        // x = -0x1.ca4322p-5, exp10m1f(x) = -0x1.ef073p-4 (RZ)
        {0xbd65'2191U, 0xbdf7'8398U, 0U, 1U, 1U},
    }};
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

} // namespace exp10m1f_internal

LIBC_INLINE constexpr float exp10m1f(float x) {
  using namespace exp10m1f_internal;
  using FPBits = fputil::FPBits<float>;
  FPBits xbits(x);

  uint32_t x_u = xbits.uintval();
  uint32_t x_abs = x_u & 0x7fff'ffffU;

  // When x >= log10(2^128), or x is nan
  if (LIBC_UNLIKELY(xbits.is_pos() && x_u >= 0x421a'209bU)) {
    if (xbits.is_finite()) {
      int rounding = fputil::quick_get_round();
      if (rounding == FE_DOWNWARD || rounding == FE_TOWARDZERO)
        return FPBits::max_normal().get_val();

      fputil::set_errno_if_required(ERANGE);
      fputil::raise_except_if_required(FE_OVERFLOW);
    }

    // x >= log10(2^128) and 10^x - 1 rounds to +inf, or x is +inf or nan
    return x + FPBits::inf().get_val();
  }

  // When |x| <= log10(2) * 2^(-6)
  if (LIBC_UNLIKELY(x_abs <= 0x3b9a'209bU)) {
#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
    if (auto r = EXP10M1F_EXCEPTS_LO.lookup(x_u); LIBC_UNLIKELY(r.has_value()))
      return r.value();
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

    double dx = x;
    double dx_sq = dx * dx;
    double c0 = dx * Exp10Base::COEFFS[0];
    double c1 =
        fputil::multiply_add(dx, Exp10Base::COEFFS[2], Exp10Base::COEFFS[1]);
    double c2 =
        fputil::multiply_add(dx, Exp10Base::COEFFS[4], Exp10Base::COEFFS[3]);
    // 10^dx - 1 ~ (1 + COEFFS[0] * dx + ... + COEFFS[4] * dx^5) - 1
    //           = COEFFS[0] * dx + ... + COEFFS[4] * dx^5
    return static_cast<float>(fputil::polyeval(dx_sq, c0, c1, c2));
  }

  // When x <= log10(2^-25), or x is nan
  if (LIBC_UNLIKELY(x_u >= 0xc0f0d2f1)) {
    // exp10m1(-inf) = -1
    if (xbits.is_inf())
      return -1.0f;
    // exp10m1(nan) = nan
    if (xbits.is_nan())
      return x;

    int rounding = fputil::quick_get_round();
    if (rounding == FE_UPWARD || rounding == FE_TOWARDZERO ||
        (rounding == FE_TONEAREST && x_u == 0xc0f0d2f1))
      return -0x1.ffff'fep-1f; // -1.0f + 0x1.0p-24f

    fputil::set_errno_if_required(ERANGE);
    fputil::raise_except_if_required(FE_UNDERFLOW);
    return -1.0f;
  }

  // Exact outputs when x = 1, 2, ..., 10.
  // Quick check mask: 0x800f'ffffU = ~(bits of 1.0f | ... | bits of 10.0f)
  if (LIBC_UNLIKELY((x_u & 0x800f'ffffU) == 0)) {
    switch (x_u) {
    case 0x3f800000U: // x = 1.0f
      return 9.0f;
    case 0x40000000U: // x = 2.0f
      return 99.0f;
    case 0x40400000U: // x = 3.0f
      return 999.0f;
    case 0x40800000U: // x = 4.0f
      return 9'999.0f;
    case 0x40a00000U: // x = 5.0f
      return 99'999.0f;
    case 0x40c00000U: // x = 6.0f
      return 999'999.0f;
    case 0x40e00000U: // x = 7.0f
      return 9'999'999.0f;
    case 0x41000000U: { // x = 8.0f
      int rounding = fputil::quick_get_round();
      if (rounding == FE_UPWARD || rounding == FE_TONEAREST)
        return 100'000'000.0f;
      return 99'999'992.0f;
    }
    case 0x41100000U: { // x = 9.0f
      int rounding = fputil::quick_get_round();
      if (rounding == FE_UPWARD || rounding == FE_TONEAREST)
        return 1'000'000'000.0f;
      return 999'999'936.0f;
    }
    case 0x41200000U: { // x = 10.0f
      int rounding = fputil::quick_get_round();
      if (rounding == FE_UPWARD || rounding == FE_TONEAREST)
        return 10'000'000'000.0f;
      return 9'999'998'976.0f;
    }
    }
  }

#ifndef LIBC_MATH_HAS_SKIP_ACCURATE_PASS
  if (auto r = EXP10M1F_EXCEPTS_HI.lookup(x_u); LIBC_UNLIKELY(r.has_value()))
    return r.value();
#endif // !LIBC_MATH_HAS_SKIP_ACCURATE_PASS

  // Range reduction: 10^x = 2^(mid + hi) * 10^lo
  //   rr = (2^(mid + hi), lo)
  auto rr = exp_b_range_reduc<Exp10Base>(x);

  // The low part is approximated by a degree-5 minimax polynomial.
  // 10^lo ~ 1 + COEFFS[0] * lo + ... + COEFFS[4] * lo^5
  double lo_sq = rr.lo * rr.lo;
  double c0 = fputil::multiply_add(rr.lo, Exp10Base::COEFFS[0], 1.0);
  double c1 =
      fputil::multiply_add(rr.lo, Exp10Base::COEFFS[2], Exp10Base::COEFFS[1]);
  double c2 =
      fputil::multiply_add(rr.lo, Exp10Base::COEFFS[4], Exp10Base::COEFFS[3]);
  double exp10_lo = fputil::polyeval(lo_sq, c0, c1, c2);
  // 10^x - 1 = 2^(mid + hi) * 10^lo - 1
  //          ~ mh * exp10_lo - 1
  return static_cast<float>(fputil::multiply_add(exp10_lo, rr.mh, -1.0));
}

} // namespace math

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_EXP10M1F_H

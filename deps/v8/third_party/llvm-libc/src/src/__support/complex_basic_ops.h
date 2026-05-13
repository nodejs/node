//===-- complex basic operations --------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_COMPLEX_BASIC_OPERATIONS_H
#define LLVM_LIBC_SRC___SUPPORT_COMPLEX_BASIC_OPERATIONS_H

#include "complex_type.h"
#include "src/__support/CPP/bit.h"
#include "src/__support/FPUtil/FPBits.h"

namespace LIBC_NAMESPACE_DECL {

template <typename T> LIBC_INLINE constexpr T conjugate(T c) {
  Complex<make_real_t<T>> c_c = cpp::bit_cast<Complex<make_real_t<T>>>(c);
  c_c.imag = -c_c.imag;
  return cpp::bit_cast<T>(c_c);
}

template <typename T> LIBC_INLINE constexpr T project(T c) {
  using real_t = make_real_t<T>;
  Complex<real_t> c_c = cpp::bit_cast<Complex<real_t>>(c);
  if (fputil::FPBits<real_t>(c_c.real).is_inf() ||
      fputil::FPBits<real_t>(c_c.imag).is_inf())
    return cpp::bit_cast<T>(
        Complex<real_t>{(fputil::FPBits<real_t>::inf(Sign::POS).get_val()),
                        static_cast<real_t>(c_c.imag > 0 ? 0.0 : -0.0)});
  return c;
}

} // namespace LIBC_NAMESPACE_DECL
#endif // LLVM_LIBC_SRC___SUPPORT_COMPLEX_BASIC_OPERATIONS_H

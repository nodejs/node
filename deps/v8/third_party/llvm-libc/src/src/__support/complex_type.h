//===-- complex type --------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_COMPLEX_TYPE_H
#define LLVM_LIBC_SRC___SUPPORT_COMPLEX_TYPE_H

#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/complex_types.h"
#include "src/__support/macros/properties/types.h"

namespace LIBC_NAMESPACE_DECL {
template <typename T> struct Complex {
  T real;
  T imag;
};

template <typename T> struct make_complex;

template <> struct make_complex<float> {
  using type = _Complex float;
};
template <> struct make_complex<double> {
  using type = _Complex double;
};
template <> struct make_complex<long double> {
  using type = _Complex long double;
};

#if defined(LIBC_TYPES_HAS_CFLOAT16)
template <> struct make_complex<float16> {
  using type = cfloat16;
};
#endif
#ifdef LIBC_TYPES_CFLOAT128_IS_NOT_COMPLEX_LONG_DOUBLE
template <> struct make_complex<float128> {
  using type = cfloat128;
};
#endif

template <typename T> using make_complex_t = typename make_complex<T>::type;

template <typename T> struct make_real;

template <> struct make_real<_Complex float> {
  using type = float;
};
template <> struct make_real<_Complex double> {
  using type = double;
};
template <> struct make_real<_Complex long double> {
  using type = long double;
};

#if defined(LIBC_TYPES_HAS_CFLOAT16)
template <> struct make_real<cfloat16> {
  using type = float16;
};
#endif
#ifdef LIBC_TYPES_CFLOAT128_IS_NOT_COMPLEX_LONG_DOUBLE
template <> struct make_real<cfloat128> {
  using type = float128;
};
#endif

template <typename T> using make_real_t = typename make_real<T>::type;

} // namespace LIBC_NAMESPACE_DECL
#endif // LLVM_LIBC_SRC___SUPPORT_COMPLEX_TYPE_H

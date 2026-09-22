//===------ Pretty print function for FPBits --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_FPBITS_STR_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_FPBITS_STR_H

#include "src/__support/CPP/string.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/integer_to_string.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {

namespace details {

// Format T as uppercase hexadecimal number with leading zeros.
template <typename T>
using ZeroPaddedHexFmt = IntegerToString<
    T, typename radix::Hex::WithWidth<(sizeof(T) * 2)>::WithPrefix::Uppercase>;

} // namespace details

// Converts the bits to a string in the following format:
//    "0x<NNN...N> = S: N, E: 0xNNNN, M:0xNNN...N"
// 1. N is a hexadecimal digit.
// 2. The hexadecimal number on the LHS is the raw numerical representation
//    of the bits.
// 3. The exponent is always 16 bits wide irrespective of the type of the
//    floating encoding.
template <typename T> LIBC_INLINE cpp::string str(fputil::FPBits<T> x) {
  using StorageType = typename fputil::FPBits<T>::StorageType;

  if (x.is_nan())
    return "(NaN)";
  if (x.is_inf())
    return x.is_neg() ? "(-Infinity)" : "(+Infinity)";

  const auto sign_char = [](Sign sign) -> char {
    return sign.is_neg() ? '1' : '0';
  };

  cpp::string s;

  const details::ZeroPaddedHexFmt<StorageType> bits(x.uintval());
  s += bits.view();

  s += " = (S: ";
  s += sign_char(x.sign());

  s += ", E: ";
  const details::ZeroPaddedHexFmt<uint16_t> exponent(x.get_biased_exponent());
  s += exponent.view();

  if constexpr (fputil::get_fp_type<T>() == fputil::FPType::X86_Binary80) {
    s += ", I: ";
    s += sign_char(x.get_implicit_bit() ? Sign::NEG : Sign::POS);
  }

  s += ", M: ";
  const details::ZeroPaddedHexFmt<StorageType> mantissa(x.get_mantissa());
  s += mantissa.view();

  s += ')';
  return s;
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_FPBITS_STR_H

//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header exposes LLVM-libc's __extendhfdf2 implementation as
/// builtins::extendhfdf2 so that it can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BUILTINS_EXTENDHFDF2_H
#define LLVM_LIBC_SRC___SUPPORT_BUILTINS_EXTENDHFDF2_H

#include "hdr/stdint_proxy.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/builtins/fpconvert_helper.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace builtins {

// Extend float16 to double; mirrors compiler-rt's __extendhfdf2.
LIBC_INLINE double extendhfdf2(uint16_t bits) {
  return fpconvert_from_bits<double, fputil::FPType::IEEE754_Binary16>(bits);
}

} // namespace builtins
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_BUILTINS_EXTENDHFDF2_H

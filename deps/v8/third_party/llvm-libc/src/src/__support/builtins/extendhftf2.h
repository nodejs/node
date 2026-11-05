//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header exposes LLVM-libc's __extendhftf2 implementation as
/// builtins::extendhftf2 so that it can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BUILTINS_EXTENDHFTF2_H
#define LLVM_LIBC_SRC___SUPPORT_BUILTINS_EXTENDHFTF2_H

#include "include/llvm-libc-types/float128.h"
#include "src/__support/macros/properties/types.h"

#ifdef LIBC_TYPES_HAS_NATIVE_FLOAT128

#include "hdr/stdint_proxy.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/builtins/fpconvert_helper.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace builtins {

// Extend float16 to float128; mirrors compiler-rt's __extendhftf2.
LIBC_INLINE float128 extendhftf2(uint16_t bits) {
  return fpconvert_from_bits<float128, fputil::FPType::IEEE754_Binary16>(bits);
}

} // namespace builtins
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_NATIVE_FLOAT128

#endif // LLVM_LIBC_SRC___SUPPORT_BUILTINS_EXTENDHFTF2_H

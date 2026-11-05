//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This header exposes LLVM-libc's __trunctfhf2 implementation as
/// builtins::trunctfhf2 so that it can be reused by compiler-rt's builtins.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_BUILTINS_TRUNCTFHF2_H
#define LLVM_LIBC_SRC___SUPPORT_BUILTINS_TRUNCTFHF2_H

#include "include/llvm-libc-types/float128.h"
#include "src/__support/macros/properties/types.h"

#ifdef LIBC_TYPES_HAS_NATIVE_FLOAT128

#include "hdr/stdint_proxy.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/builtins/fpconvert_helper.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace builtins {

// Truncate float128 to float16; mirrors compiler-rt's __trunctfhf2.
LIBC_INLINE uint16_t trunctfhf2(float128 x) {
  return fpconvert_to_bits<fputil::FPType::IEEE754_Binary16>(x);
}

} // namespace builtins
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TYPES_HAS_NATIVE_FLOAT128

#endif // LLVM_LIBC_SRC___SUPPORT_BUILTINS_TRUNCTFHF2_H

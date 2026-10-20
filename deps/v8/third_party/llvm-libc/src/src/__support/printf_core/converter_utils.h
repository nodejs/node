//===-- Shared Converter Utilities for printf -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_CONVERTER_UTILS_H
#define LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_CONVERTER_UTILS_H

#include "src/__support/CPP/limits.h"
#include "src/__support/macros/config.h"
#include "src/__support/printf_core/core_structs.h"

#include <inttypes.h>
#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {
namespace printf_core {

LIBC_INLINE uintmax_t apply_length_modifier(uintmax_t num,
                                            LengthSpec length_spec) {
  auto [lm, bw] = length_spec;
  switch (lm) {
  case LengthModifier::none:
    return num & cpp::numeric_limits<unsigned int>::max();
  case LengthModifier::l:
    return num & cpp::numeric_limits<unsigned long>::max();
  case LengthModifier::ll:
  case LengthModifier::L:
    return num & cpp::numeric_limits<unsigned long long>::max();
  case LengthModifier::h:
    return num & cpp::numeric_limits<unsigned short>::max();
  case LengthModifier::hh:
    return num & cpp::numeric_limits<unsigned char>::max();
  case LengthModifier::z:
    return num & cpp::numeric_limits<size_t>::max();
  case LengthModifier::t:
    // We don't have unsigned ptrdiff so uintptr_t is used, since we need an
    // unsigned type and ptrdiff is usually the same size as a pointer.
    static_assert(sizeof(ptrdiff_t) == sizeof(uintptr_t));
    return num & cpp::numeric_limits<uintptr_t>::max();
  case LengthModifier::j:
    return num; // j is intmax, so no mask is necessary.
#ifndef LIBC_COPT_PRINTF_DISABLE_BITINT
  case LengthModifier::w:
  case LengthModifier::wf: {
    uintmax_t mask;
    if (bw == 0) {
      mask = 0;
    } else if (bw < sizeof(uintmax_t) * CHAR_BIT) {
      mask = (static_cast<uintmax_t>(1) << bw) - 1;
    } else {
      mask = UINTMAX_MAX;
    }
    return num & mask;
  }
#endif // LIBC_COPT_PRINTF_DISABLE_BITINT
#if defined(LIBC_INTERNAL_PRINTF_CONVERT_FLOAT128)
  case LengthModifier::Q: // This case should never happen for integers.
    return num;
#endif // LIBC_INTERNAL_PRINTF_CONVERT_FLOAT128
  }
  __builtin_unreachable();
}

#define RET_IF_RESULT_NEGATIVE(func)                                           \
  {                                                                            \
    int result = (func);                                                       \
    if (result < 0)                                                            \
      return result;                                                           \
  }

// This is used to represent which direction the number should be rounded.
enum class RoundDirection { Up, Down, Even };

} // namespace printf_core
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_CONVERTER_UTILS_H

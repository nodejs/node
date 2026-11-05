//===-- Write integer Converter for printf ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_WRITE_INT_CONVERTER_H
#define LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_WRITE_INT_CONVERTER_H

#include "hdr/types/size_t.h"
#include "src/__support/macros/config.h"
#include "src/__support/printf_core/core_structs.h"

#include <inttypes.h>
#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {
namespace printf_core {

LIBC_INLINE int convert_write_int(size_t written,
                                  LengthModifier conv_length_modifier,
                                  void *conv_val_ptr) {

#ifndef LIBC_COPT_PRINTF_NO_NULLPTR_CHECKS
  // This is an additional check added by LLVM-libc.
  if (conv_val_ptr == nullptr)
    return NULLPTR_WRITE_ERROR;
#endif // LIBC_COPT_PRINTF_NO_NULLPTR_CHECKS

  switch (conv_length_modifier) {
  case LengthModifier::none:
    *reinterpret_cast<int *>(conv_val_ptr) = static_cast<int>(written);
    break;
  case LengthModifier::l:
    *reinterpret_cast<long *>(conv_val_ptr) = written;
    break;
  case LengthModifier::ll:
  case LengthModifier::L:
    *reinterpret_cast<long long *>(conv_val_ptr) = written;
    break;
  case LengthModifier::h:
    *reinterpret_cast<short *>(conv_val_ptr) = static_cast<short>(written);
    break;
  case LengthModifier::hh:
    *reinterpret_cast<signed char *>(conv_val_ptr) =
        static_cast<signed char>(written);
    break;
  case LengthModifier::z:
    *reinterpret_cast<size_t *>(conv_val_ptr) = written;
    break;
  case LengthModifier::t:
    *reinterpret_cast<ptrdiff_t *>(conv_val_ptr) = written;
    break;
  case LengthModifier::j:
#ifndef LIBC_COPT_PRINTF_DISABLE_BITINT
  case LengthModifier::w:
  case LengthModifier::wf:
#endif // LIBC_COPT_PRINTF_DISABLE_BITINT
    *reinterpret_cast<uintmax_t *>(conv_val_ptr) = written;
    break;
#if defined(LIBC_TYPES_HAS_NATIVE_FLOAT128)
  case (LengthModifier::Q): // 'Q' is not valid for integer format; this case
                            // should not be reachable.
    break;
#endif // LIBC_TYPES_HAS_NATIVE_FLOAT128
  }
  return WRITE_OK;
}

} // namespace printf_core
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_WRITE_INT_CONVERTER_H

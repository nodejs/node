//===-- Write integer Converter for printf ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_WRITE_INT_CONVERTER_H
#define LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_WRITE_INT_CONVERTER_H

#include "src/__support/macros/config.h"
#include "src/__support/printf_core/core_structs.h"
#include "src/__support/printf_core/writer.h"

#include <inttypes.h>
#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {
namespace printf_core {

template <WriteMode write_mode>
LIBC_INLINE int convert_write_int(Writer<write_mode> *writer,
                                  const FormatSection &to_conv) {

#ifndef LIBC_COPT_PRINTF_NO_NULLPTR_CHECKS
  // This is an additional check added by LLVM-libc.
  if (to_conv.conv_val_ptr == nullptr)
    return NULLPTR_WRITE_ERROR;
#endif // LIBC_COPT_PRINTF_NO_NULLPTR_CHECKS

  size_t written = writer->get_chars_written();

  switch (to_conv.length_modifier) {
  case LengthModifier::none:
    *reinterpret_cast<int *>(to_conv.conv_val_ptr) = static_cast<int>(written);
    break;
  case LengthModifier::l:
    *reinterpret_cast<long *>(to_conv.conv_val_ptr) = written;
    break;
  case LengthModifier::ll:
  case LengthModifier::L:
    *reinterpret_cast<long long *>(to_conv.conv_val_ptr) = written;
    break;
  case LengthModifier::h:
    *reinterpret_cast<short *>(to_conv.conv_val_ptr) =
        static_cast<short>(written);
    break;
  case LengthModifier::hh:
    *reinterpret_cast<signed char *>(to_conv.conv_val_ptr) =
        static_cast<signed char>(written);
    break;
  case LengthModifier::z:
    *reinterpret_cast<size_t *>(to_conv.conv_val_ptr) = written;
    break;
  case LengthModifier::t:
    *reinterpret_cast<ptrdiff_t *>(to_conv.conv_val_ptr) = written;
    break;
  case LengthModifier::j:
#ifndef LIBC_COPT_PRINTF_DISABLE_BITINT
  case LengthModifier::w:
  case LengthModifier::wf:
#endif // LIBC_COPT_PRINTF_DISABLE_BITINT
    *reinterpret_cast<uintmax_t *>(to_conv.conv_val_ptr) = written;
    break;
#if defined(LIBC_TYPES_HAS_FLOAT128)
  case (LengthModifier::Q): // 'Q' is not valid for integer format; this case
                            // should not be reachable.
    break;
#endif // LIBC_TYPES_HAS_FLOAT128
  }
  return WRITE_OK;
}

} // namespace printf_core
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_WRITE_INT_CONVERTER_H

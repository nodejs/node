//===-- Format specifier converter for printf -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_CONVERTER_H
#define LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_CONVERTER_H

#include "src/__support/macros/config.h"
#include "src/__support/printf_core/core_structs.h"
#include "src/__support/printf_core/printf_config.h"
#include "src/__support/printf_core/strerror_converter.h"
#include "src/__support/printf_core/writer.h"

// This option allows for replacing all of the conversion functions with custom
// replacements. This allows conversions to be replaced at compile time.
#ifndef LIBC_COPT_PRINTF_CONV_ATLAS
#include "src/__support/printf_core/converter_atlas.h"
#else
#include LIBC_COPT_PRINTF_CONV_ATLAS
#endif

#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {
namespace printf_core {

#ifndef LIBC_COPT_PRINTF_DISABLE_FLOAT
LIBC_PRINTF_MODULE((template <WriteMode write_mode>
                    int convert_float(Writer<write_mode> *writer,
                                      const FormatSection &to_conv)),
                   {
                     switch (to_conv.conv_name) {
                     case 'f':
                     case 'F':
                       return convert_float_decimal(writer, to_conv);
                     case 'e':
                     case 'E':
                       return convert_float_dec_exp(writer, to_conv);
                     case 'a':
                     case 'A':
                       return convert_float_hex_exp(writer, to_conv);
                     case 'g':
                     case 'G':
                       return convert_float_dec_auto(writer, to_conv);
                     }
                     __builtin_unreachable();
                   })
#endif // not LIBC_COPT_PRINTF_DISABLE_FLOAT

#ifdef LIBC_PRINTF_DEFINE_MODULES
#define HANDLE_WRITE_MODE(MODE)                                                \
  template int convert_float<WriteMode::MODE>(                                 \
      Writer<WriteMode::MODE> * writer, const FormatSection &to_conv);
#include "src/__support/printf_core/write_modes.def"
#undef HANDLE_WRITE_MODE
#endif // LIBC_PRINTF_DEFINE_MODULES

// convert will call a conversion function to convert the FormatSection into
// its string representation, and then that will write the result to the
// writer.
template <WriteMode write_mode>
int convert(Writer<write_mode> *writer, const FormatSection &to_conv) {
  if (!to_conv.has_conv)
    return writer->write(to_conv.raw_string);

#if !defined(LIBC_COPT_PRINTF_DISABLE_FLOAT) &&                                \
    defined(LIBC_COPT_PRINTF_HEX_LONG_DOUBLE)
  if (to_conv.length_modifier == LengthModifier::L) {
    switch (to_conv.conv_name) {
    case 'f':
    case 'F':
    case 'e':
    case 'E':
    case 'g':
    case 'G':
      return convert_float_hex_exp(writer, to_conv);
    default:
      break;
    }
  }
#endif // LIBC_COPT_PRINTF_DISABLE_FLOAT

  switch (to_conv.conv_name) {
  case '%':
    return writer->write("%");
  case 'c':
    return convert_char(writer, to_conv);
  case 's':
    return convert_string(writer, to_conv);
  case 'd':
  case 'i':
  case 'u':
  case 'o':
  case 'x':
  case 'X':
  case 'b':
  case 'B':
    return convert_int(writer, to_conv);
#ifndef LIBC_COPT_PRINTF_DISABLE_FLOAT
  case 'f':
  case 'F':
  case 'e':
  case 'E':
  case 'a':
  case 'A':
  case 'g':
  case 'G':
    return convert_float(writer, to_conv);
#endif // LIBC_COPT_PRINTF_DISABLE_FLOAT
#ifdef LIBC_INTERNAL_PRINTF_HAS_FIXED_POINT
  case 'r':
  case 'R':
  case 'k':
  case 'K':
    return convert_fixed(writer, to_conv);
#endif // LIBC_INTERNAL_PRINTF_HAS_FIXED_POINT
#ifndef LIBC_COPT_PRINTF_DISABLE_STRERROR
  case 'm':
    return convert_strerror(writer, to_conv);
#endif // LIBC_COPT_PRINTF_DISABLE_STRERROR
#ifndef LIBC_COPT_PRINTF_DISABLE_WRITE_INT
  case 'n':
    return convert_write_int(writer, to_conv);
#endif // LIBC_COPT_PRINTF_DISABLE_WRITE_INT
  case 'p':
    return convert_pointer(writer, to_conv);
  default:
    return writer->write(to_conv.raw_string);
  }
  return -1;
}

} // namespace printf_core
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_CONVERTER_H

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
LIBC_PRINTF_MODULE((template <OverflowMode mode, typename CharT>
                    int convert_float(Writer<mode, CharT> *writer,
                                      const FormatSection<CharT> &to_conv)),
                   {
                     switch (to_conv.conv_name) {
                     case CharT{'f'}:
                     case CharT{'F'}:
                       return convert_float_decimal(writer, to_conv);
                     case CharT{'e'}:
                     case CharT{'E'}:
                       return convert_float_dec_exp(writer, to_conv);
                     case CharT{'a'}:
                     case CharT{'A'}:
                       return convert_float_hex_exp(writer, to_conv);
                     case CharT{'g'}:
                     case CharT{'G'}:
                       return convert_float_dec_auto(writer, to_conv);
                     }
                     __builtin_unreachable();
                   })
#endif // not LIBC_COPT_PRINTF_DISABLE_FLOAT

#ifdef LIBC_PRINTF_DEFINE_MODULES
#define HANDLE_OVERFLOW_MODE(MODE)                                             \
  template int convert_float<OverflowMode::MODE, char>(                        \
      Writer<OverflowMode::MODE, char> * writer,                               \
      const FormatSection<char> &to_conv);
#include "src/__support/printf_core/overflow_modes.def"
#undef HANDLE_OVERFLOW_MODE

#if !defined(LIBC_COPT_PRINTF_DISABLE_WIDE)
#define HANDLE_OVERFLOW_MODE(MODE)                                             \
  template int convert_float<OverflowMode::MODE, wchar_t>(                     \
      Writer<OverflowMode::MODE, wchar_t> * writer,                            \
      const FormatSection<wchar_t> &to_conv);
#include "src/__support/printf_core/overflow_modes.def"
#undef HANDLE_OVERFLOW_MODE
#endif // !LIBC_COPT_PRINTF_DISABLE_WIDE

#endif // LIBC_PRINTF_DEFINE_MODULES

// convert will call a conversion function to convert the FormatSection into
// its string representation, and then that will write the result to the
// writer.
template <OverflowMode mode, typename CharT>
int convert(Writer<mode, CharT> *writer, const FormatSection<CharT> &to_conv) {
  if (!to_conv.has_conv)
    return writer->write(to_conv.raw_string);

#if !defined(LIBC_COPT_PRINTF_DISABLE_FLOAT) &&                                \
    defined(LIBC_COPT_PRINTF_HEX_LONG_DOUBLE)
  if (to_conv.length_modifier == LengthModifier::L) {
    switch (to_conv.conv_name) {
    case CharT{'f'}:
    case CharT{'F'}:
    case CharT{'e'}:
    case CharT{'E'}:
    case CharT{'g'}:
    case CharT{'G'}:
      return convert_float_hex_exp(writer, to_conv);
    default:
      break;
    }
  }
#endif // LIBC_COPT_PRINTF_DISABLE_FLOAT

  switch (to_conv.conv_name) {
  case CharT{'%'}:
    return writer->write(CharT{'%'});
  case CharT{'c'}:
    return convert_character(writer, to_conv);
  case CharT{'s'}:
    return convert_string(writer, to_conv);
  case CharT{'d'}:
  case CharT{'i'}:
  case CharT{'u'}:
  case CharT{'o'}:
  case CharT{'x'}:
  case CharT{'X'}:
  case CharT{'b'}:
  case CharT{'B'}:
    return convert_int(writer, to_conv);
#ifndef LIBC_COPT_PRINTF_DISABLE_FLOAT
  case CharT{'f'}:
  case CharT{'F'}:
  case CharT{'e'}:
  case CharT{'E'}:
  case CharT{'a'}:
  case CharT{'A'}:
  case CharT{'g'}:
  case CharT{'G'}:
    return convert_float(writer, to_conv);
#endif // LIBC_COPT_PRINTF_DISABLE_FLOAT
#ifdef LIBC_INTERNAL_PRINTF_HAS_FIXED_POINT
  case CharT{'r'}:
  case CharT{'R'}:
  case CharT{'k'}:
  case CharT{'K'}:
    return convert_fixed(writer, to_conv);
#endif // LIBC_INTERNAL_PRINTF_HAS_FIXED_POINT
#ifndef LIBC_COPT_PRINTF_DISABLE_STRERROR
  case CharT{'m'}:
    return convert_strerror(writer, to_conv);
#endif // LIBC_COPT_PRINTF_DISABLE_STRERROR
#ifndef LIBC_COPT_PRINTF_DISABLE_WRITE_INT
  case CharT{'n'}:
    return convert_write_int(writer->get_chars_written(),
                             to_conv.length_modifier, to_conv.conv_val_ptr);
#endif // LIBC_COPT_PRINTF_DISABLE_WRITE_INT
  case CharT{'p'}:
    return convert_pointer(writer, to_conv);
  default:
    return writer->write(to_conv.raw_string);
  }
  return -1;
}

} // namespace printf_core
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_CONVERTER_H

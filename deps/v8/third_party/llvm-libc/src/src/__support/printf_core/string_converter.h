//===-- String Converter for printf -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_STRING_CONVERTER_H
#define LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_STRING_CONVERTER_H

#ifndef LIBC_COPT_PRINTF_DISABLE_WIDE
#include "hdr/types/char32_t.h"
#include "hdr/types/char8_t.h"
#include "src/__support/wchar/mbstate.h"
#include "src/__support/wchar/string_converter.h"
#endif // LIBC_COPT_PRINTF_DISABLE_WIDE

#include "src/__support/macros/config.h"
#include "src/__support/printf_core/converter_utils.h"
#include "src/__support/printf_core/core_structs.h"
#include "src/__support/printf_core/writer.h"
#include "src/string/string_utils.h" // string_length

#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {
namespace printf_core {

template <WriteMode write_mode>
LIBC_INLINE int char_writer(Writer<write_mode> *writer,
                            const FormatSection &to_conv) {
  const char *str_ptr = reinterpret_cast<const char *>(to_conv.conv_val_ptr);
  size_t string_len = 0;

#ifndef LIBC_COPT_PRINTF_NO_NULLPTR_CHECKS
  if (str_ptr == nullptr) {
    str_ptr = "(null)";
  }
#endif // LIBC_COPT_PRINTF_NO_NULLPTR_CHECKS

  string_len = internal::string_length(str_ptr);

  if (to_conv.precision >= 0 &&
      static_cast<size_t>(to_conv.precision) < string_len)
    string_len = to_conv.precision;

  size_t padding_spaces = to_conv.min_width > static_cast<int>(string_len)
                              ? to_conv.min_width - string_len
                              : 0;

  // If the padding is on the left side, write the spaces first.
  if (padding_spaces > 0 &&
      (to_conv.flags & FormatFlags::LEFT_JUSTIFIED) == 0) {
    RET_IF_RESULT_NEGATIVE(writer->write(' ', padding_spaces));
  }

  RET_IF_RESULT_NEGATIVE(writer->write({(str_ptr), string_len}));

  // If the padding is on the right side, write the spaces last.
  if (padding_spaces > 0 &&
      (to_conv.flags & FormatFlags::LEFT_JUSTIFIED) != 0) {
    RET_IF_RESULT_NEGATIVE(writer->write(' ', padding_spaces));
  }
  return WRITE_OK;
}

#ifndef LIBC_COPT_PRINTF_DISABLE_WIDE
template <WriteMode write_mode>
LIBC_INLINE int wchar_writer(Writer<write_mode> *writer,
                             const FormatSection &to_conv) {
  size_t string_len = 0;
  const char32_t *wstr_ptr =
      reinterpret_cast<const char32_t *>(to_conv.conv_val_ptr);
  size_t precision =
      to_conv.precision < 0 ? SIZE_MAX : static_cast<size_t>(to_conv.precision);

#ifndef LIBC_COPT_PRINTF_NO_NULLPTR_CHECKS
  if (wstr_ptr == nullptr) {
    wstr_ptr = U"(null)";
  }
#endif // LIBC_COPT_PRINTF_NO_NULLPTR_CHECKS

  internal::mbstate mbstate;

  internal::StringConverter<char32_t> length_counter(wstr_ptr, &mbstate,
                                                     precision);

  for (auto converted = length_counter.pop<char8_t>();
       converted.has_value() && converted.value() != '\0';
       converted = length_counter.pop<char8_t>()) {
    ++string_len;
  }

  size_t padding_spaces = to_conv.min_width > static_cast<int>(string_len)
                              ? to_conv.min_width - string_len
                              : 0;

  // If the padding is on the left side, write the spaces first.
  if (padding_spaces > 0 &&
      (to_conv.flags & FormatFlags::LEFT_JUSTIFIED) == 0) {
    RET_IF_RESULT_NEGATIVE(writer->write(' ', padding_spaces));
  }

  mbstate = internal::mbstate();
  internal::StringConverter<char32_t> out_conv(wstr_ptr, &mbstate, precision);

  for (auto converted = out_conv.pop<char8_t>();
       converted.has_value() && converted.value() != '\0';
       converted = out_conv.pop<char8_t>()) {
    RET_IF_RESULT_NEGATIVE(writer->write(static_cast<char>(converted.value())));
  }

  // If the padding is on the right side, write the spaces last.
  if (padding_spaces > 0 &&
      (to_conv.flags & FormatFlags::LEFT_JUSTIFIED) != 0) {
    RET_IF_RESULT_NEGATIVE(writer->write(' ', padding_spaces));
  }

  return WRITE_OK;
}
#endif // LIBC_COPT_PRINTF_DISABLE_WIDE

template <WriteMode write_mode>
LIBC_INLINE int convert_string(Writer<write_mode> *writer,
                               const FormatSection &to_conv) {
  int ret = 0;
  if (to_conv.length_modifier == LengthModifier::l) {
    // find length and print wide char characters
#ifndef LIBC_COPT_PRINTF_DISABLE_WIDE
    ret = wchar_writer(writer, to_conv);
#else
    ret = char_writer(writer, to_conv);
#endif
  } else {
    ret = char_writer(writer, to_conv);
  }

  return ret;
}

} // namespace printf_core
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_STRING_CONVERTER_H

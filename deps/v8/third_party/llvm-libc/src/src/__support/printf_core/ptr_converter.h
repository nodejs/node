//===-- Pointer Converter for printf ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_PTR_CONVERTER_H
#define LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_PTR_CONVERTER_H

#include "src/__support/macros/config.h"
#include "src/__support/printf_core/core_structs.h"
#include "src/__support/printf_core/int_converter.h"
#include "src/__support/printf_core/string_converter.h"
#include "src/__support/printf_core/writer.h"

namespace LIBC_NAMESPACE_DECL {
namespace printf_core {

template <WriteMode write_mode>
LIBC_INLINE int convert_pointer(Writer<write_mode> *writer,
                                const FormatSection &to_conv) {
  FormatSection new_conv = to_conv;

  if (to_conv.conv_val_ptr == nullptr) {
    constexpr char NULLPTR_STR[] = "(nullptr)";
    new_conv.conv_name = 's';
    new_conv.conv_val_ptr = const_cast<char *>(NULLPTR_STR);
    return convert_string(writer, new_conv);
  }
  new_conv.conv_name = 'x';
  new_conv.flags =
      static_cast<FormatFlags>(to_conv.flags | FormatFlags::ALTERNATE_FORM);
  new_conv.length_modifier = LengthModifier::t;
  new_conv.conv_val_raw = reinterpret_cast<uintptr_t>(to_conv.conv_val_ptr);
  return convert_int(writer, new_conv);
}

} // namespace printf_core
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_PTR_CONVERTER_H

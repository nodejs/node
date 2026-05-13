//===-- Implementation header for wcsnrtombs ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC__SUPPORT_WCHAR_WCSNRTOMBS_H
#define LLVM_LIBC_SRC__SUPPORT_WCHAR_WCSNRTOMBS_H

#include "hdr/types/char32_t.h"
#include "hdr/types/char8_t.h"
#include "hdr/types/size_t.h"
#include "hdr/types/wchar_t.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/__support/wchar/mbstate.h"
#include "src/__support/wchar/string_converter.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

LIBC_INLINE static ErrorOr<size_t>
wcsnrtombs(char *__restrict dest, const wchar_t **__restrict ptr_to_src,
           size_t num_src_widechars, size_t dest_len, mbstate *ps) {
  LIBC_CRASH_ON_NULLPTR(ptr_to_src);
  LIBC_CRASH_ON_NULLPTR(ps);

  CharacterConverter cr(ps);
  if (!cr.isValidState())
    return Error(EINVAL);

  if (dest == nullptr)
    dest_len = SIZE_MAX;

  StringConverter<char32_t> str_conv(
      reinterpret_cast<const char32_t *>(*ptr_to_src), ps, dest_len,
      num_src_widechars);
  size_t dst_idx = 0;
  ErrorOr<char8_t> converted = str_conv.pop<char8_t>();
  while (converted.has_value()) {
    if (dest != nullptr)
      dest[dst_idx] = converted.value();

    if (converted.value() == '\0') {
      if (dest != nullptr)
        *ptr_to_src = nullptr;
      return dst_idx;
    }

    dst_idx++;
    converted = str_conv.pop<char8_t>();
  }

  if (dest != nullptr)
    *ptr_to_src += str_conv.getSourceIndex();

  if (converted.error() == -1) // if we hit conversion limit
    return dst_idx;

  return Error(converted.error());
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC__SUPPORT_WCHAR_WCSNRTOMBS_H

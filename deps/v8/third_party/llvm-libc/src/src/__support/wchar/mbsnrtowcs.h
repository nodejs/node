//===-- Implementation for mbsnrtowcs function ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_WCHAR_MBSNRTOWCS_H
#define LLVM_LIBC_SRC___SUPPORT_WCHAR_MBSNRTOWCS_H

#include "hdr/errno_macros.h"
#include "hdr/types/size_t.h"
#include "hdr/types/wchar_t.h"
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/__support/wchar/character_converter.h"
#include "src/__support/wchar/mbstate.h"
#include "src/__support/wchar/string_converter.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

LIBC_INLINE static ErrorOr<size_t>
mbsnrtowcs(wchar_t *__restrict dst, const char **__restrict src,
           size_t max_src_bytes, size_t max_dst_chars, mbstate *__restrict ps) {
  LIBC_CRASH_ON_NULLPTR(src);
  CharacterConverter char_conv(ps);
  if (!char_conv.isValidState())
    return Error(EINVAL);

  StringConverter<char8_t> str_conv(reinterpret_cast<const char8_t *>(*src), ps,
                                    max_dst_chars, max_src_bytes);
  size_t dst_idx = 0;
  ErrorOr<char32_t> converted = str_conv.pop<char32_t>();
  while (converted.has_value()) {
    if (dst != nullptr)
      dst[dst_idx] = converted.value();
    // null terminator should not be counted in return value
    if (converted.value() == L'\0') {
      if (dst != nullptr)
        *src = nullptr;
      return dst_idx;
    }
    dst_idx++;
    converted = str_conv.pop<char32_t>();
  }

  if (converted.error() == -1) { // if we hit conversion limit
    if (dst != nullptr)
      *src += str_conv.getSourceIndex();
    return dst_idx;
  }

  return Error(converted.error());
}

} // namespace internal

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_WCHAR_MBSNRTOWCS_H

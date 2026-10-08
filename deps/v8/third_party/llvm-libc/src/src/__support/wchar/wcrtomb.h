//===-- Implementation header for wcrtomb ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC__SUPPORT_WCHAR_WCRTOMB_H
#define LLVM_LIBC_SRC__SUPPORT_WCHAR_WCRTOMB_H

#include "src/__support/error_or.h"
#include "src/__support/macros/null_check.h"
#include "src/__support/wchar/character_converter.h"
#include "src/__support/wchar/mbstate.h"

#include "hdr/errno_macros.h"
#include "hdr/types/char32_t.h"
#include "hdr/types/size_t.h"
#include "hdr/types/wchar_t.h"
#include "src/__support/common.h"
#include "src/__support/libc_assert.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

LIBC_INLINE ErrorOr<size_t> wcrtomb(char *__restrict dest_ptr, wchar_t wc,
                                    mbstate *__restrict ps) {
  LIBC_CRASH_ON_NULLPTR(dest_ptr);
  LIBC_CRASH_ON_NULLPTR(ps);
  static_assert(sizeof(wchar_t) == 4);

  CharacterConverter cr(ps);

  if (!cr.isValidState())
    return Error(EINVAL);

  int status = cr.push(static_cast<char32_t>(wc));
  if (status != 0)
    return Error(status);

  size_t count = 0;
  while (!cr.isEmpty()) {
    auto utf8 = cr.pop_utf8(); // can never fail as long as the push succeeded
    LIBC_ASSERT(utf8.has_value());

    *dest_ptr = utf8.value();
    dest_ptr++;
    count++;
  }
  return count;
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC__SUPPORT_WCHAR_WCRTOMB_H

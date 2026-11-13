//===-- Implementation of memrchr -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/string/memrchr.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(void *, memrchr, (const void *src, int c, size_t n)) {

  if (n)
    LIBC_CRASH_ON_NULLPTR(src);

  const unsigned char *str = reinterpret_cast<const unsigned char *>(src);
  const unsigned char ch = static_cast<unsigned char>(c);
  for (; n != 0; --n) {
    const unsigned char *s = str + n - 1;
    if (*s == ch)
      return const_cast<unsigned char *>(s);
  }
  return nullptr;
}

} // namespace LIBC_NAMESPACE_DECL

//===-- Implementation of memchr ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/string/memchr.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/string/string_utils.h"

#include "src/__support/common.h"
#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {

// TODO: Look at performance benefits of comparing words.
LLVM_LIBC_FUNCTION(void *, memchr, (const void *src, int c, size_t n)) {
  if (n)
    LIBC_CRASH_ON_NULLPTR(src);
  return internal::find_first_character(
      reinterpret_cast<const unsigned char *>(src),
      static_cast<unsigned char>(c), n);
}

} // namespace LIBC_NAMESPACE_DECL

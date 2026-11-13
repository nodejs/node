//===-- Implementation of mempcpy ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/string/mempcpy.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/string/memory_utils/inline_memcpy.h"

#include "src/__support/common.h"
#include <stddef.h> // For size_t.

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(void *, mempcpy,
                   (void *__restrict dst, const void *__restrict src,
                    size_t count)) {
  if (count) {
    LIBC_CRASH_ON_NULLPTR(dst);
    LIBC_CRASH_ON_NULLPTR(src);
  }
  inline_memcpy(dst, src, count);
  return reinterpret_cast<char *>(dst) + count;
}

} // namespace LIBC_NAMESPACE_DECL

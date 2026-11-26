//===-- Implementation of strndup -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/string/strndup.h"
#include "src/__support/checked_ptr.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/string/memory_utils/inline_memcpy.h"
#include "src/string/string_utils.h"

#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(char *, strndup, (const char *src, size_t size)) {
  if (src == nullptr)
    return nullptr;
  size_t len = internal::string_length(src);
  if (len > size)
    len = size;
  CheckedPtr<char[]> dest = make_unique_for_overwrite<char[]>(len + 1);
  if (!dest)
    return nullptr;
  inline_memcpy(dest.get(), src, len + 1);
  dest[len] = '\0';
  return dest.release();
}

} // namespace LIBC_NAMESPACE_DECL

//===-- Implementation of strndup -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/string/strndup.h"
#include "src/__support/CPP/new.h"
#include "src/__support/alloc-checker.h"
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
  AllocChecker ac;
  char *dest = new (ac) char[len + 1];
  if (!ac)
    return nullptr;
  inline_memcpy(dest, src, len + 1);
  dest[len] = '\0';
  return dest;
}

} // namespace LIBC_NAMESPACE_DECL

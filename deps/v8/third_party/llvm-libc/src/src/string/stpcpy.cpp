//===-- Implementation of stpcpy ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/string/stpcpy.h"
#include "src/__support/macros/config.h"
#include "src/string/memory_utils/inline_memcpy.h"
#include "src/string/string_utils.h"

#include "src/__support/common.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(char *, stpcpy,
                   (char *__restrict dest, const char *__restrict src)) {
  size_t size = internal::string_length(src) + 1;
  inline_memcpy(dest, src, size);
  char *result = dest + size;

  if (result != nullptr)
    return result - 1;
  return nullptr;
}

} // namespace LIBC_NAMESPACE_DECL

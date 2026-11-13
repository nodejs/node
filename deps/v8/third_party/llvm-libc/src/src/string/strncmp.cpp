//===-- Implementation of strncmp -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/string/strncmp.h"

#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/string/memory_utils/inline_strcmp.h"

#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(int, strncmp,
                   (const char *left, const char *right, size_t n)) {
  if (n) {
    LIBC_CRASH_ON_NULLPTR(left);
    LIBC_CRASH_ON_NULLPTR(right);
  }

  auto comp = [](char l, char r) -> int {
    return static_cast<unsigned char>(l) - static_cast<unsigned char>(r);
  };
  return inline_strncmp(left, right, n, comp);
}

} // namespace LIBC_NAMESPACE_DECL

//===-- Implementation of strtok_r ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/string/strtok_r.h"

#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/string/string_utils.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(char *, strtok_r,
                   (char *__restrict src,
                    const char *__restrict delimiter_string,
                    char **__restrict saveptr)) {
  return internal::string_token(src, delimiter_string, saveptr);
}

} // namespace LIBC_NAMESPACE_DECL

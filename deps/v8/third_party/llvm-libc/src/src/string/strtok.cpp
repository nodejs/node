//===-- Implementation of strtok ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/string/strtok.h"

#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/string/string_utils.h"

namespace LIBC_NAMESPACE_DECL {

static char *strtok_str = nullptr;

LLVM_LIBC_FUNCTION(char *, strtok,
                   (char *__restrict src,
                    const char *__restrict delimiter_string)) {
  return internal::string_token(src, delimiter_string, &strtok_str);
}

} // namespace LIBC_NAMESPACE_DECL

//===-- Implementation of strsep ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/string/strsep.h"

#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/string/string_utils.h"

#include "src/__support/common.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(char *, strsep,
                   (char **__restrict stringp, const char *__restrict delim)) {
  LIBC_CRASH_ON_NULLPTR(stringp);
  if (!*stringp)
    return nullptr;
  LIBC_CRASH_ON_NULLPTR(delim);
  return internal::string_token<false>(*stringp, delim, stringp);
}

} // namespace LIBC_NAMESPACE_DECL

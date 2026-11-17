//===-- Implementation of strchrnul --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/string/strchrnul.h"
#include "src/__support/macros/config.h"
#include "src/string/string_utils.h"

#include "src/__support/common.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(char *, strchrnul, (const char *src, int c)) {
  return internal::strchr_implementation<false>(src, c);
}

} // namespace LIBC_NAMESPACE_DECL

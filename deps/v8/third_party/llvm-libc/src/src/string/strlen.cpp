//===-- Implementation of strlen ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/string/strlen.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/string/string_utils.h"

#include "src/__support/common.h"

namespace LIBC_NAMESPACE_DECL {

// TODO: investigate the performance of this function.
// There might be potential for compiler optimization.
LLVM_LIBC_FUNCTION(size_t, strlen, (const char *src)) {
  LIBC_CRASH_ON_NULLPTR(src);
  return internal::string_length(src);
}

} // namespace LIBC_NAMESPACE_DECL

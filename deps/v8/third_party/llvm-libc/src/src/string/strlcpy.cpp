//===-- Implementation of strlcpy -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/string/strlcpy.h"
#include "src/__support/macros/config.h"
#include "src/string/string_utils.h"

#include "src/__support/common.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(size_t, strlcpy,
                   (char *__restrict dst, const char *__restrict src,
                    size_t size)) {
  return internal::strlcpy(dst, src, size);
}

} // namespace LIBC_NAMESPACE_DECL

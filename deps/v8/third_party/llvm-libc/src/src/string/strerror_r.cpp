//===-- Implementation of strerror_r --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/string/strerror_r.h"
#include "src/__support/StringUtil/error_to_string.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"

#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {

// This is the gnu version of strerror_r. The XSI version may be added later.
LLVM_LIBC_FUNCTION(char *, strerror_r,
                   (int err_num, char *buf, size_t buflen)) {
  return const_cast<char *>(get_error_string(err_num, {buf, buflen}).data());
}

} // namespace LIBC_NAMESPACE_DECL

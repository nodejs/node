//===-- Implementation of strnlen_s ------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/string/strnlen_s.h"
#include "hdr/types/size_t.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/string/string_utils.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(size_t, strnlen_s, (const char *s, size_t n)) {
  return (s != 0) ? internal::strnlen(s, n) : 0;
}

} // namespace LIBC_NAMESPACE_DECL

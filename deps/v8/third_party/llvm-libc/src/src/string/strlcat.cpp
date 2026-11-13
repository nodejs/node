//===-- Implementation of strlcat -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/string/strlcat.h"
#include "src/__support/macros/config.h"
#include "src/string/string_utils.h"

#include "src/__support/common.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(size_t, strlcat,
                   (char *__restrict dst, const char *__restrict src,
                    size_t size)) {
  char *new_dst = reinterpret_cast<char *>(internal::find_first_character(
      reinterpret_cast<unsigned char *>(dst), 0, size));
  if (!new_dst)
    return size + internal::string_length(src);
  size_t first_len = new_dst - dst;
  return first_len + internal::strlcpy(new_dst, src, size - first_len);
}

} // namespace LIBC_NAMESPACE_DECL

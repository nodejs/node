//===-- Implementation of memmove -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/string/memmove.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/string/memory_utils/inline_memcpy.h"
#include "src/string/memory_utils/inline_memmove.h"
#include <stddef.h> // size_t

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(void *, memmove,
                   (void *dst, const void *src, size_t count)) {
  if (count) {
    LIBC_CRASH_ON_NULLPTR(dst);
    LIBC_CRASH_ON_NULLPTR(src);
  }
  // Memmove may handle some small sizes as efficiently as inline_memcpy.
  // For these sizes we may not do is_disjoint check.
  // This both avoids additional code for the most frequent smaller sizes
  // and removes code bloat (we don't need the memcpy logic for small sizes).
  if (inline_memmove_small_size(dst, src, count))
    return dst;
  if (is_disjoint(dst, src, count))
    inline_memcpy(dst, src, count);
  else
    inline_memmove_follow_up(dst, src, count);
  return dst;
}

} // namespace LIBC_NAMESPACE_DECL

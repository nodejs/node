//===-- Memmove implementation for aarch64 ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LIBC_SRC_STRING_MEMORY_UTILS_AARCH64_INLINE_MEMMOVE_H
#define LIBC_SRC_STRING_MEMORY_UTILS_AARCH64_INLINE_MEMMOVE_H

#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/string/memory_utils/op_builtin.h"
#include "src/string/memory_utils/op_generic.h"
#include "src/string/memory_utils/utils.h"

#include <stddef.h> // size_t

namespace LIBC_NAMESPACE_DECL {

using uint128_t = generic_v128;
using uint256_t = generic_v256;
using uint512_t = generic_v512;

LIBC_INLINE void inline_memmove_aarch64(Ptr dst, CPtr src, size_t count) {
  if (count == 0)
    return;
  if (count == 1)
    return generic::Memmove<uint8_t>::block(dst, src);
  if (count <= 4)
    return generic::Memmove<uint16_t>::head_tail(dst, src, count);
  if (count <= 8)
    return generic::Memmove<uint32_t>::head_tail(dst, src, count);
  if (count <= 16)
    return generic::Memmove<uint64_t>::head_tail(dst, src, count);
  if (count <= 32)
    return generic::Memmove<uint128_t>::head_tail(dst, src, count);
  if (count <= 64)
    return generic::Memmove<uint256_t>::head_tail(dst, src, count);
  if (count <= 128)
    return generic::Memmove<uint512_t>::head_tail(dst, src, count);
  if (dst < src) {
    generic::Memmove<uint256_t>::align_forward<Arg::Src>(dst, src, count);
    return generic::Memmove<uint512_t>::loop_and_tail_forward(dst, src, count);
  } else {
    generic::Memmove<uint256_t>::align_backward<Arg::Src>(dst, src, count);
    return generic::Memmove<uint512_t>::loop_and_tail_backward(dst, src, count);
  }
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_SRC_STRING_MEMORY_UTILS_AARCH64_INLINE_MEMMOVE_H

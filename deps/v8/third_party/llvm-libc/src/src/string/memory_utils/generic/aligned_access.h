//===-- Implementations for platform with mandatory aligned memory access -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// For some platforms, unaligned loads and stores are either illegal or very
// slow. The implementations in this file make sure all loads and stores are
// always aligned.
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_GENERIC_ALIGNED_ACCESS_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_GENERIC_ALIGNED_ACCESS_H

#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/string/memory_utils/generic/byte_per_byte.h"
#include "src/string/memory_utils/op_generic.h" // generic::splat
#include "src/string/memory_utils/utils.h"      // Ptr, CPtr

#include <stddef.h> // size_t

namespace LIBC_NAMESPACE_DECL {

[[maybe_unused]] LIBC_INLINE uint32_t load32_aligned(CPtr ptr, size_t offset,
                                                     size_t alignment) {
  if (alignment == 0)
    return load32_aligned<uint32_t>(ptr, offset);
  else if (alignment == 2)
    return load32_aligned<uint16_t, uint16_t>(ptr, offset);
  else // 1, 3
    return load32_aligned<uint8_t, uint16_t, uint8_t>(ptr, offset);
}

[[maybe_unused]] LIBC_INLINE uint64_t load64_aligned(CPtr ptr, size_t offset,
                                                     size_t alignment) {
  if (alignment == 0)
    return load64_aligned<uint64_t>(ptr, offset);
  else if (alignment == 4)
    return load64_aligned<uint32_t, uint32_t>(ptr, offset);
  else if (alignment == 6)
    return load64_aligned<uint16_t, uint32_t, uint16_t>(ptr, offset);
  else if (alignment == 2)
    return load64_aligned<uint16_t, uint16_t, uint16_t, uint16_t>(ptr, offset);
  else // 1, 3, 5, 7
    return load64_aligned<uint8_t, uint16_t, uint16_t, uint16_t, uint8_t>(
        ptr, offset);
}

///////////////////////////////////////////////////////////////////////////////
// memcpy
///////////////////////////////////////////////////////////////////////////////

[[maybe_unused]] LIBC_INLINE void
inline_memcpy_aligned_access_32bit(Ptr __restrict dst, CPtr __restrict src,
                                   size_t count) {
  constexpr size_t kAlign = sizeof(uint32_t);
  if (count <= 2 * kAlign)
    return inline_memcpy_byte_per_byte(dst, src, count);
  size_t bytes_to_dst_align = distance_to_align_up<kAlign>(dst);
  inline_memcpy_byte_per_byte(dst, src, bytes_to_dst_align);
  size_t offset = bytes_to_dst_align;
  size_t src_alignment = distance_to_align_down<kAlign>(src + offset);
  for (; offset < count - kAlign; offset += kAlign) {
    uint32_t value = load32_aligned(src, offset, src_alignment);
    store32_aligned<uint32_t>(value, dst, offset);
  }
  // remainder
  inline_memcpy_byte_per_byte(dst, src, count, offset);
}

[[maybe_unused]] LIBC_INLINE void
inline_memcpy_aligned_access_64bit(Ptr __restrict dst, CPtr __restrict src,
                                   size_t count) {
  constexpr size_t kAlign = sizeof(uint64_t);
  if (count <= 2 * kAlign)
    return inline_memcpy_byte_per_byte(dst, src, count);
  size_t bytes_to_dst_align = distance_to_align_up<kAlign>(dst);
  inline_memcpy_byte_per_byte(dst, src, bytes_to_dst_align);
  size_t offset = bytes_to_dst_align;
  size_t src_alignment = distance_to_align_down<kAlign>(src + offset);
  for (; offset < count - kAlign; offset += kAlign) {
    uint64_t value = load64_aligned(src, offset, src_alignment);
    store64_aligned<uint64_t>(value, dst, offset);
  }
  // remainder
  inline_memcpy_byte_per_byte(dst, src, count, offset);
}

///////////////////////////////////////////////////////////////////////////////
// memset
///////////////////////////////////////////////////////////////////////////////

[[maybe_unused]] LIBC_INLINE static void
inline_memset_aligned_access_32bit(Ptr dst, uint8_t value, size_t count) {
  constexpr size_t kAlign = sizeof(uint32_t);
  if (count <= 2 * kAlign)
    return inline_memset_byte_per_byte(dst, value, count);
  size_t bytes_to_dst_align = distance_to_align_up<kAlign>(dst);
  inline_memset_byte_per_byte(dst, value, bytes_to_dst_align);
  size_t offset = bytes_to_dst_align;
  for (; offset < count - kAlign; offset += kAlign)
    store32_aligned<uint32_t>(generic::splat<uint32_t>(value), dst, offset);
  inline_memset_byte_per_byte(dst, value, count, offset);
}

[[maybe_unused]] LIBC_INLINE static void
inline_memset_aligned_access_64bit(Ptr dst, uint8_t value, size_t count) {
  constexpr size_t kAlign = sizeof(uint64_t);
  if (count <= 2 * kAlign)
    return inline_memset_byte_per_byte(dst, value, count);
  size_t bytes_to_dst_align = distance_to_align_up<kAlign>(dst);
  inline_memset_byte_per_byte(dst, value, bytes_to_dst_align);
  size_t offset = bytes_to_dst_align;
  for (; offset < count - kAlign; offset += kAlign)
    store64_aligned<uint64_t>(generic::splat<uint64_t>(value), dst, offset);
  inline_memset_byte_per_byte(dst, value, count, offset);
}

///////////////////////////////////////////////////////////////////////////////
// bcmp
///////////////////////////////////////////////////////////////////////////////

[[maybe_unused]] LIBC_INLINE BcmpReturnType
inline_bcmp_aligned_access_32bit(CPtr p1, CPtr p2, size_t count) {
  constexpr size_t kAlign = sizeof(uint32_t);
  if (count <= 2 * kAlign)
    return inline_bcmp_byte_per_byte(p1, p2, count);
  size_t bytes_to_p1_align = distance_to_align_up<kAlign>(p1);
  if (auto value = inline_bcmp_byte_per_byte(p1, p2, bytes_to_p1_align))
    return value;
  size_t offset = bytes_to_p1_align;
  size_t p2_alignment = distance_to_align_down<kAlign>(p2 + offset);
  for (; offset < count - kAlign; offset += kAlign) {
    uint32_t a = load32_aligned<uint32_t>(p1, offset);
    uint32_t b = load32_aligned(p2, offset, p2_alignment);
    if (a != b)
      return BcmpReturnType::nonzero();
  }
  return inline_bcmp_byte_per_byte(p1, p2, count, offset);
}

[[maybe_unused]] LIBC_INLINE BcmpReturnType
inline_bcmp_aligned_access_64bit(CPtr p1, CPtr p2, size_t count) {
  constexpr size_t kAlign = sizeof(uint64_t);
  if (count <= 2 * kAlign)
    return inline_bcmp_byte_per_byte(p1, p2, count);
  size_t bytes_to_p1_align = distance_to_align_up<kAlign>(p1);
  if (auto value = inline_bcmp_byte_per_byte(p1, p2, bytes_to_p1_align))
    return value;
  size_t offset = bytes_to_p1_align;
  size_t p2_alignment = distance_to_align_down<kAlign>(p2 + offset);
  for (; offset < count - kAlign; offset += kAlign) {
    uint64_t a = load64_aligned<uint64_t>(p1, offset);
    uint64_t b = load64_aligned(p2, offset, p2_alignment);
    if (a != b)
      return BcmpReturnType::nonzero();
  }
  return inline_bcmp_byte_per_byte(p1, p2, count, offset);
}

///////////////////////////////////////////////////////////////////////////////
// memcmp
///////////////////////////////////////////////////////////////////////////////

[[maybe_unused]] LIBC_INLINE MemcmpReturnType
inline_memcmp_aligned_access_32bit(CPtr p1, CPtr p2, size_t count) {
  constexpr size_t kAlign = sizeof(uint32_t);
  if (count <= 2 * kAlign)
    return inline_memcmp_byte_per_byte(p1, p2, count);
  size_t bytes_to_p1_align = distance_to_align_up<kAlign>(p1);
  if (auto value = inline_memcmp_byte_per_byte(p1, p2, bytes_to_p1_align))
    return value;
  size_t offset = bytes_to_p1_align;
  size_t p2_alignment = distance_to_align_down<kAlign>(p2 + offset);
  for (; offset < count - kAlign; offset += kAlign) {
    uint32_t a = load32_aligned<uint32_t>(p1, offset);
    uint32_t b = load32_aligned(p2, offset, p2_alignment);
    if (a != b)
      return cmp_uint32_t(Endian::to_big_endian(a), Endian::to_big_endian(b));
  }
  return inline_memcmp_byte_per_byte(p1, p2, count, offset);
}

[[maybe_unused]] LIBC_INLINE MemcmpReturnType
inline_memcmp_aligned_access_64bit(CPtr p1, CPtr p2, size_t count) {
  constexpr size_t kAlign = sizeof(uint64_t);
  if (count <= 2 * kAlign)
    return inline_memcmp_byte_per_byte(p1, p2, count);
  size_t bytes_to_p1_align = distance_to_align_up<kAlign>(p1);
  if (auto value = inline_memcmp_byte_per_byte(p1, p2, bytes_to_p1_align))
    return value;
  size_t offset = bytes_to_p1_align;
  size_t p2_alignment = distance_to_align_down<kAlign>(p2 + offset);
  for (; offset < count - kAlign; offset += kAlign) {
    uint64_t a = load64_aligned<uint64_t>(p1, offset);
    uint64_t b = load64_aligned(p2, offset, p2_alignment);
    if (a != b)
      return cmp_neq_uint64_t(Endian::to_big_endian(a),
                              Endian::to_big_endian(b));
  }
  return inline_memcmp_byte_per_byte(p1, p2, count, offset);
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_GENERIC_ALIGNED_ACCESS_H

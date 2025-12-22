// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UTILS_MEMCOPY_H_
#define V8_UTILS_MEMCOPY_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <type_traits>

#include "include/v8config.h"
#include "src/base/bits.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/memcopy.h"
#include "src/utils/utils.h"

namespace v8::internal {

using Address = uintptr_t;

inline void MemMove(void* dest, const void* src, size_t size) {
  v8::base::MemMove(dest, src, size);
}

inline void MemCopy(void* dest, const void* src, size_t size) {
  v8::base::MemCopy(dest, src, size);
}

#if V8_TARGET_BIG_ENDIAN
inline void MemCopyAndSwitchEndianness(void* dst, void* src,
                                       size_t num_elements,
                                       size_t element_size) {
  v8::base::MemCopyAndSwitchEndianness(dst, src, num_elements, element_size);
}
#endif  // V8_TARGET_BIG_ENDIAN

// Copies words from |src| to |dst|. The data spans must not overlap.
// |src| and |dst| must be TWord-size aligned.
template <typename T>
inline void CopyImpl(T* dst_ptr, const T* src_ptr, size_t count) {
  constexpr int kTWordSize = sizeof(T);
#ifdef DEBUG
  Address dst = reinterpret_cast<Address>(dst_ptr);
  Address src = reinterpret_cast<Address>(src_ptr);
  DCHECK(IsAligned(dst, kTWordSize));
  DCHECK(IsAligned(src, kTWordSize));
#endif
  MemCopy(dst_ptr, src_ptr, count * kTWordSize);
}

// Copies `count` system words from `src` to `dst`.  The data spans must not
// overlap. `src` and `dst` must be kSystemPointerSize-aligned.
inline void CopyWords(Address dst, const Address src, size_t count) {
  CopyImpl(reinterpret_cast<Address*>(dst),
           reinterpret_cast<const Address*>(src), count);
}

// Copies `count` tagged words from `src` to `dst`.  The data spans must not
// overlap. `src` and `dst` must be kTaggedSize-aligned.
inline void CopyTagged(Address dst, const Address src, size_t count) {
  CopyImpl(reinterpret_cast<Tagged_t*>(dst),
           reinterpret_cast<const Tagged_t*>(src), count);
}

// Copies `count` bytes from `src` to `dst`.  The data spans must not overlap.
template <typename T>
inline void CopyBytes(T* dst, const T* src, size_t count) {
  static_assert(sizeof(T) == 1);
  CopyImpl(dst, src, count);
}

// Copy from 8bit/16bit chars to 8bit/16bit chars. Values are zero-extended if
// needed. Ranges are not allowed to overlap.
// The separate declaration is needed for the V8_NONNULL, which is not allowed
// on a definition.
template <typename SrcType, typename DstType>
void CopyChars(DstType* dst, const SrcType* src, size_t count) V8_NONNULL(1, 2);

template <typename SrcType, typename DstType>
void CopyChars(DstType* dst, const SrcType* src, size_t count) {
  static_assert(std::is_integral_v<SrcType>);
  static_assert(std::is_integral_v<DstType>);
  using SrcTypeUnsigned = std::make_unsigned_t<SrcType>;
  using DstTypeUnsigned = std::make_unsigned_t<DstType>;

  auto* dst_u = reinterpret_cast<DstTypeUnsigned*>(dst);
  auto* src_u = reinterpret_cast<const SrcTypeUnsigned*>(src);

  if constexpr (std::is_same_v<DstTypeUnsigned, SrcTypeUnsigned>) {
    v8::base::MemCopy(dst_u, src_u, count * sizeof(DstTypeUnsigned));
    return;
  }

#ifdef DEBUG
  // Check for no overlap, otherwise {std::copy_n} cannot be used.
  Address src_start = reinterpret_cast<Address>(src);
  Address src_end = src_start + count * sizeof(SrcType);
  Address dst_start = reinterpret_cast<Address>(dst);
  Address dst_end = dst_start + count * sizeof(DstType);
  DCHECK(src_end <= dst_start || dst_end <= src_start);
#endif

  // Especially Atom CPUs profit from this explicit instantiation for small
  // counts. This gives up to 20 percent improvement for microbenchmarks such as
  // joining an array of small integers (2019-10-16).
  switch (count) {
#define CASE(N)                   \
  case N:                         \
    std::copy_n(src_u, N, dst_u); \
    return;
    CASE(1)
    CASE(2)
    CASE(3)
    CASE(4)
    CASE(5)
    CASE(6)
    CASE(7)
    CASE(8)
    CASE(9)
    CASE(10)
    CASE(11)
    CASE(12)
    CASE(13)
    CASE(14)
    CASE(15)
    CASE(16)
#undef CASE
    default:
      std::copy_n(src_u, count, dst_u);
      return;
  }
}

// Fills `destination` with `count` `value`s.
template <typename T, typename U>
constexpr void Memset(T* destination, U value, size_t count)
  requires std::is_trivially_assignable_v<T&, U>
{
  return v8::base::Memset(destination, value, count);
}

// Fills `destination` with `count` `value`s.
template <typename T>
inline void Relaxed_Memset(T* destination, T value, size_t count)
  requires std::is_integral_v<T>
{
  return v8::base::Relaxed_Memset(destination, value, count);
}

}  // namespace v8::internal

#endif  // V8_UTILS_MEMCOPY_H_

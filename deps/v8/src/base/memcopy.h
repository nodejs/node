// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_MEMCOPY_H_
#define V8_BASE_MEMCOPY_H_

#include <stdlib.h>

#include <atomic>

#include "include/v8config.h"
#include "src/base/base-export.h"
#include "src/base/bits.h"
#include "src/base/macros.h"

#if defined(V8_TARGET_ARCH_ARM64) && \
    (defined(__ARM_NEON) || defined(__ARM_NEON__))
#define V8_OPTIMIZE_WITH_NEON
#include <arm_neon.h>
#endif
namespace v8::base {

// Routines for memcpy and friends. Historically, V8 has specialized various
// implementations on different platforms for performance reasons. In an ideal
// world this would all just be plain stdlib `memcpy()` and friends.

#if defined(V8_OPTIMIZE_WITH_NEON)

// We intentionally use misaligned read/writes for NEON intrinsics, disable
// alignment sanitization explicitly.
// Overlapping writes help to save instructions, e.g. doing 2 two-byte writes
// instead 3 one-byte write for count == 3.
template <typename IntType>
V8_INLINE V8_CLANG_NO_SANITIZE("alignment") void OverlappingWrites(
    void* dst, const void* src, size_t count) {
  *reinterpret_cast<IntType*>(dst) = *reinterpret_cast<const IntType*>(src);
  *reinterpret_cast<IntType*>(static_cast<uint8_t*>(dst) + count -
                              sizeof(IntType)) =
      *reinterpret_cast<const IntType*>(static_cast<const uint8_t*>(src) +
                                        count - sizeof(IntType));
}

V8_CLANG_NO_SANITIZE("alignment")
inline void SimdMemCopy(void* dst, const void* src, size_t count) {
  auto* dst_u = static_cast<uint8_t*>(dst);
  const auto* src_u = static_cast<const uint8_t*>(src);
  // Common cases. Handle before doing clz.
  if (count == 0) {
    return;
  }
  if (count == 1) {
    *dst_u = *src_u;
    return;
  }
  const size_t order =
      sizeof(count) * CHAR_BIT - bits::CountLeadingZeros(count - 1);
  switch (order) {
    case 1:  // count: [2, 2]
      *reinterpret_cast<uint16_t*>(dst_u) =
          *reinterpret_cast<const uint16_t*>(src_u);
      return;
    case 2:  // count: [3, 4]
      OverlappingWrites<uint16_t>(dst_u, src_u, count);
      return;
    case 3:  // count: [5, 8]
      OverlappingWrites<uint32_t>(dst_u, src_u, count);
      return;
    case 4:  // count: [9, 16]
      OverlappingWrites<uint64_t>(dst_u, src_u, count);
      return;
    case 5:  // count: [17, 32]
      vst1q_u8(dst_u, vld1q_u8(src_u));
      vst1q_u8(dst_u + count - sizeof(uint8x16_t),
               vld1q_u8(src_u + count - sizeof(uint8x16_t)));
      return;
    default:  // count: [33, ...]
      vst1q_u8(dst_u, vld1q_u8(src_u));
      for (size_t i = count % sizeof(uint8x16_t); i < count;
           i += sizeof(uint8x16_t)) {
        vst1q_u8(dst_u + i, vld1q_u8(src_u + i));
      }
      return;
  }
}

V8_INLINE void MemCopy(void* dest, const void* src, size_t size) {
#ifdef DEBUG
  if (size == 0) {
    return;
  }
  // Check that there's no overlap in ranges.
  const char* src_char = reinterpret_cast<const char*>(src);
  char* dest_char = reinterpret_cast<char*>(dest);
  DCHECK(dest_char >= (src_char + size) || src_char >= (dest_char + size));
#endif  // DEBUG
  // Wrap call to be able to easily identify SIMD usage in profiles.
  SimdMemCopy(dest, src, size);
}

#else  // !defined(V8_OPTIMIZE_WITH_NEON)

// Copy memory area to disjoint memory area.
V8_INLINE void MemCopy(void* dest, const void* src, size_t size) {
#ifdef DEBUG
  if (size == 0) {
    return;
  }
  // Check that there's no overlap in ranges.
  const char* src_char = reinterpret_cast<const char*>(src);
  char* dest_char = reinterpret_cast<char*>(dest);
  DCHECK(dest_char >= (src_char + size) || src_char >= (dest_char + size));
#endif  // DEBUG
  // Fast path for small sizes. The compiler will expand the `memcpy()` for
  // small fixed sizes to a sequence of move instructions. This avoids the
  // overhead of the general `memcpy()` function.
  switch (size) {
    case 0:
      return;
#define CASE(N)           \
  case N:                 \
    memcpy(dest, src, N); \
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
      memcpy(dest, src, size);
      return;
  }
}

#endif  // !defined(V8_OPTIMIZE_WITH_NEON)

V8_INLINE void MemMove(void* dest, const void* src, size_t size) {
  // Fast path for small sizes. The compiler will expand the `memmove()` for
  // small fixed sizes to a sequence of move instructions. This avoids the
  // overhead of the general `memmove()` function.
  switch (size) {
    case 0:
      return;
#define CASE(N)            \
  case N:                  \
    memmove(dest, src, N); \
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
      memmove(dest, src, size);
      return;
  }
}

#if V8_TARGET_BIG_ENDIAN
inline void MemCopyAndSwitchEndianness(void* dst, void* src,
                                       size_t num_elements,
                                       size_t element_size) {
#define COPY_LOOP(type, reverse)                            \
  {                                                         \
    for (uint32_t i = 0; i < num_elements; i++) {           \
      type t;                                               \
      type* s = reinterpret_cast<type*>(src) + i;           \
      type* d = reinterpret_cast<type*>(dst) + i;           \
      memcpy(&t, reinterpret_cast<void*>(s), element_size); \
      t = reverse(t);                                       \
      memcpy(reinterpret_cast<void*>(d), &t, element_size); \
    }                                                       \
    return;                                                 \
  }

  switch (element_size) {
    case 1:
      MemCopy(dst, src, num_elements);
      return;
    case 2:
      COPY_LOOP(uint16_t, bits::ByteReverse16);
    case 4:
      COPY_LOOP(uint32_t, bits::ByteReverse32);
    case 8:
      COPY_LOOP(uint64_t, bits::ByteReverse64);
    default:
      UNREACHABLE();
  }
#undef COPY_LOOP
}
#endif

template <typename T>
V8_INLINE bool TryTrivialCopy(const T* src_begin, const T* src_end, T* dest) {
  DCHECK_LE(src_begin, src_end);
  if constexpr (std::is_trivially_copyable_v<T>) {
    const size_t count = src_end - src_begin;
    base::MemCopy(dest, src_begin, count * sizeof(T));
    return true;
  }
  return false;
}

template <typename T>
V8_INLINE bool TryTrivialMove(const T* src_begin, const T* src_end, T* dest) {
  DCHECK_LE(src_begin, src_end);
  if constexpr (std::is_trivially_copyable_v<T>) {
    const size_t count = src_end - src_begin;
    base::MemMove(dest, src_begin, count * sizeof(T));
    return true;
  }
  return false;
}

// Fills `destination` with `count` `value`s.
template <typename T, typename U>
constexpr void Memset(T* destination, U value, size_t count)
  requires std::is_trivially_assignable_v<T&, U>
{
  for (size_t i = 0; i < count; i++) {
    destination[i] = value;
  }
}

// Fills `destination` with `count` `value`s.
template <typename T>
inline void Relaxed_Memset(T* destination, T value, size_t count)
  requires std::is_integral_v<T>
{
  for (size_t i = 0; i < count; i++) {
    std::atomic_ref<T>(destination[i]).store(value, std::memory_order_relaxed);
  }
}

}  // namespace v8::base

#endif  // V8_BASE_MEMCOPY_H_

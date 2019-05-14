// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MEMCOPY_H_
#define V8_MEMCOPY_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "src/base/logging.h"
#include "src/base/macros.h"

namespace v8 {
namespace internal {

typedef uintptr_t Address;

// ----------------------------------------------------------------------------
// Generated memcpy/memmove for ia32, arm, and mips.

void init_memcopy_functions();

#if defined(V8_TARGET_ARCH_IA32)
// Limit below which the extra overhead of the MemCopy function is likely
// to outweigh the benefits of faster copying.
const size_t kMinComplexMemCopy = 64;

// Copy memory area. No restrictions.
V8_EXPORT_PRIVATE void MemMove(void* dest, const void* src, size_t size);
typedef void (*MemMoveFunction)(void* dest, const void* src, size_t size);

// Keep the distinction of "move" vs. "copy" for the benefit of other
// architectures.
V8_INLINE void MemCopy(void* dest, const void* src, size_t size) {
  MemMove(dest, src, size);
}
#elif defined(V8_HOST_ARCH_ARM)
typedef void (*MemCopyUint8Function)(uint8_t* dest, const uint8_t* src,
                                     size_t size);
V8_EXPORT_PRIVATE extern MemCopyUint8Function memcopy_uint8_function;
V8_INLINE void MemCopyUint8Wrapper(uint8_t* dest, const uint8_t* src,
                                   size_t chars) {
  memcpy(dest, src, chars);
}
// For values < 16, the assembler function is slower than the inlined C code.
const size_t kMinComplexMemCopy = 16;
V8_INLINE void MemCopy(void* dest, const void* src, size_t size) {
  (*memcopy_uint8_function)(reinterpret_cast<uint8_t*>(dest),
                            reinterpret_cast<const uint8_t*>(src), size);
}
V8_EXPORT_PRIVATE V8_INLINE void MemMove(void* dest, const void* src,
                                         size_t size) {
  memmove(dest, src, size);
}

typedef void (*MemCopyUint16Uint8Function)(uint16_t* dest, const uint8_t* src,
                                           size_t size);
extern MemCopyUint16Uint8Function memcopy_uint16_uint8_function;
void MemCopyUint16Uint8Wrapper(uint16_t* dest, const uint8_t* src,
                               size_t chars);
// For values < 12, the assembler function is slower than the inlined C code.
const int kMinComplexConvertMemCopy = 12;
V8_INLINE void MemCopyUint16Uint8(uint16_t* dest, const uint8_t* src,
                                  size_t size) {
  (*memcopy_uint16_uint8_function)(dest, src, size);
}
#elif defined(V8_HOST_ARCH_MIPS)
typedef void (*MemCopyUint8Function)(uint8_t* dest, const uint8_t* src,
                                     size_t size);
V8_EXPORT_PRIVATE extern MemCopyUint8Function memcopy_uint8_function;
V8_INLINE void MemCopyUint8Wrapper(uint8_t* dest, const uint8_t* src,
                                   size_t chars) {
  memcpy(dest, src, chars);
}
// For values < 16, the assembler function is slower than the inlined C code.
const size_t kMinComplexMemCopy = 16;
V8_INLINE void MemCopy(void* dest, const void* src, size_t size) {
  (*memcopy_uint8_function)(reinterpret_cast<uint8_t*>(dest),
                            reinterpret_cast<const uint8_t*>(src), size);
}
V8_EXPORT_PRIVATE V8_INLINE void MemMove(void* dest, const void* src,
                                         size_t size) {
  memmove(dest, src, size);
}
#else
// Copy memory area to disjoint memory area.
V8_INLINE void MemCopy(void* dest, const void* src, size_t size) {
  memcpy(dest, src, size);
}
V8_EXPORT_PRIVATE V8_INLINE void MemMove(void* dest, const void* src,
                                         size_t size) {
  memmove(dest, src, size);
}
const size_t kMinComplexMemCopy = 8;
#endif  // V8_TARGET_ARCH_IA32

// Copies words from |src| to |dst|. The data spans must not overlap.
// |src| and |dst| must be TWord-size aligned.
template <size_t kBlockCopyLimit, typename T>
inline void CopyImpl(T* dst_ptr, const T* src_ptr, size_t count) {
  constexpr int kTWordSize = sizeof(T);
#ifdef DEBUG
  Address dst = reinterpret_cast<Address>(dst_ptr);
  Address src = reinterpret_cast<Address>(src_ptr);
  DCHECK(IsAligned(dst, kTWordSize));
  DCHECK(IsAligned(src, kTWordSize));
  DCHECK(((src <= dst) && ((src + count * kTWordSize) <= dst)) ||
         ((dst <= src) && ((dst + count * kTWordSize) <= src)));
#endif

  // Use block copying MemCopy if the segment we're copying is
  // enough to justify the extra call/setup overhead.
  if (count < kBlockCopyLimit) {
    do {
      count--;
      *dst_ptr++ = *src_ptr++;
    } while (count > 0);
  } else {
    MemCopy(dst_ptr, src_ptr, count * kTWordSize);
  }
}

// Copies kSystemPointerSize-sized words from |src| to |dst|. The data spans
// must not overlap. |src| and |dst| must be kSystemPointerSize-aligned.
inline void CopyWords(Address dst, const Address src, size_t num_words) {
  static const size_t kBlockCopyLimit = 16;
  CopyImpl<kBlockCopyLimit>(reinterpret_cast<Address*>(dst),
                            reinterpret_cast<const Address*>(src), num_words);
}

// Copies data from |src| to |dst|.  The data spans must not overlap.
template <typename T>
inline void CopyBytes(T* dst, const T* src, size_t num_bytes) {
  STATIC_ASSERT(sizeof(T) == 1);
  if (num_bytes == 0) return;
  CopyImpl<kMinComplexMemCopy>(dst, src, num_bytes);
}

inline void MemsetInt32(int32_t* dest, int32_t value, size_t counter) {
#if V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64
#define STOS "stosl"
#endif

#if defined(MEMORY_SANITIZER)
  // MemorySanitizer does not understand inline assembly.
#undef STOS
#endif

#if defined(__GNUC__) && defined(STOS)
  asm volatile(
      "cld;"
      "rep ; " STOS
      : "+&c"(counter), "+&D"(dest)
      : "a"(value)
      : "memory", "cc");
#else
  for (size_t i = 0; i < counter; i++) {
    dest[i] = value;
  }
#endif

#undef STOS
}

inline void MemsetPointer(Address* dest, Address value, size_t counter) {
#if V8_HOST_ARCH_IA32
#define STOS "stosl"
#elif V8_HOST_ARCH_X64
#define STOS "stosq"
#endif

#if defined(MEMORY_SANITIZER)
  // MemorySanitizer does not understand inline assembly.
#undef STOS
#endif

#if defined(__GNUC__) && defined(STOS)
  asm volatile(
      "cld;"
      "rep ; " STOS
      : "+&c"(counter), "+&D"(dest)
      : "a"(value)
      : "memory", "cc");
#else
  for (size_t i = 0; i < counter; i++) {
    dest[i] = value;
  }
#endif

#undef STOS
}

template <typename T, typename U>
inline void MemsetPointer(T** dest, U* value, size_t counter) {
#ifdef DEBUG
  T* a = nullptr;
  U* b = nullptr;
  a = b;  // Fake assignment to check assignability.
  USE(a);
#endif  // DEBUG
  MemsetPointer(reinterpret_cast<Address*>(dest),
                reinterpret_cast<Address>(value), counter);
}

template <typename sourcechar, typename sinkchar>
V8_INLINE static void CopyCharsUnsigned(sinkchar* dest, const sourcechar* src,
                                        size_t chars);
#if defined(V8_HOST_ARCH_ARM)
V8_INLINE void CopyCharsUnsigned(uint8_t* dest, const uint8_t* src,
                                 size_t chars);
V8_INLINE void CopyCharsUnsigned(uint16_t* dest, const uint8_t* src,
                                 size_t chars);
V8_INLINE void CopyCharsUnsigned(uint16_t* dest, const uint16_t* src,
                                 size_t chars);
#elif defined(V8_HOST_ARCH_MIPS)
V8_INLINE void CopyCharsUnsigned(uint8_t* dest, const uint8_t* src,
                                 size_t chars);
V8_INLINE void CopyCharsUnsigned(uint16_t* dest, const uint16_t* src,
                                 size_t chars);
#elif defined(V8_HOST_ARCH_PPC) || defined(V8_HOST_ARCH_S390)
V8_INLINE void CopyCharsUnsigned(uint8_t* dest, const uint8_t* src,
                                 size_t chars);
V8_INLINE void CopyCharsUnsigned(uint16_t* dest, const uint16_t* src,
                                 size_t chars);
#endif

// Copy from 8bit/16bit chars to 8bit/16bit chars.
template <typename sourcechar, typename sinkchar>
V8_INLINE void CopyChars(sinkchar* dest, const sourcechar* src, size_t chars);

template <typename sourcechar, typename sinkchar>
void CopyChars(sinkchar* dest, const sourcechar* src, size_t chars) {
  DCHECK_LE(sizeof(sourcechar), 2);
  DCHECK_LE(sizeof(sinkchar), 2);
  if (sizeof(sinkchar) == 1) {
    if (sizeof(sourcechar) == 1) {
      CopyCharsUnsigned(reinterpret_cast<uint8_t*>(dest),
                        reinterpret_cast<const uint8_t*>(src), chars);
    } else {
      CopyCharsUnsigned(reinterpret_cast<uint8_t*>(dest),
                        reinterpret_cast<const uint16_t*>(src), chars);
    }
  } else {
    if (sizeof(sourcechar) == 1) {
      CopyCharsUnsigned(reinterpret_cast<uint16_t*>(dest),
                        reinterpret_cast<const uint8_t*>(src), chars);
    } else {
      CopyCharsUnsigned(reinterpret_cast<uint16_t*>(dest),
                        reinterpret_cast<const uint16_t*>(src), chars);
    }
  }
}

template <typename sourcechar, typename sinkchar>
void CopyCharsUnsigned(sinkchar* dest, const sourcechar* src, size_t chars) {
  sinkchar* limit = dest + chars;
  if ((sizeof(*dest) == sizeof(*src)) &&
      (chars >= kMinComplexMemCopy / sizeof(*dest))) {
    MemCopy(dest, src, chars * sizeof(*dest));
  } else {
    while (dest < limit) *dest++ = static_cast<sinkchar>(*src++);
  }
}

#if defined(V8_HOST_ARCH_ARM)
void CopyCharsUnsigned(uint8_t* dest, const uint8_t* src, size_t chars) {
  switch (static_cast<unsigned>(chars)) {
    case 0:
      break;
    case 1:
      *dest = *src;
      break;
    case 2:
      memcpy(dest, src, 2);
      break;
    case 3:
      memcpy(dest, src, 3);
      break;
    case 4:
      memcpy(dest, src, 4);
      break;
    case 5:
      memcpy(dest, src, 5);
      break;
    case 6:
      memcpy(dest, src, 6);
      break;
    case 7:
      memcpy(dest, src, 7);
      break;
    case 8:
      memcpy(dest, src, 8);
      break;
    case 9:
      memcpy(dest, src, 9);
      break;
    case 10:
      memcpy(dest, src, 10);
      break;
    case 11:
      memcpy(dest, src, 11);
      break;
    case 12:
      memcpy(dest, src, 12);
      break;
    case 13:
      memcpy(dest, src, 13);
      break;
    case 14:
      memcpy(dest, src, 14);
      break;
    case 15:
      memcpy(dest, src, 15);
      break;
    default:
      MemCopy(dest, src, chars);
      break;
  }
}

void CopyCharsUnsigned(uint16_t* dest, const uint8_t* src, size_t chars) {
  if (chars >= static_cast<size_t>(kMinComplexConvertMemCopy)) {
    MemCopyUint16Uint8(dest, src, chars);
  } else {
    MemCopyUint16Uint8Wrapper(dest, src, chars);
  }
}

void CopyCharsUnsigned(uint16_t* dest, const uint16_t* src, size_t chars) {
  switch (static_cast<unsigned>(chars)) {
    case 0:
      break;
    case 1:
      *dest = *src;
      break;
    case 2:
      memcpy(dest, src, 4);
      break;
    case 3:
      memcpy(dest, src, 6);
      break;
    case 4:
      memcpy(dest, src, 8);
      break;
    case 5:
      memcpy(dest, src, 10);
      break;
    case 6:
      memcpy(dest, src, 12);
      break;
    case 7:
      memcpy(dest, src, 14);
      break;
    default:
      MemCopy(dest, src, chars * sizeof(*dest));
      break;
  }
}

#elif defined(V8_HOST_ARCH_MIPS)
void CopyCharsUnsigned(uint8_t* dest, const uint8_t* src, size_t chars) {
  if (chars < kMinComplexMemCopy) {
    memcpy(dest, src, chars);
  } else {
    MemCopy(dest, src, chars);
  }
}

void CopyCharsUnsigned(uint16_t* dest, const uint16_t* src, size_t chars) {
  if (chars < kMinComplexMemCopy) {
    memcpy(dest, src, chars * sizeof(*dest));
  } else {
    MemCopy(dest, src, chars * sizeof(*dest));
  }
}
#elif defined(V8_HOST_ARCH_PPC) || defined(V8_HOST_ARCH_S390)
#define CASE(n)           \
  case n:                 \
    memcpy(dest, src, n); \
    break
void CopyCharsUnsigned(uint8_t* dest, const uint8_t* src, size_t chars) {
  switch (static_cast<unsigned>(chars)) {
    case 0:
      break;
    case 1:
      *dest = *src;
      break;
      CASE(2);
      CASE(3);
      CASE(4);
      CASE(5);
      CASE(6);
      CASE(7);
      CASE(8);
      CASE(9);
      CASE(10);
      CASE(11);
      CASE(12);
      CASE(13);
      CASE(14);
      CASE(15);
      CASE(16);
      CASE(17);
      CASE(18);
      CASE(19);
      CASE(20);
      CASE(21);
      CASE(22);
      CASE(23);
      CASE(24);
      CASE(25);
      CASE(26);
      CASE(27);
      CASE(28);
      CASE(29);
      CASE(30);
      CASE(31);
      CASE(32);
      CASE(33);
      CASE(34);
      CASE(35);
      CASE(36);
      CASE(37);
      CASE(38);
      CASE(39);
      CASE(40);
      CASE(41);
      CASE(42);
      CASE(43);
      CASE(44);
      CASE(45);
      CASE(46);
      CASE(47);
      CASE(48);
      CASE(49);
      CASE(50);
      CASE(51);
      CASE(52);
      CASE(53);
      CASE(54);
      CASE(55);
      CASE(56);
      CASE(57);
      CASE(58);
      CASE(59);
      CASE(60);
      CASE(61);
      CASE(62);
      CASE(63);
      CASE(64);
    default:
      memcpy(dest, src, chars);
      break;
  }
}
#undef CASE

#define CASE(n)               \
  case n:                     \
    memcpy(dest, src, n * 2); \
    break
void CopyCharsUnsigned(uint16_t* dest, const uint16_t* src, size_t chars) {
  switch (static_cast<unsigned>(chars)) {
    case 0:
      break;
    case 1:
      *dest = *src;
      break;
      CASE(2);
      CASE(3);
      CASE(4);
      CASE(5);
      CASE(6);
      CASE(7);
      CASE(8);
      CASE(9);
      CASE(10);
      CASE(11);
      CASE(12);
      CASE(13);
      CASE(14);
      CASE(15);
      CASE(16);
      CASE(17);
      CASE(18);
      CASE(19);
      CASE(20);
      CASE(21);
      CASE(22);
      CASE(23);
      CASE(24);
      CASE(25);
      CASE(26);
      CASE(27);
      CASE(28);
      CASE(29);
      CASE(30);
      CASE(31);
      CASE(32);
    default:
      memcpy(dest, src, chars * 2);
      break;
  }
}
#undef CASE
#endif

}  // namespace internal
}  // namespace v8

#endif  // V8_MEMCOPY_H_

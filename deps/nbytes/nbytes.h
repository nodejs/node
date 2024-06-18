#pragma once

#include <stddef.h>
#include <stdint.h>
#include <cmath>
#include <cstddef>

namespace nbytes {

// The nbytes (short for "node bytes") is a set of utility helpers for
// working with bytes that are extracted from Node.js' internals. The
// motivation for extracting these into a separate library is to make it
// easier for other projects to implement functionality that is compatible
// with Node.js' implementation of various byte manipulation functions.

// Round up a to the next highest multiple of b.
template <typename T>
constexpr T RoundUp(T a, T b) {
  return a % b != 0 ? a + b - (a % b) : a;
}

// Align ptr to an `alignment`-bytes boundary.
template <typename T, typename U>
constexpr T* AlignUp(T* ptr, U alignment) {
  return reinterpret_cast<T*>(
      RoundUp(reinterpret_cast<uintptr_t>(ptr), alignment));
}

// ============================================================================
// Byte Swapping

// Swaps bytes in place. nbytes is the number of bytes to swap and must be a
// multiple of the word size (checked by function).
bool SwapBytes16(void* data, size_t nbytes);
bool SwapBytes32(void* data, size_t nbytes);
bool SwapBytes64(void* data, size_t nbytes);

// ============================================================================
// Base64 (legacy)

#ifdef _MSC_VER
#pragma warning(push)
// MSVC C4003: not enough actual parameters for macro 'identifier'
#pragma warning(disable : 4003)
#endif

extern const int8_t unbase64_table[256];

template <typename TypeName>
bool Base64DecodeGroupSlow(char* const dst, const size_t dstlen,
                           const TypeName* const src, const size_t srclen,
                           size_t* const i, size_t* const k) {
  uint8_t hi;
  uint8_t lo;
#define V(expr)                                                                \
  for (;;) {                                                                   \
    const uint8_t c = static_cast<uint8_t>(src[*i]);                           \
    lo = unbase64_table[c];                                                    \
    *i += 1;                                                                   \
    if (lo < 64) break;                         /* Legal character. */         \
    if (c == '=' || *i >= srclen) return false; /* Stop decoding. */           \
  }                                                                            \
  expr;                                                                        \
  if (*i >= srclen) return false;                                              \
  if (*k >= dstlen) return false;                                              \
  hi = lo;
  V(/* Nothing. */);
  V(dst[(*k)++] = ((hi & 0x3F) << 2) | ((lo & 0x30) >> 4));
  V(dst[(*k)++] = ((hi & 0x0F) << 4) | ((lo & 0x3C) >> 2));
  V(dst[(*k)++] = ((hi & 0x03) << 6) | ((lo & 0x3F) >> 0));
#undef V
  return true;  // Continue decoding.
}

enum class Base64Mode {
  NORMAL,
  URL
};

inline constexpr size_t Base64EncodedSize(
    size_t size,
    Base64Mode mode = Base64Mode::NORMAL) {
  return mode == Base64Mode::NORMAL ? ((size + 2) / 3 * 4)
                                    : static_cast<size_t>(std::ceil(
                                          static_cast<double>(size * 4) / 3));
}

// Doesn't check for padding at the end.  Can be 1-2 bytes over.
inline constexpr size_t Base64DecodedSizeFast(size_t size) {
  // 1-byte input cannot be decoded
  return size > 1 ? (size / 4) * 3 + (size % 4 + 1) / 2 : 0;
}

inline uint32_t ReadUint32BE(const unsigned char* p) {
  return static_cast<uint32_t>(p[0] << 24U) |
         static_cast<uint32_t>(p[1] << 16U) |
         static_cast<uint32_t>(p[2] << 8U) |
         static_cast<uint32_t>(p[3]);
}

template <typename TypeName>
size_t Base64DecodedSize(const TypeName* src, size_t size) {
  // 1-byte input cannot be decoded
  if (size < 2)
    return 0;

  if (src[size - 1] == '=') {
    size--;
    if (src[size - 1] == '=')
      size--;
  }
  return Base64DecodedSizeFast(size);
}

template <typename TypeName>
size_t Base64DecodeFast(char* const dst, const size_t dstlen,
                        const TypeName* const src, const size_t srclen,
                        const size_t decoded_size) {
  const size_t available = dstlen < decoded_size ? dstlen : decoded_size;
  const size_t max_k = available / 3 * 3;
  size_t max_i = srclen / 4 * 4;
  size_t i = 0;
  size_t k = 0;
  while (i < max_i && k < max_k) {
    const unsigned char txt[] = {
        static_cast<unsigned char>(unbase64_table[static_cast<uint8_t>(src[i + 0])]),
        static_cast<unsigned char>(unbase64_table[static_cast<uint8_t>(src[i + 1])]),
        static_cast<unsigned char>(unbase64_table[static_cast<uint8_t>(src[i + 2])]),
        static_cast<unsigned char>(unbase64_table[static_cast<uint8_t>(src[i + 3])]),
    };

    const uint32_t v = ReadUint32BE(txt);
    // If MSB is set, input contains whitespace or is not valid base64.
    if (v & 0x80808080) {
      if (!Base64DecodeGroupSlow(dst, dstlen, src, srclen, &i, &k))
        return k;
      max_i = i + (srclen - i) / 4 * 4;  // Align max_i again.
    } else {
      dst[k + 0] = ((v >> 22) & 0xFC) | ((v >> 20) & 0x03);
      dst[k + 1] = ((v >> 12) & 0xF0) | ((v >> 10) & 0x0F);
      dst[k + 2] = ((v >>  2) & 0xC0) | ((v >>  0) & 0x3F);
      i += 4;
      k += 3;
    }
  }
  if (i < srclen && k < dstlen) {
    Base64DecodeGroupSlow(dst, dstlen, src, srclen, &i, &k);
  }
  return k;
}

template <typename TypeName>
size_t Base64Decode(char* const dst, const size_t dstlen,
                     const TypeName* const src, const size_t srclen) {
  const size_t decoded_size = Base64DecodedSize(src, srclen);
  return Base64DecodeFast(dst, dstlen, src, srclen, decoded_size);
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}  // namespace nbytes

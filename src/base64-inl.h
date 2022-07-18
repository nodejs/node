#ifndef SRC_BASE64_INL_H_
#define SRC_BASE64_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base64.h"
#include "util.h"

#if (defined(__x86_64) || defined(__x86_64__)) &&                              \
    (defined(__GNUC__) || defined(__GNUC))
#include <immintrin.h>
#endif

namespace node {

extern const int8_t unbase64_table[256];


inline static int8_t unbase64(uint8_t x) {
  return unbase64_table[x];
}


inline uint32_t ReadUint32BE(const unsigned char* p) {
  return static_cast<uint32_t>(p[0] << 24U) |
         static_cast<uint32_t>(p[1] << 16U) |
         static_cast<uint32_t>(p[2] << 8U) |
         static_cast<uint32_t>(p[3]);
}

#ifdef _MSC_VER
#pragma warning(push)
// MSVC C4003: not enough actual parameters for macro 'identifier'
#pragma warning(disable : 4003)
#endif

template <typename TypeName>
bool base64_decode_group_slow(char* const dst, const size_t dstlen,
                              const TypeName* const src, const size_t srclen,
                              size_t* const i, size_t* const k) {
  uint8_t hi;
  uint8_t lo;
#define V(expr)                                                                \
  for (;;) {                                                                   \
    const uint8_t c = static_cast<uint8_t>(src[*i]);                           \
    lo = unbase64(c);                                                          \
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

#ifdef _MSC_VER
#pragma warning(pop)
#endif

template <typename TypeName>
size_t base64_decode_fast(char* const dst, const size_t dstlen,
                          const TypeName* const src, const size_t srclen,
                          const size_t decoded_size) {
  const size_t available = dstlen < decoded_size ? dstlen : decoded_size;
  const size_t max_k = available / 3 * 3;
  size_t max_i = srclen / 4 * 4;
  size_t i = 0;
  size_t k = 0;
  while (i < max_i && k < max_k) {
    const unsigned char txt[] = {
        static_cast<unsigned char>(unbase64(static_cast<uint8_t>(src[i + 0]))),
        static_cast<unsigned char>(unbase64(static_cast<uint8_t>(src[i + 1]))),
        static_cast<unsigned char>(unbase64(static_cast<uint8_t>(src[i + 2]))),
        static_cast<unsigned char>(unbase64(static_cast<uint8_t>(src[i + 3]))),
    };

    const uint32_t v = ReadUint32BE(txt);
    // If MSB is set, input contains whitespace or is not valid base64.
    if (v & 0x80808080) {
      if (!base64_decode_group_slow(dst, dstlen, src, srclen, &i, &k))
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
    base64_decode_group_slow(dst, dstlen, src, srclen, &i, &k);
  }
  return k;
}


template <typename TypeName>
size_t base64_decoded_size(const TypeName* src, size_t size) {
  // 1-byte input cannot be decoded
  if (size < 2)
    return 0;

  if (src[size - 1] == '=') {
    size--;
    if (src[size - 1] == '=')
      size--;
  }
  return base64_decoded_size_fast(size);
}


template <typename TypeName>
size_t base64_decode(char* const dst, const size_t dstlen,
                     const TypeName* const src, const size_t srclen) {
  const size_t decoded_size = base64_decoded_size(src, srclen);
  return base64_decode_fast(dst, dstlen, src, srclen, decoded_size);
}

inline size_t base64_encode_scalar(
    const char* src, size_t slen, char* dst, size_t dlen, Base64Mode mode) {
  // We know how much we'll write, just make sure that there's space.
  CHECK(dlen >= base64_encoded_size(slen, mode) &&
        "not enough space provided for base64 encode");

  dlen = base64_encoded_size(slen, mode);

  unsigned a;
  unsigned b;
  unsigned c;
  unsigned i;
  unsigned k;
  unsigned n;

  const char* table = base64_select_table(mode);

  i = 0;
  k = 0;
  n = slen / 3 * 3;

  while (i < n) {
    a = src[i + 0] & 0xff;
    b = src[i + 1] & 0xff;
    c = src[i + 2] & 0xff;

    dst[k + 0] = table[a >> 2];
    dst[k + 1] = table[((a & 3) << 4) | (b >> 4)];
    dst[k + 2] = table[((b & 0x0f) << 2) | (c >> 6)];
    dst[k + 3] = table[c & 0x3f];

    i += 3;
    k += 4;
  }

  switch (slen - n) {
    case 1:
      a = src[i + 0] & 0xff;
      dst[k + 0] = table[a >> 2];
      dst[k + 1] = table[(a & 3) << 4];
      if (mode == Base64Mode::NORMAL) {
        dst[k + 2] = '=';
        dst[k + 3] = '=';
      }
      break;
    case 2:
      a = src[i + 0] & 0xff;
      b = src[i + 1] & 0xff;
      dst[k + 0] = table[a >> 2];
      dst[k + 1] = table[((a & 3) << 4) | (b >> 4)];
      dst[k + 2] = table[(b & 0x0f) << 2];
      if (mode == Base64Mode::NORMAL)
        dst[k + 3] = '=';
      break;
  }

  return dlen;
}

#if (defined(__x86_64) || defined(__x86_64__)) &&                              \
    (defined(__GNUC__) || defined(__GNUC))
#pragma GCC target("avx512vl", "avx512vbmi")
/* Core logic is from https://github.com/WojciechMula/base64-avx512
 * Copyright 2019 Daniel Lemire, Wojciech Muła
 * under BSD-3-Clause license
 */
inline size_t base64_encode_avx512vl(
    const char* src, size_t slen, char* dst, size_t dlen, Base64Mode mode) {
  // Do the exact check as scalar algorithm.
  CHECK(dlen >= base64_encoded_size(slen, mode) &&
        "not enough space provided for base64 encode");
  size_t dlen_remain = dlen;
  const char* lookup_tbl = base64_select_table(mode);
  // 32-bit input
  // [ 0  0  0  0  0  0  0  0|c1 c0 d5 d4 d3 d2 d1 d0|
  //  b3 b2 b1 b0 c5 c4 c3 c2|a5 a4 a3 a2 a1 a0 b5 b4]
  // output order  [1, 2, 0, 1]
  // [b3 b2 b1 b0 c5 c4 c3 c2|c1 c0 d5 d4 d3 d2 d1 d0|
  //  a5 a4 a3 a2 a1 a0 b5 b4|b3 b2 b1 b0 c3 c2 c1 c0]

  const __m512i shuffle_input = _mm512_setr_epi32(0x01020001,
                                                  0x04050304,
                                                  0x07080607,
                                                  0x0a0b090a,
                                                  0x0d0e0c0d,
                                                  0x10110f10,
                                                  0x13141213,
                                                  0x16171516,
                                                  0x191a1819,
                                                  0x1c1d1b1c,
                                                  0x1f201e1f,
                                                  0x22232122,
                                                  0x25262425,
                                                  0x28292728,
                                                  0x2b2c2a2b,
                                                  0x2e2f2d2e);
  const __m512i lookup = _mm512_loadu_si512(lookup_tbl);

  while (slen >= 64) {
    const __m512i v = _mm512_loadu_si512(src);

    // Reorder bytes
    // [b3 b2 b1 b0 c5 c4 c3 c2|c1 c0 d5 d4 d3 d2 d1 d0|
    //  a5 a4 a3 a2 a1 a0 b5 b4|b3 b2 b1 b0 c3 c2 c1 c0]
    const __m512i in = _mm512_permutexvar_epi8(shuffle_input, v);

    // After multishift a single 32-bit lane has following layout
    // [c1 c0 d5 d4 d3 d2 d1 d0|b1 b0 c5 c4 c3 c2 c1 c0|
    //  a1 a0 b5 b4 b3 b2 b1 b0|d1 d0 a5 a4 a3 a2 a1 a0]
    // (a = [10:17], b = [4:11], c = [22:27], d = [16:21])

    // 48, 54, 36, 42, 16, 22, 4, 10
    const __m512i shifts = _mm512_set1_epi64(0x3036242a1016040alu);
    const __m512i indices = _mm512_multishift_epi64_epi8(shifts, in);

    // Note: the two higher bits of each indices' byte have garbage
    // but the following permutexvar instruction masks them out

    // Translation 6-bit values to ASCII.
    const __m512i result = _mm512_permutexvar_epi8(indices, lookup);

    _mm512_storeu_si512(reinterpret_cast<__m512i*>(dst), result);

    dlen_remain -= 64;
    dst += 64;
    src += 48;
    slen -= 48;
  }

  if (slen > 0) base64_encode_scalar(src, slen, dst, dlen_remain, mode);

  return dlen;
}

inline size_t base64_encode(
    const char* src, size_t slen, char* dst, size_t dlen, Base64Mode mode) {
  if (__builtin_cpu_supports("avx512vl") &&
      __builtin_cpu_supports("avx512vbmi")) {
    return base64_encode_avx512vl(src, slen, dst, dlen, mode);
  } else {
    return base64_encode_scalar(src, slen, dst, dlen, mode);
  }
}
#pragma GCC reset_options
#else
inline size_t base64_encode(
    const char* src, size_t slen, char* dst, size_t dlen, Base64Mode mode) {
  return base64_encode_scalar(src, slen, dst, dlen, mode);
}
#endif

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_BASE64_INL_H_

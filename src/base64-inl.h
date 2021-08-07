#ifndef SRC_BASE64_INL_H_
#define SRC_BASE64_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base64.h"
#include "util.h"

#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) ||                             \
    defined(__LITTLE_ENDIAN__) ||                                              \
    _MSC_VER
// Little Endian
#ifdef _MSC_VER
#define NODE_BSWAP32(x) _byteswap_ulong(x)
#define NODE_BSWAP64(x) _byteswap_uint64(x)
#else
#define NODE_BSWAP32(x) __builtin_bswap32(x)
#define NODE_BSWAP64(x) __builtin_bswap64(x)
#endif
#else
// Big Endian
#define NODE_BSWAP32(x) (x)
#define NODE_BSWAP64(x) (x)
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


inline size_t base64_encode(const char* src,
                            size_t slen,
                            char* dst,
                            size_t dlen,
                            Base64Mode mode) {
  // We know how much we'll write, just make sure that there's space.
  CHECK(dlen >= base64_encoded_size(slen, mode) &&
        "not enough space provided for base64 encode");

  dlen = base64_encoded_size(slen, mode);

  unsigned a;
  unsigned b;
  unsigned c;
  unsigned i;
  unsigned k;
  unsigned n32;
  unsigned n64;

  const char* table = base64_select_table(mode);

  i = 0;
  k = 0;
  n32 = (slen >= 4 ? ((slen - 1) / 3) * 3 : 0);
  n64 = (slen >= 8 ? ((slen - 2) / 6) * 3 : 0);

  // Read in chunks of 8 bytes for as long as possible
  while (i < n64) {
    const uint64_t dword = *reinterpret_cast<const uint64_t*>(src + i);
    const uint64_t swapped = NODE_BSWAP64(dword);

    dst[k + 0] = table[(swapped >> 58) & 0x3f];
    dst[k + 1] = table[(swapped >> 52) & 0x3f];
    dst[k + 2] = table[(swapped >> 46) & 0x3f];
    dst[k + 3] = table[(swapped >> 40) & 0x3f];
    dst[k + 4] = table[(swapped >> 34) & 0x3f];
    dst[k + 5] = table[(swapped >> 28) & 0x3f];
    dst[k + 6] = table[(swapped >> 22) & 0x3f];
    dst[k + 7] = table[(swapped >> 16) & 0x3f];

    i += 6;
    k += 8;
  }

  // Read in chunks of 4 bytes for as long as possible
  while (i < n32) {
    const uint32_t dword = *reinterpret_cast<const uint32_t*>(src + i);
    const uint32_t swapped = NODE_BSWAP32(dword);

    dst[k + 0] = table[(swapped >> 26) & 0x3f];
    dst[k + 1] = table[(swapped >> 20) & 0x3f];
    dst[k + 2] = table[(swapped >> 14) & 0x3f];
    dst[k + 3] = table[(swapped >> 8) & 0x3f];

    i += 3;
    k += 4;
  }

  // Deal with leftover bytes
  switch (slen - n32) {
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
    case 3:
      a = src[i + 0] & 0xff;
      b = src[i + 1] & 0xff;
      c = src[i + 2] & 0xff;
      dst[k + 0] = table[a >> 2];
      dst[k + 1] = table[((a & 3) << 4) | (b >> 4)];
      dst[k + 2] = table[((b & 0x0f) << 2) | (c >> 6)];
      dst[k + 3] = table[c & 0x3f];
      break;
  }

  return dlen;
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_BASE64_INL_H_

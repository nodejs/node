#pragma once

#include "util.h"
#include <cmath>
#include <cstdint>

namespace node {

enum class Base64Mode {
  NORMAL,
  URL
};

// Calculates the size of the base64-encoded data
constexpr size_t base64_encoded_size(size_t size, Base64Mode mode = Base64Mode::NORMAL) {
  return mode == Base64Mode::NORMAL ? ((size + 2) / 3 * 4)
                                    : static_cast<size_t>(std::ceil(static_cast<double>(size * 4) / 3));
}

// Calculates the size of the base64-decoded data without checking padding
constexpr size_t base64_decoded_size_fast(size_t size) {
  // 1-byte input cannot be decoded
  return size > 1 ? (size / 4) * 3 + (size % 4 + 1) / 2 : 0;
}

// Reads a 32-bit unsigned integer in big-endian format
uint32_t ReadUint32BE(const unsigned char* p);

// Calculates the size of the base64-decoded data
template <typename TypeName>
size_t base64_decoded_size(const TypeName* src, size_t size);

// Decodes base64-encoded data
template <typename TypeName>
size_t base64_decode(char* const dst, const size_t dstlen, const TypeName* const src, const size_t srclen);

// Encodes data to base64
size_t base64_encode(const char* src, size_t slen, char* dst, size_t dlen, Base64Mode mode = Base64Mode::NORMAL);

}  // namespace node

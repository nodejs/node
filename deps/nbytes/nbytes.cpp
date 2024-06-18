#include "nbytes.h"
#include <string.h>
#include <cmath>
#include <cstddef>

namespace nbytes {

// ============================================================================
// Byte Swapping

namespace {
// These are defined by <sys/byteorder.h> or <netinet/in.h> on some systems.
// To avoid warnings, undefine them before redefining them.
#ifdef BSWAP_2
# undef BSWAP_2
#endif
#ifdef BSWAP_4
# undef BSWAP_4
#endif
#ifdef BSWAP_8
# undef BSWAP_8
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#define BSWAP_2(x) _byteswap_ushort(x)
#define BSWAP_4(x) _byteswap_ulong(x)
#define BSWAP_8(x) _byteswap_uint64(x)
#else
#define BSWAP_2(x) ((x) << 8) | ((x) >> 8)
#define BSWAP_4(x)                                                            \
  (((x) & 0xFF) << 24) |                                                      \
  (((x) & 0xFF00) << 8) |                                                     \
  (((x) >> 8) & 0xFF00) |                                                     \
  (((x) >> 24) & 0xFF)
#define BSWAP_8(x)                                                            \
  (((x) & 0xFF00000000000000ull) >> 56) |                                     \
  (((x) & 0x00FF000000000000ull) >> 40) |                                     \
  (((x) & 0x0000FF0000000000ull) >> 24) |                                     \
  (((x) & 0x000000FF00000000ull) >> 8) |                                      \
  (((x) & 0x00000000FF000000ull) << 8) |                                      \
  (((x) & 0x0000000000FF0000ull) << 24) |                                     \
  (((x) & 0x000000000000FF00ull) << 40) |                                     \
  (((x) & 0x00000000000000FFull) << 56)
#endif
}  // namespace

bool SwapBytes16(void* data, size_t nbytes) {
  if (nbytes % sizeof(uint16_t) != 0) return false;

#if defined(_MSC_VER)
  if (AlignUp(data, sizeof(uint16_t)) == data) {
    // MSVC has no strict aliasing, and is able to highly optimize this case.
    uint16_t* data16 = reinterpret_cast<uint16_t*>(data);
    size_t len16 = nbytes / sizeof(uint16_t);
    for (size_t i = 0; i < len16; i++) {
      data16[i] = BSWAP_2(data16[i]);
    }
    return;
  }
#endif

  uint16_t temp;
  uint8_t* ptr = reinterpret_cast<uint8_t*>(data);
  for (size_t i = 0; i < nbytes; i += sizeof(uint16_t)) {
    memcpy(&temp, &ptr[i], sizeof(uint16_t));
    temp = BSWAP_2(temp);
    memcpy(&ptr[i], &temp, sizeof(uint16_t));
  }

  return true;
}

bool SwapBytes32(void* data, size_t nbytes) {
  if (nbytes % sizeof(uint32_t) != 0) return false;

#if defined(_MSC_VER)
  // MSVC has no strict aliasing, and is able to highly optimize this case.
  if (AlignUp(data, sizeof(uint32_t)) == data) {
    uint32_t* data32 = reinterpret_cast<uint32_t*>(data);
    size_t len32 = nbytes / sizeof(uint32_t);
    for (size_t i = 0; i < len32; i++) {
      data32[i] = BSWAP_4(data32[i]);
    }
    return;
  }
#endif

  uint32_t temp = 0;
  uint8_t* ptr = reinterpret_cast<uint8_t*>(data);
  for (size_t i = 0; i < nbytes; i += sizeof(uint32_t)) {
    memcpy(&temp, &ptr[i], sizeof(uint32_t));
    temp = BSWAP_4(temp);
    memcpy(&ptr[i], &temp, sizeof(uint32_t));
  }

  return true;
}

bool SwapBytes64(void* data, size_t nbytes) {
  if (nbytes % sizeof(uint64_t) != 0) return false;

#if defined(_MSC_VER)
  if (AlignUp(data, sizeof(uint64_t)) == data) {
    // MSVC has no strict aliasing, and is able to highly optimize this case.
    uint64_t* data64 = reinterpret_cast<uint64_t*>(data);
    size_t len64 = nbytes / sizeof(uint64_t);
    for (size_t i = 0; i < len64; i++) {
      data64[i] = BSWAP_8(data64[i]);
    }
    return;
  }
#endif

  uint64_t temp = 0;
  uint8_t* ptr = reinterpret_cast<uint8_t*>(data);
  for (size_t i = 0; i < nbytes; i += sizeof(uint64_t)) {
    memcpy(&temp, &ptr[i], sizeof(uint64_t));
    temp = BSWAP_8(temp);
    memcpy(&ptr[i], &temp, sizeof(uint64_t));
  }

  return true;
}

// ============================================================================
// Base64 (legacy)

// supports regular and URL-safe base64
const int8_t unbase64_table[256] =
  { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -2, -1, -1, -2, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, 62, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, 63,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
  };



}  // namespace nbytes

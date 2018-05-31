/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "misc/MurmurHash.h"

using namespace antlr4::misc;

// A variation of the MurmurHash3 implementation
// (https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp) Here
// we unrolled the loop used there into individual calls to update(), as we
// usually hash object fields instead of entire buffers.

// Platform-specific functions and macros

// Microsoft Visual Studio

#if defined(_MSC_VER)

#define FORCE_INLINE __forceinline

#include <stdlib.h>

#define ROTL32(x, y) _rotl(x, y)
#define ROTL64(x, y) _rotl64(x, y)

#define BIG_CONSTANT(x) (x)

#else  // defined(_MSC_VER)

// Other compilers

#define FORCE_INLINE inline __attribute__((always_inline))

inline uint32_t rotl32(uint32_t x, int8_t r) {
  return (x << r) | (x >> (32 - r));
}

inline uint64_t rotl64(uint64_t x, int8_t r) {
  return (x << r) | (x >> (64 - r));
}

#define ROTL32(x, y) rotl32(x, y)
#define ROTL64(x, y) rotl64(x, y)

#define BIG_CONSTANT(x) (x##LLU)

#endif  // !defined(_MSC_VER)

size_t MurmurHash::initialize() { return initialize(DEFAULT_SEED); }

size_t MurmurHash::initialize(size_t seed) { return seed; }

#if defined(_WIN32) || defined(_WIN64)
#if _WIN64
#define ENVIRONMENT64
#else
#define ENVIRONMENT32
#endif
#endif

#if defined(__GNUC__)
#if defined(__x86_64__) || defined(__ppc64__)
#define ENVIRONMENT64
#else
#define ENVIRONMENT32
#endif
#endif

#if defined(ENVIRONMENT32)

size_t MurmurHash::update(size_t hash, size_t value) {
  static const size_t c1 = 0xCC9E2D51;
  static const size_t c2 = 0x1B873593;

  size_t k1 = value;
  k1 *= c1;
  k1 = ROTL32(k1, 15);
  k1 *= c2;

  hash ^= k1;
  hash = ROTL32(hash, 13);
  hash = hash * 5 + 0xE6546B64;

  return hash;
}

size_t MurmurHash::finish(size_t hash, size_t entryCount) {
  hash ^= entryCount * 4;
  hash ^= hash >> 16;
  hash *= 0x85EBCA6B;
  hash ^= hash >> 13;
  hash *= 0xC2B2AE35;
  hash ^= hash >> 16;
  return hash;
}

#else

size_t MurmurHash::update(size_t hash, size_t value) {
  static const size_t c1 = BIG_CONSTANT(0x87c37b91114253d5);
  static const size_t c2 = BIG_CONSTANT(0x4cf5ad432745937f);

  size_t k1 = value;
  k1 *= c1;
  k1 = ROTL64(k1, 31);
  k1 *= c2;

  hash ^= k1;
  hash = ROTL64(hash, 27);
  hash = hash * 5 + 0x52dce729;

  return hash;
}

size_t MurmurHash::finish(size_t hash, size_t entryCount) {
  hash ^= entryCount * 8;
  hash ^= hash >> 33;
  hash *= 0xff51afd7ed558ccd;
  hash ^= hash >> 33;
  hash *= 0xc4ceb9fe1a85ec53;
  hash ^= hash >> 33;
  return hash;
}

#endif

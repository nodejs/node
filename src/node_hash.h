#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

// Fast, high-quality hash function for byte sequences.
//
// Provides HashBytes() for use in hash tables. Uses native-width integer
// loads and a 128-bit multiply-and-fold mixer for excellent avalanche
// properties on short byte sequences (network identifiers, addresses,
// tokens).
//
// Based on rapidhash V3 by Nicolas De Carli, which evolved from wyhash
// by Wang Yi. Both use the same core mixing primitive (MUM: multiply,
// then XOR the high and low halves of the 128-bit result).
//
//   rapidhash: https://github.com/Nicoshev/rapidhash
//     Copyright (C) 2025 Nicolas De Carli — MIT License
//   wyhash:    https://github.com/wangyi-fudan/wyhash
//     Wang Yi — public domain (The Unlicense)
//
// The implementation here uses rapidhash's read strategy (native-width
// overlapping reads, optimized for short inputs) and secret constants.
// The core mixing function (rapid_mum/rapid_mix) is identical to
// wyhash's wymum/wymix.

#include <cstddef>
#include <cstdint>
#include <cstring>

#if defined(_MSC_VER)
#include <intrin.h>
#if defined(_M_X64) && !defined(_M_ARM64EC)
#pragma intrinsic(_umul128)
#endif
#endif

namespace node {

namespace hash_detail {

// 128-bit multiply, then XOR the high and low halves.
// This is the core mixing function ("rapid_mum" / "wymum").
// On 64-bit platforms with __int128, this compiles to a single
// mul instruction + shift + xor.
inline uint64_t RapidMix(uint64_t a, uint64_t b) {
#ifdef __SIZEOF_INT128__
  __uint128_t r = static_cast<__uint128_t>(a) * b;
  a = static_cast<uint64_t>(r);
  b = static_cast<uint64_t>(r >> 64);
#elif defined(_MSC_VER) && (defined(_WIN64) || defined(_M_HYBRID_CHPE_ARM64))
#if defined(_M_X64)
  a = _umul128(a, b, &b);
#else
  uint64_t hi = __umulh(a, b);
  a = a * b;
  b = hi;
#endif
#else
  // Portable 64x64 -> 128-bit multiply fallback for 32-bit platforms.
  uint64_t ha = a >> 32, hb = b >> 32;
  uint64_t la = static_cast<uint32_t>(a), lb = static_cast<uint32_t>(b);
  uint64_t rh = ha * hb, rm0 = ha * lb, rm1 = hb * la, rl = la * lb;
  uint64_t t = rl + (rm0 << 32);
  uint64_t lo = t + (rm1 << 32);
  uint64_t hi = rh + (rm0 >> 32) + (rm1 >> 32) + (t < rl) + (lo < t);
  a = lo;
  b = hi;
#endif
  return a ^ b;
}

// Inline 128-bit multiply WITHOUT the final XOR (used in the
// penultimate mixing step where a and b are updated separately).
inline void RapidMum(uint64_t* a, uint64_t* b) {
#ifdef __SIZEOF_INT128__
  __uint128_t r = static_cast<__uint128_t>(*a) * (*b);
  *a = static_cast<uint64_t>(r);
  *b = static_cast<uint64_t>(r >> 64);
#elif defined(_MSC_VER) && (defined(_WIN64) || defined(_M_HYBRID_CHPE_ARM64))
#if defined(_M_X64)
  *a = _umul128(*a, *b, b);
#else
  uint64_t hi = __umulh(*a, *b);
  *a = (*a) * (*b);
  *b = hi;
#endif
#else
  uint64_t ha = *a >> 32, hb = *b >> 32;
  uint64_t la = static_cast<uint32_t>(*a), lb = static_cast<uint32_t>(*b);
  uint64_t rh = ha * hb, rm0 = ha * lb, rm1 = hb * la, rl = la * lb;
  uint64_t t = rl + (rm0 << 32);
  *a = t + (rm1 << 32);
  *b = rh + (rm0 >> 32) + (rm1 >> 32) + (t < rl) + (*a < t);
#endif
}

// Read functions. The compiler optimizes small fixed-size memcpy calls
// to single load instructions — no actual byte-by-byte copy occurs.
inline uint64_t RapidRead64(const uint8_t* p) {
  uint64_t v;
  memcpy(&v, p, sizeof(uint64_t));
  return v;
}

inline uint64_t RapidRead32(const uint8_t* p) {
  uint32_t v;
  memcpy(&v, p, sizeof(uint32_t));
  return v;
}

// Default rapidhash secret parameters.
constexpr uint64_t kSecret[8] = {0x2d358dccaa6c78a5ULL,
                                 0x8bb84b93962eacc9ULL,
                                 0x4b33a62ed433d4a3ULL,
                                 0x4d5a2da51de1aa47ULL,
                                 0xa0761d6478bd642fULL,
                                 0xe7037ed1a0b428dbULL,
                                 0x90ed1765281c388cULL,
                                 0xaaaaaaaaaaaaaaaaULL};

}  // namespace hash_detail

// Hash a contiguous byte range. Optimized for short inputs (≤48 bytes)
// which is the common case for network identifiers and addresses. For
// inputs >48 bytes, falls through to a loop processing 48-byte chunks.
inline size_t HashBytes(const void* data, size_t len) {
  const uint8_t* p = static_cast<const uint8_t*>(data);

  // Seed initialization.
  uint64_t seed = hash_detail::RapidMix(0 ^ hash_detail::kSecret[2],
                                        hash_detail::kSecret[1]);
  uint64_t a = 0;
  uint64_t b = 0;
  size_t i = len;

  if (len <= 16) {
    if (len >= 4) {
      // Mix length into seed for better distribution of
      // different-length inputs with shared prefixes.
      seed ^= len;
      if (len >= 8) {
        // 8-16 bytes: two native 64-bit reads (overlapping from end).
        a = hash_detail::RapidRead64(p);
        b = hash_detail::RapidRead64(p + len - 8);
      } else {
        // 4-7 bytes: two 32-bit reads (overlapping from end).
        a = hash_detail::RapidRead32(p);
        b = hash_detail::RapidRead32(p + len - 4);
      }
    } else if (len > 0) {
      // 1-3 bytes: spread bytes across two values for mixing.
      a = (static_cast<uint64_t>(p[0]) << 45) | p[len - 1];
      b = p[len >> 1];
    } else {
      a = b = 0;
    }
  } else if (len <= 48) {
    // 17-48 bytes: process in 16-byte chunks, then read the tail.
    seed = hash_detail::RapidMix(
        hash_detail::RapidRead64(p) ^ hash_detail::kSecret[2],
        hash_detail::RapidRead64(p + 8) ^ seed);
    if (len > 32) {
      seed = hash_detail::RapidMix(
          hash_detail::RapidRead64(p + 16) ^ hash_detail::kSecret[2],
          hash_detail::RapidRead64(p + 24) ^ seed);
    }
    a = hash_detail::RapidRead64(p + len - 16) ^ len;
    b = hash_detail::RapidRead64(p + len - 8);
  } else {
    // >48 bytes: process 48-byte chunks with three parallel mix lanes.
    uint64_t see1 = seed;
    uint64_t see2 = seed;
    do {
      seed = hash_detail::RapidMix(
          hash_detail::RapidRead64(p) ^ hash_detail::kSecret[0],
          hash_detail::RapidRead64(p + 8) ^ seed);
      see1 = hash_detail::RapidMix(
          hash_detail::RapidRead64(p + 16) ^ hash_detail::kSecret[1],
          hash_detail::RapidRead64(p + 24) ^ see1);
      see2 = hash_detail::RapidMix(
          hash_detail::RapidRead64(p + 32) ^ hash_detail::kSecret[2],
          hash_detail::RapidRead64(p + 40) ^ see2);
      p += 48;
      i -= 48;
    } while (i > 48);
    seed ^= see1 ^ see2;
    // Process remaining 17-48 bytes.
    if (i > 16) {
      seed = hash_detail::RapidMix(
          hash_detail::RapidRead64(p) ^ hash_detail::kSecret[2],
          hash_detail::RapidRead64(p + 8) ^ seed);
      if (i > 32) {
        seed = hash_detail::RapidMix(
            hash_detail::RapidRead64(p + 16) ^ hash_detail::kSecret[2],
            hash_detail::RapidRead64(p + 24) ^ seed);
      }
    }
    a = hash_detail::RapidRead64(p + i - 16) ^ i;
    b = hash_detail::RapidRead64(p + i - 8);
  }

  // Final mix.
  a ^= hash_detail::kSecret[1];
  b ^= seed;
  hash_detail::RapidMum(&a, &b);
  return static_cast<size_t>(hash_detail::RapidMix(
      a ^ hash_detail::kSecret[7], b ^ hash_detail::kSecret[1] ^ len));
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

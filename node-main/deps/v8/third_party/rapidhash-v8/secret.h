/*
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <https://unlicense.org/>
 *
 * author: 王一 Wang Yi <godspeed_china@yeah.net>
 * contributors: Reini Urban, Dietrich Epp, Joshua Haberman, Tommy Ettinger,
 * Daniel Lemire, Otmar Ertl, cocowalla, leo-yuriev, Diego Barrios Romero,
 * paulie-g, dumblob, Yann Collet, ivte-ms, hyb, James Z.M. Gao, easyaspi314
 * (Devin), TheOneric
 */

#ifndef _THIRD_PARTY_RAPIDHASH_SECRET_H
#define _THIRD_PARTY_RAPIDHASH_SECRET_H 1

#include "rapidhash.h"
#include "src/base/bits.h"

// Default secret parameters. If we wanted to, we could generate our own
// versions of these at renderer startup in order to perturb the hash
// and make it more DoS-resistant (similar to what base/hash.h does),
// but generating new ones takes a little bit of time (~200 µs on a desktop
// machine as of 2024), and good-quality random numbers may not be copious
// from within the sandbox. The secret concept is inherited from wyhash,
// described by the wyhash author here:
//
//   https://github.com/wangyi-fudan/wyhash/issues/139
//
// The rules are:
//
//   1. Each byte must be “balanced”, i.e., have exactly 4 bits set.
//      (This is trivially done by just precompting a LUT with the
//      possible bytes and picking from those.)
//
//   2. Each 64-bit group should have a Hamming distance of 32 to
//      all the others (i.e., popcount(secret[i] ^ secret[j]) == 32).
//      This is just done by rejection sampling.
//
//   3. Each 64-bit group should be prime. It's not obvious that this
//      is really needed for the hash, as opposed to wyrand which also
//      uses the same secret, but according to the author, it is
//      “a feeling to be perfect”. This naturally means the last byte
//      cannot be divisible by 2, but apart from that, is easiest
//      checked by testing a few small factors and then the Miller-Rabin
//      test, which rejects nearly all bad candidates in the first iteration
//      and is fast as long as we have 64x64 -> 128 bit muls and modulos.

static constexpr uint64_t RAPIDHASH_DEFAULT_SECRET[3] = {
    0x2d358dccaa6c78a5ull, 0x8bb84b93962eacc9ull, 0x4b33a62ed433d4a3ull};

#if !V8_USE_DEFAULT_HASHER_SECRET

namespace detail {

static inline uint64_t wyrand(uint64_t* seed) {
  *seed += 0x2d358dccaa6c78a5ull;
  return rapid_mix(*seed, *seed ^ 0x8bb84b93962eacc9ull);
}

static inline unsigned long long mul_mod(unsigned long long a,
                                         unsigned long long b,
                                         unsigned long long m) {
  unsigned long long r = 0;
  while (b) {
    if (b & 1) {
      unsigned long long r2 = r + a;
      if (r2 < r) r2 -= m;
      r = r2 % m;
    }
    b >>= 1;
    if (b) {
      unsigned long long a2 = a + a;
      if (a2 < a) a2 -= m;
      a = a2 % m;
    }
  }
  return r;
}

static inline unsigned long long pow_mod(unsigned long long a,
                                         unsigned long long b,
                                         unsigned long long m) {
  unsigned long long r = 1;
  while (b) {
    if (b & 1) r = mul_mod(r, a, m);
    b >>= 1;
    if (b) a = mul_mod(a, a, m);
  }
  return r;
}

static unsigned sprp(unsigned long long n, unsigned long long a) {
  unsigned long long d = n - 1;
  unsigned char s = 0;
  while (!(d & 0xff)) {
    d >>= 8;
    s += 8;
  }
  if (!(d & 0xf)) {
    d >>= 4;
    s += 4;
  }
  if (!(d & 0x3)) {
    d >>= 2;
    s += 2;
  }
  if (!(d & 0x1)) {
    d >>= 1;
    s += 1;
  }
  unsigned long long b = pow_mod(a, d, n);
  if ((b == 1) || (b == (n - 1))) return 1;
  unsigned char r;
  for (r = 1; r < s; r++) {
    b = mul_mod(b, b, n);
    if (b <= 1) return 0;
    if (b == (n - 1)) return 1;
  }
  return 0;
}

static unsigned is_prime(unsigned long long n) {
  if (n < 2 || !(n & 1)) return 0;
  if (n < 4) return 1;
  if (!sprp(n, 2)) return 0;
  if (n < 2047) return 1;
  if (!sprp(n, 3)) return 0;
  if (!sprp(n, 5)) return 0;
  if (!sprp(n, 7)) return 0;
  if (!sprp(n, 11)) return 0;
  if (!sprp(n, 13)) return 0;
  if (!sprp(n, 17)) return 0;
  if (!sprp(n, 19)) return 0;
  if (!sprp(n, 23)) return 0;
  if (!sprp(n, 29)) return 0;
  if (!sprp(n, 31)) return 0;
  if (!sprp(n, 37)) return 0;
  return 1;
}

}  // namespace detail

static inline void rapidhash_make_secret(uint64_t seed, uint64_t* secret) {
  uint8_t c[] = {15,  23,  27,  29,  30,  39,  43,  45,  46,  51,  53,  54,
                 57,  58,  60,  71,  75,  77,  78,  83,  85,  86,  89,  90,
                 92,  99,  101, 102, 105, 106, 108, 113, 114, 116, 120, 135,
                 139, 141, 142, 147, 149, 150, 153, 154, 156, 163, 165, 166,
                 169, 170, 172, 177, 178, 180, 184, 195, 197, 198, 201, 202,
                 204, 209, 210, 212, 216, 225, 226, 228, 232, 240};
  for (size_t i = 0; i < 3; i++) {
    uint8_t ok;
    do {
      ok = 1;
      secret[i] = 0;
      for (size_t j = 0; j < 64; j += 8)
        secret[i] |= static_cast<uint64_t>(c[detail::wyrand(&seed) % sizeof(c)])
                     << j;
      if (secret[i] % 2 == 0) {
        ok = 0;
        continue;
      }
      for (size_t j = 0; j < i; j++) {
        if (v8::base::bits::CountPopulation(secret[j] ^ secret[i]) != 32) {
          ok = 0;
          break;
        }
      }
      if (ok && !detail::is_prime(secret[i])) {
        ok = 0;
      }
    } while (!ok);
  }
}

#endif  // !V8_USE_DEFAULT_HASHER_SECRET

#endif  // _THIRD_PARTY_RAPIDHASH_SECRET_H

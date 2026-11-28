#ifndef TOOLS_EMBED_SHA256_H_
#define TOOLS_EMBED_SHA256_H_

// SHA-256 of an embedded blob. Shared by the host compressor and node so the
// cache filename and the header digest match.

#include <cstddef>
#include <cstdint>
#include <cstring>

inline uint32_t EmbedSha256Rotr(uint32_t x, uint32_t n) {
  return (x >> n) | (x << (32 - n));
}

inline void EmbedSha256Transform(uint32_t state[8], const uint8_t block[64]) {
  static const uint32_t k[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
      0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
      0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
      0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
      0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
      0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
      0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
      0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
      0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  uint32_t w[64];
  for (int i = 0; i < 16; i++) {
    w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
           (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
           (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
           static_cast<uint32_t>(block[i * 4 + 3]);
  }
  for (int i = 16; i < 64; i++) {
    uint32_t s0 = EmbedSha256Rotr(w[i - 15], 7) ^
                  EmbedSha256Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    uint32_t s1 = EmbedSha256Rotr(w[i - 2], 17) ^
                  EmbedSha256Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  uint32_t a = state[0];
  uint32_t b = state[1];
  uint32_t c = state[2];
  uint32_t d = state[3];
  uint32_t e = state[4];
  uint32_t f = state[5];
  uint32_t g = state[6];
  uint32_t h = state[7];
  for (int i = 0; i < 64; i++) {
    uint32_t s1 = EmbedSha256Rotr(e, 6) ^ EmbedSha256Rotr(e, 11) ^
                  EmbedSha256Rotr(e, 25);
    uint32_t ch = (e & f) ^ (~e & g);
    uint32_t t1 = h + s1 + ch + k[i] + w[i];
    uint32_t s0 = EmbedSha256Rotr(a, 2) ^ EmbedSha256Rotr(a, 13) ^
                  EmbedSha256Rotr(a, 22);
    uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    uint32_t t2 = s0 + maj;
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}

inline void EmbedSha256(const uint8_t* data, size_t len, uint8_t out[32]) {
  uint32_t state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                       0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  uint8_t block[64];
  size_t off = 0;
  while (len - off >= 64) {
    EmbedSha256Transform(state, data + off);
    off += 64;
  }
  size_t rem = len - off;
  memcpy(block, data + off, rem);
  block[rem++] = 0x80;
  if (rem > 56) {
    memset(block + rem, 0, 64 - rem);
    EmbedSha256Transform(state, block);
    rem = 0;
  }
  memset(block + rem, 0, 56 - rem);
  uint64_t bits = static_cast<uint64_t>(len) * 8;
  for (int i = 0; i < 8; i++) {
    block[63 - i] = static_cast<uint8_t>(bits >> (8 * i));
  }
  EmbedSha256Transform(state, block);
  for (int i = 0; i < 8; i++) {
    out[i * 4] = static_cast<uint8_t>(state[i] >> 24);
    out[i * 4 + 1] = static_cast<uint8_t>(state[i] >> 16);
    out[i * 4 + 2] = static_cast<uint8_t>(state[i] >> 8);
    out[i * 4 + 3] = static_cast<uint8_t>(state[i]);
  }
}

#endif  // TOOLS_EMBED_SHA256_H_

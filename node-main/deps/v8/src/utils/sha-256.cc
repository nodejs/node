// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2013 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ========================================================================
//
// Optimized for minimal code size.
//
// This code originates from the Omaha installer for Windows but is
// reduced in complexity. Changes made are outlined in the header file.

#include "src/utils/sha-256.h"

#include <stdint.h>
#include <string.h>

#define ror(value, bits) (((value) >> (bits)) | ((value) << (32 - (bits))))
#define shr(value, bits) ((value) >> (bits))

namespace v8 {
namespace internal {

static const uint32_t K[64] = {
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

static void SHA256_Transform(LITE_SHA256_CTX* ctx) {
  uint32_t W[64];
  uint32_t A, B, C, D, E, F, G, H;
  uint8_t* p = ctx->buf;
  int t;

  for (t = 0; t < 16; ++t) {
    uint32_t tmp = (uint32_t)*p++ << 24;
    tmp |= (uint32_t)*p++ << 16;
    tmp |= (uint32_t)*p++ << 8;
    tmp |= (uint32_t)*p++;
    W[t] = tmp;
  }

  for (; t < 64; t++) {
    uint32_t s0 = ror(W[t - 15], 7) ^ ror(W[t - 15], 18) ^ shr(W[t - 15], 3);
    uint32_t s1 = ror(W[t - 2], 17) ^ ror(W[t - 2], 19) ^ shr(W[t - 2], 10);
    W[t] = W[t - 16] + s0 + W[t - 7] + s1;
  }

  A = ctx->state[0];
  B = ctx->state[1];
  C = ctx->state[2];
  D = ctx->state[3];
  E = ctx->state[4];
  F = ctx->state[5];
  G = ctx->state[6];
  H = ctx->state[7];

  for (t = 0; t < 64; t++) {
    uint32_t s0 = ror(A, 2) ^ ror(A, 13) ^ ror(A, 22);
    uint32_t maj = (A & B) ^ (A & C) ^ (B & C);
    uint32_t t2 = s0 + maj;
    uint32_t s1 = ror(E, 6) ^ ror(E, 11) ^ ror(E, 25);
    uint32_t ch = (E & F) ^ ((~E) & G);
    uint32_t t1 = H + s1 + ch + K[t] + W[t];

    H = G;
    G = F;
    F = E;
    E = D + t1;
    D = C;
    C = B;
    B = A;
    A = t1 + t2;
  }

  ctx->state[0] += A;
  ctx->state[1] += B;
  ctx->state[2] += C;
  ctx->state[3] += D;
  ctx->state[4] += E;
  ctx->state[5] += F;
  ctx->state[6] += G;
  ctx->state[7] += H;
}

static const HASH_VTAB SHA256_VTAB = {
    SHA256_init, SHA256_update, SHA256_final, SHA256_hash, kSizeOfSha256Digest,
};

void SHA256_init(LITE_SHA256_CTX* ctx) {
  ctx->f = &SHA256_VTAB;
  ctx->state[0] = 0x6a09e667;
  ctx->state[1] = 0xbb67ae85;
  ctx->state[2] = 0x3c6ef372;
  ctx->state[3] = 0xa54ff53a;
  ctx->state[4] = 0x510e527f;
  ctx->state[5] = 0x9b05688c;
  ctx->state[6] = 0x1f83d9ab;
  ctx->state[7] = 0x5be0cd19;
  ctx->count = 0;
}

void SHA256_update(LITE_SHA256_CTX* ctx, const void* data, size_t len) {
  int i = static_cast<int>(ctx->count & 63);
  const uint8_t* p = (const uint8_t*)data;

  ctx->count += len;

  while (len--) {
    ctx->buf[i++] = *p++;
    if (i == 64) {
      SHA256_Transform(ctx);
      i = 0;
    }
  }
}

const uint8_t* SHA256_final(LITE_SHA256_CTX* ctx) {
  uint8_t* p = ctx->buf;
  uint64_t cnt = LITE_LShiftU64(ctx->count, 3);
  int i;

  const uint8_t completion[] { 0x80, 0 };

  SHA256_update(ctx, &completion[0], 1);
  while ((ctx->count & 63) != 56) {
    SHA256_update(ctx, &completion[1], 1);
  }
  for (i = 0; i < 8; ++i) {
    uint8_t tmp = (uint8_t)LITE_RShiftU64(cnt, 56);
    cnt = LITE_LShiftU64(cnt, 8);
    SHA256_update(ctx, &tmp, 1);
  }

  for (i = 0; i < 8; i++) {
    uint32_t tmp = ctx->state[i];
    *p++ = (uint8_t)(tmp >> 24);
    *p++ = (uint8_t)(tmp >> 16);
    *p++ = (uint8_t)(tmp >> 8);
    *p++ = (uint8_t)(tmp >> 0);
  }

  return ctx->buf;
}

/* Convenience function */
const uint8_t* SHA256_hash(const void* data, size_t len, uint8_t* digest) {
  LITE_SHA256_CTX ctx;
  SHA256_init(&ctx);
  SHA256_update(&ctx, data, len);
  memcpy(digest, SHA256_final(&ctx), kSizeOfSha256Digest);
  return digest;
}

}  // namespace internal
}  // namespace v8

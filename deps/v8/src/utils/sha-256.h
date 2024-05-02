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
// This code originates from the Omaha installer for Windows:
//   https://github.com/google/omaha
// The following changes were made:
//  - Combined the hash-internal.h and sha256.h headers together to form
//    this one.
//  - Eliminated conditional definitions related to LITE_EMULATED_64BIT_OPS
//  - Eliminated `extern "C"` definitions as these aren't exported
//  - Eliminated `SHA512_SUPPORT` as we only support SHA256
//  - Eliminated generic `HASH_` definitions as unnecessary
//  - Moved the hashing functions into `namespace v8::internal`
//
// This is intended simply to provide a minimal-impact SHA256 hash utility
// to support the stack trace source hash functionality.

#ifndef V8_UTILS_SHA_256_H_
#define V8_UTILS_SHA_256_H_

#include <stddef.h>
#include <stdint.h>

#define LITE_LShiftU64(a, b) ((a) << (b))
#define LITE_RShiftU64(a, b) ((a) >> (b))

const size_t kSizeOfSha256Digest = 32;
const size_t kSizeOfFormattedSha256Digest = (kSizeOfSha256Digest * 2) + 1;

namespace v8 {
namespace internal {

typedef struct HASH_VTAB {
  void (*const init)(struct HASH_CTX*);
  void (*const update)(struct HASH_CTX*, const void*, size_t);
  const uint8_t* (*const final)(struct HASH_CTX*);
  const uint8_t* (*const hash)(const void*, size_t, uint8_t*);
  unsigned int size;
} HASH_VTAB;

typedef struct HASH_CTX {
  const HASH_VTAB* f;
  uint64_t count;
  uint8_t buf[64];
  uint32_t state[8];  // upto SHA2-256
} HASH_CTX;

typedef HASH_CTX LITE_SHA256_CTX;

void SHA256_init(LITE_SHA256_CTX* ctx);
void SHA256_update(LITE_SHA256_CTX* ctx, const void* data, size_t len);
const uint8_t* SHA256_final(LITE_SHA256_CTX* ctx);

// Convenience method. Returns digest address.
const uint8_t* SHA256_hash(const void* data, size_t len, uint8_t* digest);

}  // namespace internal
}  // namespace v8

#endif  // V8_UTILS_SHA_256_H_

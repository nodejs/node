// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_NUMBERS_HASH_SEED_INL_H_
#define V8_NUMBERS_HASH_SEED_INL_H_

#include "src/numbers/hash-seed.h"
#include "src/objects/fixed-array-inl.h"
#include "src/roots/roots-inl.h"
#include "third_party/rapidhash-v8/secret.h"

namespace v8 {
namespace internal {

inline HashSeed::HashSeed(Isolate* isolate)
    : HashSeed(ReadOnlyRoots(isolate)) {}

inline HashSeed::HashSeed(LocalIsolate* isolate)
    : HashSeed(ReadOnlyRoots(isolate)) {}

inline HashSeed::HashSeed(ReadOnlyRoots roots) {
  // roots.hash_seed is not aligned
  MemCopy(&seed_, roots.hash_seed()->begin(), sizeof(seed_));
  MemCopy(secret_, roots.hash_seed()->begin() + sizeof(seed_), sizeof(secret_));
}

inline HashSeed::HashSeed(uint64_t seed, const uint64_t secret[3])
    : seed_(seed),
      secret_{
          secret[0],
          secret[1],
          secret[2],
      } {}

inline HashSeed HashSeed::Default() {
  return HashSeed(0, RAPIDHASH_DEFAULT_SECRET);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_NUMBERS_HASH_SEED_INL_H_

// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_NUMBERS_HASH_SEED_INL_H_
#define V8_NUMBERS_HASH_SEED_INL_H_

#include "src/numbers/hash-seed.h"
#include "src/objects/fixed-array-inl.h"
#include "src/roots/roots-inl.h"

namespace v8 {
namespace internal {

inline HashSeed::HashSeed(Isolate* isolate)
    : HashSeed(ReadOnlyRoots(isolate)) {}

inline HashSeed::HashSeed(LocalIsolate* isolate)
    : HashSeed(ReadOnlyRoots(isolate)) {}

inline HashSeed::HashSeed(ReadOnlyRoots roots)
    : data_(reinterpret_cast<const Data*>(roots.hash_seed()->begin())) {}

inline HashSeed HashSeed::Default() { return HashSeed(kDefaultData); }

inline uint64_t HashSeed::seed() const { return data_->seed; }
inline const uint64_t* HashSeed::secret() const { return data_->secrets; }

#ifdef V8_ENABLE_SEEDED_ARRAY_INDEX_HASH
inline uint32_t HashSeed::m1() const { return data_->m1; }
inline uint32_t HashSeed::m1_inv() const { return data_->m1_inv; }
inline uint32_t HashSeed::m2() const { return data_->m2; }
inline uint32_t HashSeed::m2_inv() const { return data_->m2_inv; }
inline uint32_t HashSeed::m3() const { return data_->m3; }
inline uint32_t HashSeed::m3_inv() const { return data_->m3_inv; }
#endif  // V8_ENABLE_SEEDED_ARRAY_INDEX_HASH

}  // namespace internal
}  // namespace v8

#endif  // V8_NUMBERS_HASH_SEED_INL_H_

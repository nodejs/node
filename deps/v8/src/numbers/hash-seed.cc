// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/numbers/hash-seed.h"

#include <cstddef>

#include "src/execution/isolate.h"
#include "src/flags/flags.h"
#include "src/numbers/hash-seed-inl.h"
#include "src/objects/name.h"
#include "third_party/rapidhash-v8/secret.h"

namespace v8 {
namespace internal {

namespace {

#ifdef V8_ENABLE_SEEDED_ARRAY_INDEX_HASH
// Calculate the modular inverse using Newton's method.
constexpr uint32_t modular_inverse(uint32_t m) {
  uint32_t x = (3 * m) ^ 2;  // 5 correct bits
  x = x * (2 - m * x);       // 10 correct bits
  x = x * (2 - m * x);       // 20 correct bits
  x = x * (2 - m * x);       // 40 correct bits
  return x;
}

constexpr uint32_t truncate_for_derived_secrets(uint64_t s) {
  return static_cast<uint32_t>(s) & Name::kArrayIndexValueMask;
}

// Derive a multiplier from a rapidhash secret and ensure it's odd.
constexpr uint32_t derive_multiplier(uint64_t secret) {
  return truncate_for_derived_secrets(secret) | 1;
}

// Compute the modular inverse of the derived multiplier.
constexpr uint32_t derive_multiplier_inverse(uint64_t secret) {
  return truncate_for_derived_secrets(
      modular_inverse(derive_multiplier(secret)));
}

constexpr bool is_modular_inverse(uint32_t m, uint32_t m_inv) {
  return ((m * m_inv) & Name::kArrayIndexValueMask) == 1;
}

constexpr void DeriveSecretsForArrayIndexHash(HashSeed::Data* data) {
  data->m1 = derive_multiplier(data->secrets[0]);
  data->m1_inv = derive_multiplier_inverse(data->secrets[0]);
  data->m2 = derive_multiplier(data->secrets[1]);
  data->m2_inv = derive_multiplier_inverse(data->secrets[1]);
  data->m3 = derive_multiplier(data->secrets[2]);
  data->m3_inv = derive_multiplier_inverse(data->secrets[2]);
}
#endif  // V8_ENABLE_SEEDED_ARRAY_INDEX_HASH

static constexpr HashSeed::Data kDefaultSeed = [] {
  HashSeed::Data d{};
  d.seed = 0;
  d.secrets[0] = RAPIDHASH_DEFAULT_SECRET[0];
  d.secrets[1] = RAPIDHASH_DEFAULT_SECRET[1];
  d.secrets[2] = RAPIDHASH_DEFAULT_SECRET[2];
#ifdef V8_ENABLE_SEEDED_ARRAY_INDEX_HASH
  DeriveSecretsForArrayIndexHash(&d);
#endif  // V8_ENABLE_SEEDED_ARRAY_INDEX_HASH
  return d;
}();

}  // anonymous namespace

static_assert(HashSeed::kSecretsCount == arraysize(RAPIDHASH_DEFAULT_SECRET));
const HashSeed::Data* const HashSeed::kDefaultData = &kDefaultSeed;

#ifdef V8_ENABLE_SEEDED_ARRAY_INDEX_HASH
// Compile-time verification that m * m_inv === 1 for the derived secrets.
static_assert(is_modular_inverse(kDefaultSeed.m1, kDefaultSeed.m1_inv));
static_assert(is_modular_inverse(kDefaultSeed.m2, kDefaultSeed.m2_inv));
static_assert(is_modular_inverse(kDefaultSeed.m3, kDefaultSeed.m3_inv));
#endif  // V8_ENABLE_SEEDED_ARRAY_INDEX_HASH

// static
void HashSeed::InitializeRoots(Isolate* isolate) {
  DCHECK(!isolate->heap()->deserialization_complete());
  uint64_t seed;
  if (v8_flags.hash_seed == 0) {
    int64_t rnd = isolate->random_number_generator()->NextInt64();
    seed = static_cast<uint64_t>(rnd);
  } else {
    seed = static_cast<uint64_t>(v8_flags.hash_seed);
  }

  // Write the seed and derived secrets into the read-only roots ByteArray.
  Data* data = const_cast<Data*>(HashSeed(isolate).data_);

#if V8_USE_DEFAULT_HASHER_SECRET
  // Copy from the default seed and just override the meta seed.
  *data = kDefaultSeed;
  data->seed = seed;
#else
  data->seed = seed;
  rapidhash_make_secret(seed, data->secrets);
#ifdef V8_ENABLE_SEEDED_ARRAY_INDEX_HASH
  DeriveSecretsForArrayIndexHash(data);
  DCHECK(is_modular_inverse(data->m1, data->m1_inv));
  DCHECK(is_modular_inverse(data->m2, data->m2_inv));
  DCHECK(is_modular_inverse(data->m3, data->m3_inv));
#endif  // V8_ENABLE_SEEDED_ARRAY_INDEX_HASH
#endif  // V8_USE_DEFAULT_HASHER_SECRET
}

}  // namespace internal
}  // namespace v8

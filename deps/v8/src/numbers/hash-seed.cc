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

static constexpr HashSeed::Data kDefaultSeed = {
    0,
    {RAPIDHASH_DEFAULT_SECRET[0], RAPIDHASH_DEFAULT_SECRET[1],
     RAPIDHASH_DEFAULT_SECRET[2]}};

}  // anonymous namespace

static_assert(HashSeed::kSecretsCount == arraysize(RAPIDHASH_DEFAULT_SECRET));
const HashSeed::Data* const HashSeed::kDefaultData = &kDefaultSeed;

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
#endif  // V8_USE_DEFAULT_HASHER_SECRET
}

}  // namespace internal
}  // namespace v8

// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_NUMBERS_HASH_SEED_H_
#define V8_NUMBERS_HASH_SEED_H_

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "src/base/macros.h"

namespace v8 {
namespace internal {

class Isolate;
class LocalIsolate;
class ReadOnlyRoots;

// A lightweight view over the hash_seed ByteArray in read-only roots.
class V8_EXPORT_PRIVATE HashSeed {
 public:
  inline explicit HashSeed(Isolate* isolate);
  inline explicit HashSeed(LocalIsolate* isolate);
  inline explicit HashSeed(ReadOnlyRoots roots);

  static inline HashSeed Default();

  inline uint64_t seed() const;
  inline const uint64_t* secret() const;

  bool operator==(const HashSeed& b) const { return data_ == b.data_; }

  static constexpr int kSecretsCount = 3;

  // The ReadOnlyRoots::hash_seed() byte array can be interpreted
  // as a HashSeed::Data struct.
  // Since this maps over either the read-only roots or over a static byte
  // array, and in both cases, must be allocated at 8-byte boundaries,
  // we don't use V8_OBJECT here.
  struct Data {
    // meta seed from --hash-seed, 0 = generate at startup
    uint64_t seed;
    // When V8_USE_DEFAULT_HASHER_SECRET is enabled, these are just
    // RAPIDHASH_DEFAULT_SECRET. Otherwise they are derived from the seed
    // using rapidhash_make_secret().
    uint64_t secrets[kSecretsCount];
  };

  static constexpr int kTotalSize = sizeof(Data);

  // Generates a hash seed (from --hash-seed or the RNG) and writes it
  // together with derived secrets into the isolate's hash_seed in
  // its read-only roots.
  static void InitializeRoots(Isolate* isolate);

 private:
  // Pointer into the Data overlaying the ByteArray data (either
  // points to read-only roots or to kDefaultData).
  const Data* data_;
  HashSeed(const Data* data) : data_(data) {}

  // Points to the static constexpr default seed.
  static const Data* const kDefaultData;
};

static_assert(std::is_trivially_copyable_v<HashSeed>);
static_assert(alignof(HashSeed::Data) == alignof(uint64_t));

}  // namespace internal
}  // namespace v8

#endif  // V8_NUMBERS_HASH_SEED_H_

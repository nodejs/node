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

#ifdef V8_ENABLE_SEEDED_ARRAY_INDEX_HASH
    // Additional precomputed secrets for seeding the array index value hashes.
    uint32_t m1;  // lower kArrayIndexValueBits bits of secret[0], must be odd
    uint32_t m1_inv;  // modular inverse of m1 mod 2^kArrayIndexValueBits
    uint32_t m2;  // lower kArrayIndexValueBits bits of secret[1], must be odd
    uint32_t m2_inv;  // modular inverse of m2 mod 2^kArrayIndexValueBits
    uint32_t m3;  // lower kArrayIndexValueBits bits of secret[2], must be odd
    uint32_t m3_inv;  // modular inverse of m3 mod 2^kArrayIndexValueBits
#endif                // V8_ENABLE_SEEDED_ARRAY_INDEX_HASH
  };

  static constexpr int kTotalSize = sizeof(Data);

#ifdef V8_ENABLE_SEEDED_ARRAY_INDEX_HASH
  // Byte offsets from the data start, for CSA that loads fields at raw
  // offsets from the ByteArray data start.
  static constexpr int kDerivedM1Offset = offsetof(Data, m1);
  static constexpr int kDerivedM1InvOffset = offsetof(Data, m1_inv);
  static constexpr int kDerivedM2Offset = offsetof(Data, m2);
  static constexpr int kDerivedM2InvOffset = offsetof(Data, m2_inv);
  static constexpr int kDerivedM3Offset = offsetof(Data, m3);
  static constexpr int kDerivedM3InvOffset = offsetof(Data, m3_inv);

  inline uint32_t m1() const;
  inline uint32_t m1_inv() const;
  inline uint32_t m2() const;
  inline uint32_t m2_inv() const;
  inline uint32_t m3() const;
  inline uint32_t m3_inv() const;
#endif  // V8_ENABLE_SEEDED_ARRAY_INDEX_HASH

  // Generates a hash seed (from --hash-seed or the RNG) and writes it
  // together with derived secrets into the isolate's hash_seed in
  // its read-only roots.
  static void InitializeRoots(Isolate* isolate);

 private:
  // Pointer into the Data overlaying the ByteArray data (either
  // points to read-only roots or to kDefaultData).
  const Data* data_;
  explicit HashSeed(const Data* data) : data_(data) {}

  // Points to the static constexpr default seed.
  static const Data* const kDefaultData;
};

static_assert(std::is_trivially_copyable_v<HashSeed>);
static_assert(alignof(HashSeed::Data) == alignof(uint64_t));

}  // namespace internal
}  // namespace v8

#endif  // V8_NUMBERS_HASH_SEED_H_

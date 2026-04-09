// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STRINGS_STRING_HASHER_H_
#define V8_STRINGS_STRING_HASHER_H_

#include "src/common/globals.h"
#include "src/numbers/hash-seed.h"

namespace v8 {

namespace base {
template <typename T>
class Vector;
}  // namespace base

namespace internal {

// A simple incremental string hasher. Slow but allows for special casing each
// individual character.
class RunningStringHasher final {
 public:
  explicit RunningStringHasher(uint32_t seed) : running_hash_(seed) {}

  V8_INLINE void AddCharacter(uint16_t c);
  V8_INLINE uint32_t Finalize();

 private:
  uint32_t running_hash_;
};

// Helper class for incrementally calculating string hashes in a form suitable
// for storing into Name::raw_hash_field.
class V8_EXPORT_PRIVATE StringHasher final {
 public:
  StringHasher() = delete;
  template <typename char_t>
  static inline uint32_t HashSequentialString(const char_t* chars,
                                              uint32_t length,
                                              const HashSeed seed);

  // Calculate the hash value for a string consisting of 1 to
  // String::kMaxArrayIndexSize digits with no leading zeros (except "0").
  //
  // The entire hash field consists of (from least significant bit to most):
  //  - HashFieldType::kIntegerIndex
  //  - kArrayIndexValueBits::kSize bits containing the hash value
  //  - The length of the decimal string
  //
  // When V8_ENABLE_SEEDED_ARRAY_INDEX_HASH is enabled, the numeric value
  // is scrambled using secrets derived from the hash seed. When it's disabled
  // the public overloads ignore the seed, whose retrieval should be optimized
  // away in common configurations.
  static V8_INLINE uint32_t MakeArrayIndexHash(uint32_t value, uint32_t length,
                                               const HashSeed seed);
  // Decode array index value from raw hash field and reverse seeding, if any.
  static V8_INLINE uint32_t
  DecodeArrayIndexFromHashField(uint32_t raw_hash_field, const HashSeed seed);

  // No string is allowed to have a hash of zero.  That value is reserved
  // for internal properties.  If the hash calculation yields zero then we
  // use 27 instead.
  static const int kZeroHash = 27;

  static V8_INLINE uint32_t GetTrivialHash(uint32_t length);

 private:
  // Raw encode/decode without seeding. Use the public overloads above.
  static V8_INLINE uint32_t MakeArrayIndexHash(uint32_t value, uint32_t length);
  static V8_INLINE uint32_t
  DecodeArrayIndexFromHashField(uint32_t raw_hash_field);

#ifdef V8_ENABLE_SEEDED_ARRAY_INDEX_HASH
  // When V8_ENABLE_SEEDED_ARRAY_INDEX_HASH is enabled, the numeric value
  // will be scrambled with 3 rounds of xorshift-multiply.
  //
  //   x ^= x >> kShift;  x = (x * m1) & kMask;   // round 1
  //   x ^= x >> kShift;  x = (x * m2) & kMask;   // round 2
  //   x ^= x >> kShift;  x = (x * m3) & kMask;   // round 3
  //   x ^= x >> kShift;                          // finalize
  //
  // To decode, apply the same steps with the modular inverses of m1, m2
  // and m3 in reverse order.
  //
  //   x ^= x >> kShift;  x = (x * m3_inv) & kMask;   // round 1
  //   x ^= x >> kShift;  x = (x * m2_inv) & kMask;   // round 2
  //   x ^= x >> kShift;  x = (x * m1_inv) & kMask;   // round 3
  //   x ^= x >> kShift;                              // finalize
  //
  // where kShift = kArrayIndexValueBits / 2, kMask = kArrayIndexValueMask,
  // m1, m2, m3 (all odd) are derived from the Isolate's rapidhash secrets.
  // m1_inv, m2_inv, m3_inv (modular inverses) are precomputed so that
  // UnseedArrayIndexValue can quickly recover the original value.
  static V8_INLINE uint32_t SeedArrayIndexValue(uint32_t value,
                                                const HashSeed seed);
  // Decode array index value from seeded raw hash field.
  static V8_INLINE uint32_t UnseedArrayIndexValue(uint32_t value,
                                                  const HashSeed seed);
#endif  // V8_ENABLE_SEEDED_ARRAY_INDEX_HASH
};

// Useful for std containers that require something ()'able.
struct SeededStringHasher {
  explicit SeededStringHasher(const HashSeed hashseed) : hashseed_(hashseed) {}
  inline std::size_t operator()(const char* name) const;

  const HashSeed hashseed_;
};

// Useful for std containers that require something ()'able.
struct StringEquals {
  bool operator()(const char* name1, const char* name2) const {
    return strcmp(name1, name2) == 0;
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_STRINGS_STRING_HASHER_H_

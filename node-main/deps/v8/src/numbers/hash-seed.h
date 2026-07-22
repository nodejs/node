// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_NUMBERS_HASH_SEED_H_
#define V8_NUMBERS_HASH_SEED_H_

#include <stdint.h>

namespace v8 {
namespace internal {

class Isolate;
class LocalIsolate;
class ReadOnlyRoots;

class HashSeed {
 public:
  inline explicit HashSeed(Isolate* isolate);
  inline explicit HashSeed(LocalIsolate* isolate);
  inline explicit HashSeed(ReadOnlyRoots roots);
  inline explicit HashSeed(uint64_t seed, const uint64_t secret[3]);

  static inline HashSeed Default();

  uint64_t seed() const { return seed_; }
  const uint64_t* secret() const { return secret_; }

  constexpr bool operator==(const HashSeed& b) const {
    return seed_ == b.seed_ && secret_[0] == b.secret_[0] &&
           secret_[1] == b.secret_[1] && secret_[2] == b.secret_[2];
  }

 private:
  uint64_t seed_;
  uint64_t secret_[3];
};

}  // namespace internal
}  // namespace v8

#endif  // V8_NUMBERS_HASH_SEED_H_

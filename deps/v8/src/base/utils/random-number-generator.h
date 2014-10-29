// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_UTILS_RANDOM_NUMBER_GENERATOR_H_
#define V8_BASE_UTILS_RANDOM_NUMBER_GENERATOR_H_

#include "src/base/macros.h"

namespace v8 {
namespace base {

// -----------------------------------------------------------------------------
// RandomNumberGenerator
//
// This class is used to generate a stream of pseudorandom numbers. The class
// uses a 48-bit seed, which is modified using a linear congruential formula.
// (See Donald Knuth, The Art of Computer Programming, Volume 3, Section 3.2.1.)
// If two instances of RandomNumberGenerator are created with the same seed, and
// the same sequence of method calls is made for each, they will generate and
// return identical sequences of numbers.
// This class uses (probably) weak entropy by default, but it's sufficient,
// because it is the responsibility of the embedder to install an entropy source
// using v8::V8::SetEntropySource(), which provides reasonable entropy, see:
// https://code.google.com/p/v8/issues/detail?id=2905
// This class is neither reentrant nor threadsafe.

class RandomNumberGenerator FINAL {
 public:
  // EntropySource is used as a callback function when V8 needs a source of
  // entropy.
  typedef bool (*EntropySource)(unsigned char* buffer, size_t buflen);
  static void SetEntropySource(EntropySource entropy_source);

  RandomNumberGenerator();
  explicit RandomNumberGenerator(int64_t seed) { SetSeed(seed); }

  // Returns the next pseudorandom, uniformly distributed int value from this
  // random number generator's sequence. The general contract of |NextInt()| is
  // that one int value is pseudorandomly generated and returned.
  // All 2^32 possible integer values are produced with (approximately) equal
  // probability.
  V8_INLINE int NextInt() WARN_UNUSED_RESULT {
    return Next(32);
  }

  // Returns a pseudorandom, uniformly distributed int value between 0
  // (inclusive) and the specified max value (exclusive), drawn from this random
  // number generator's sequence. The general contract of |NextInt(int)| is that
  // one int value in the specified range is pseudorandomly generated and
  // returned. All max possible int values are produced with (approximately)
  // equal probability.
  int NextInt(int max) WARN_UNUSED_RESULT;

  // Returns the next pseudorandom, uniformly distributed boolean value from
  // this random number generator's sequence. The general contract of
  // |NextBoolean()| is that one boolean value is pseudorandomly generated and
  // returned. The values true and false are produced with (approximately) equal
  // probability.
  V8_INLINE bool NextBool() WARN_UNUSED_RESULT {
    return Next(1) != 0;
  }

  // Returns the next pseudorandom, uniformly distributed double value between
  // 0.0 and 1.0 from this random number generator's sequence.
  // The general contract of |NextDouble()| is that one double value, chosen
  // (approximately) uniformly from the range 0.0 (inclusive) to 1.0
  // (exclusive), is pseudorandomly generated and returned.
  double NextDouble() WARN_UNUSED_RESULT;

  // Fills the elements of a specified array of bytes with random numbers.
  void NextBytes(void* buffer, size_t buflen);

  // Override the current ssed.
  void SetSeed(int64_t seed);

  int64_t initial_seed() const { return initial_seed_; }

 private:
  static const int64_t kMultiplier = V8_2PART_UINT64_C(0x5, deece66d);
  static const int64_t kAddend = 0xb;
  static const int64_t kMask = V8_2PART_UINT64_C(0xffff, ffffffff);

  int Next(int bits) WARN_UNUSED_RESULT;

  int64_t initial_seed_;
  int64_t seed_;
};

} }  // namespace v8::base

#endif  // V8_BASE_UTILS_RANDOM_NUMBER_GENERATOR_H_

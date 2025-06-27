// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_UTILS_RANDOM_NUMBER_GENERATOR_H_
#define V8_BASE_UTILS_RANDOM_NUMBER_GENERATOR_H_

#include <unordered_set>
#include <vector>

#include "src/base/base-export.h"
#include "src/base/macros.h"

namespace v8 {
namespace base {

// -----------------------------------------------------------------------------
// RandomNumberGenerator

// This class is used to generate a stream of pseudo-random numbers. The class
// uses a 64-bit seed, which is passed through MurmurHash3 to create two 64-bit
// state values. This pair of state values is then used in xorshift128+.
// The resulting stream of pseudo-random numbers has a period length of 2^128-1.
// See Marsaglia: http://www.jstatsoft.org/v08/i14/paper
// And Vigna: http://vigna.di.unimi.it/ftp/papers/xorshiftplus.pdf
// NOTE: Any changes to the algorithm must be tested against TestU01.
//       Please find instructions for this in the internal repository.

// If two instances of RandomNumberGenerator are created with the same seed, and
// the same sequence of method calls is made for each, they will generate and
// return identical sequences of numbers.
// This class uses (probably) weak entropy by default, but it's sufficient,
// because it is the responsibility of the embedder to install an entropy source
// using v8::V8::SetEntropySource(), which provides reasonable entropy, see:
// https://code.google.com/p/v8/issues/detail?id=2905
// This class is neither reentrant nor threadsafe.

class V8_BASE_EXPORT RandomNumberGenerator final {
 public:
  // EntropySource is used as a callback function when V8 needs a source of
  // entropy.
  using EntropySource = bool (*)(unsigned char* buffer, size_t buflen);
  static void SetEntropySource(EntropySource entropy_source);

  RandomNumberGenerator();
  explicit RandomNumberGenerator(int64_t seed) { SetSeed(seed); }

  // Returns the next pseudorandom, uniformly distributed int value from this
  // random number generator's sequence. The general contract of |NextInt()| is
  // that one int value is pseudorandomly generated and returned.
  // All 2^32 possible integer values are produced with (approximately) equal
  // probability.
  V8_INLINE int NextInt() V8_WARN_UNUSED_RESULT { return Next(32); }

  // Returns a pseudorandom, uniformly distributed int value between 0
  // (inclusive) and the specified max value (exclusive), drawn from this random
  // number generator's sequence. The general contract of |NextInt(int)| is that
  // one int value in the specified range is pseudorandomly generated and
  // returned. All max possible int values are produced with (approximately)
  // equal probability.
  int NextInt(int max) V8_WARN_UNUSED_RESULT;

  // Returns the next pseudorandom, uniformly distributed boolean value from
  // this random number generator's sequence. The general contract of
  // |NextBoolean()| is that one boolean value is pseudorandomly generated and
  // returned. The values true and false are produced with (approximately) equal
  // probability.
  V8_INLINE bool NextBool() V8_WARN_UNUSED_RESULT { return Next(1) != 0; }

  // Returns the next pseudorandom, uniformly distributed double value between
  // 0.0 and 1.0 from this random number generator's sequence.
  // The general contract of |NextDouble()| is that one double value, chosen
  // (approximately) uniformly from the range 0.0 (inclusive) to 1.0
  // (exclusive), is pseudorandomly generated and returned.
  double NextDouble() V8_WARN_UNUSED_RESULT;

  // Returns the next pseudorandom, uniformly distributed int64 value from this
  // random number generator's sequence. The general contract of |NextInt64()|
  // is that one 64-bit int value is pseudorandomly generated and returned.
  // All 2^64 possible integer values are produced with (approximately) equal
  // probability.
  int64_t NextInt64() V8_WARN_UNUSED_RESULT;

  // Fills the elements of a specified array of bytes with random numbers.
  void NextBytes(void* buffer, size_t buflen);

  // Returns the next pseudorandom set of n unique uint64 values smaller than
  // max.
  // n must be less or equal to max.
  std::vector<uint64_t> NextSample(uint64_t max,
                                   size_t n) V8_WARN_UNUSED_RESULT;

  // Returns the next pseudorandom set of n unique uint64 values smaller than
  // max.
  // n must be less or equal to max.
  // max - |excluded| must be less or equal to n.
  //
  // Generates list of all possible values and removes random values from it
  // until size reaches n.
  std::vector<uint64_t> NextSampleSlow(
      uint64_t max, size_t n,
      const std::unordered_set<uint64_t>& excluded =
          std::unordered_set<uint64_t>{}) V8_WARN_UNUSED_RESULT;

  // Override the current ssed.
  void SetSeed(int64_t seed);

  int64_t initial_seed() const { return initial_seed_; }

  // Static and exposed for external use.
  static inline double ToDouble(uint64_t state0) {
    // Get a random [0,2**53) integer value (up to MAX_SAFE_INTEGER) by dropping
    // 11 bits of the state.
    double random_0_to_2_53 = static_cast<double>(state0 >> 11);
    // Map this to [0,1) by division with 2**53.
    constexpr double k2_53{static_cast<uint64_t>(1) << 53};
    return random_0_to_2_53 / k2_53;
  }

  // Static and exposed for external use.
  static inline void XorShift128(uint64_t* state0, uint64_t* state1) {
    uint64_t s1 = *state0;
    uint64_t s0 = *state1;
    *state0 = s0;
    s1 ^= s1 << 23;
    s1 ^= s1 >> 17;
    s1 ^= s0;
    s1 ^= s0 >> 26;
    *state1 = s1;
  }

  static uint64_t MurmurHash3(uint64_t);

  // Implement the UniformRandomBitGenerator interface.
  using result_type = unsigned;
  result_type operator()() { return NextInt(); }
  static constexpr result_type min() { return 0; }
  static constexpr result_type max() {
    return std::numeric_limits<result_type>::max();
  }

 private:
  static const int64_t kMultiplier = 0x5'deec'e66d;
  static const int64_t kAddend = 0xb;
  static const int64_t kMask = 0xffff'ffff'ffff;

  int Next(int bits) V8_WARN_UNUSED_RESULT;

  int64_t initial_seed_;
  uint64_t state0_;
  uint64_t state1_;
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_UTILS_RANDOM_NUMBER_GENERATOR_H_

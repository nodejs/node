// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "utils/random-number-generator.h"

#include <stdio.h>
#include <stdlib.h>

#include "flags.h"
#include "platform/mutex.h"
#include "platform/time.h"
#include "utils.h"

namespace v8 {
namespace internal {

static LazyMutex entropy_mutex = LAZY_MUTEX_INITIALIZER;
static RandomNumberGenerator::EntropySource entropy_source = NULL;


// static
void RandomNumberGenerator::SetEntropySource(EntropySource source) {
  LockGuard<Mutex> lock_guard(entropy_mutex.Pointer());
  entropy_source = source;
}


RandomNumberGenerator::RandomNumberGenerator() {
  // Check --random-seed flag first.
  if (FLAG_random_seed != 0) {
    SetSeed(FLAG_random_seed);
    return;
  }

  // Check if embedder supplied an entropy source.
  { LockGuard<Mutex> lock_guard(entropy_mutex.Pointer());
    if (entropy_source != NULL) {
      int64_t seed;
      if (entropy_source(reinterpret_cast<unsigned char*>(&seed),
                         sizeof(seed))) {
        SetSeed(seed);
        return;
      }
    }
  }

#if V8_OS_CYGWIN || V8_OS_WIN
  // Use rand_s() to gather entropy on Windows. See:
  // https://code.google.com/p/v8/issues/detail?id=2905
  unsigned first_half, second_half;
  errno_t result = rand_s(&first_half);
  ASSERT_EQ(0, result);
  result = rand_s(&second_half);
  ASSERT_EQ(0, result);
  SetSeed((static_cast<int64_t>(first_half) << 32) + second_half);
#else
  // Gather entropy from /dev/urandom if available.
  FILE* fp = fopen("/dev/urandom", "rb");
  if (fp != NULL) {
    int64_t seed;
    size_t n = fread(&seed, sizeof(seed), 1, fp);
    fclose(fp);
    if (n == 1) {
      SetSeed(seed);
      return;
    }
  }

  // We cannot assume that random() or rand() were seeded
  // properly, so instead of relying on random() or rand(),
  // we just seed our PRNG using timing data as fallback.
  // This is weak entropy, but it's sufficient, because
  // it is the responsibility of the embedder to install
  // an entropy source using v8::V8::SetEntropySource(),
  // which provides reasonable entropy, see:
  // https://code.google.com/p/v8/issues/detail?id=2905
  int64_t seed = Time::NowFromSystemTime().ToInternalValue() << 24;
  seed ^= TimeTicks::HighResolutionNow().ToInternalValue() << 16;
  seed ^= TimeTicks::Now().ToInternalValue() << 8;
  SetSeed(seed);
#endif  // V8_OS_CYGWIN || V8_OS_WIN
}


int RandomNumberGenerator::NextInt(int max) {
  ASSERT_LE(0, max);

  // Fast path if max is a power of 2.
  if (IsPowerOf2(max)) {
    return static_cast<int>((max * static_cast<int64_t>(Next(31))) >> 31);
  }

  while (true) {
    int rnd = Next(31);
    int val = rnd % max;
    if (rnd - val + (max - 1) >= 0) {
      return val;
    }
  }
}


double RandomNumberGenerator::NextDouble() {
  return ((static_cast<int64_t>(Next(26)) << 27) + Next(27)) /
      static_cast<double>(static_cast<int64_t>(1) << 53);
}


void RandomNumberGenerator::NextBytes(void* buffer, size_t buflen) {
  for (size_t n = 0; n < buflen; ++n) {
    static_cast<uint8_t*>(buffer)[n] = static_cast<uint8_t>(Next(8));
  }
}


int RandomNumberGenerator::Next(int bits) {
  ASSERT_LT(0, bits);
  ASSERT_GE(32, bits);
  int64_t seed = (seed_ * kMultiplier + kAddend) & kMask;
  seed_ = seed;
  return static_cast<int>(seed >> (48 - bits));
}


void RandomNumberGenerator::SetSeed(int64_t seed) {
  seed_ = (seed ^ kMultiplier) & kMask;
}

} }  // namespace v8::internal

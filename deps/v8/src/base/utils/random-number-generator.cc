// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/utils/random-number-generator.h"

#include <stdio.h>
#include <stdlib.h>

#include <new>

#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/time.h"

namespace v8 {
namespace base {

static LazyMutex entropy_mutex = LAZY_MUTEX_INITIALIZER;
static RandomNumberGenerator::EntropySource entropy_source = NULL;


// static
void RandomNumberGenerator::SetEntropySource(EntropySource source) {
  LockGuard<Mutex> lock_guard(entropy_mutex.Pointer());
  entropy_source = source;
}


RandomNumberGenerator::RandomNumberGenerator() {
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
  DCHECK_EQ(0, result);
  result = rand_s(&second_half);
  DCHECK_EQ(0, result);
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
  DCHECK_LT(0, max);

  // Fast path if max is a power of 2.
  if (IS_POWER_OF_TWO(max)) {
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
  XorShift128(&state0_, &state1_);
  return ToDouble(state0_, state1_);
}


int64_t RandomNumberGenerator::NextInt64() {
  XorShift128(&state0_, &state1_);
  return bit_cast<int64_t>(state0_ + state1_);
}


void RandomNumberGenerator::NextBytes(void* buffer, size_t buflen) {
  for (size_t n = 0; n < buflen; ++n) {
    static_cast<uint8_t*>(buffer)[n] = static_cast<uint8_t>(Next(8));
  }
}


int RandomNumberGenerator::Next(int bits) {
  DCHECK_LT(0, bits);
  DCHECK_GE(32, bits);
  XorShift128(&state0_, &state1_);
  return static_cast<int>((state0_ + state1_) >> (64 - bits));
}


void RandomNumberGenerator::SetSeed(int64_t seed) {
  initial_seed_ = seed;
  state0_ = MurmurHash3(bit_cast<uint64_t>(seed));
  state1_ = MurmurHash3(~state0_);
  CHECK(state0_ != 0 || state1_ != 0);
}


uint64_t RandomNumberGenerator::MurmurHash3(uint64_t h) {
  h ^= h >> 33;
  h *= V8_UINT64_C(0xFF51AFD7ED558CCD);
  h ^= h >> 33;
  h *= V8_UINT64_C(0xC4CEB9FE1A85EC53);
  h ^= h >> 33;
  return h;
}

}  // namespace base
}  // namespace v8

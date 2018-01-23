// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/utils/random-number-generator.h"

#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <new>

#include "src/base/bits.h"
#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/time.h"

namespace v8 {
namespace base {

static LazyMutex entropy_mutex = LAZY_MUTEX_INITIALIZER;
static RandomNumberGenerator::EntropySource entropy_source = nullptr;

// static
void RandomNumberGenerator::SetEntropySource(EntropySource source) {
  LockGuard<Mutex> lock_guard(entropy_mutex.Pointer());
  entropy_source = source;
}


RandomNumberGenerator::RandomNumberGenerator() {
  // Check if embedder supplied an entropy source.
  { LockGuard<Mutex> lock_guard(entropy_mutex.Pointer());
    if (entropy_source != nullptr) {
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
  if (fp != nullptr) {
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
  if (bits::IsPowerOfTwo(max)) {
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

static std::vector<uint64_t> ComplementSample(
    const std::unordered_set<uint64_t>& set, uint64_t max) {
  std::vector<uint64_t> result;
  result.reserve(max - set.size());
  for (uint64_t i = 0; i < max; i++) {
    if (!set.count(i)) {
      result.push_back(i);
    }
  }
  return result;
}

std::vector<uint64_t> RandomNumberGenerator::NextSample(uint64_t max,
                                                        size_t n) {
  CHECK_LE(n, max);

  if (n == 0) {
    return std::vector<uint64_t>();
  }

  // Choose to select or exclude, whatever needs fewer generator calls.
  size_t smaller_part = static_cast<size_t>(
      std::min(max - static_cast<uint64_t>(n), static_cast<uint64_t>(n)));
  std::unordered_set<uint64_t> selected;

  size_t counter = 0;
  while (selected.size() != smaller_part && counter / 3 < smaller_part) {
    uint64_t x = static_cast<uint64_t>(NextDouble() * max);
    CHECK_LT(x, max);

    selected.insert(x);
    counter++;
  }

  if (selected.size() == smaller_part) {
    if (smaller_part != n) {
      return ComplementSample(selected, max);
    }
    return std::vector<uint64_t>(selected.begin(), selected.end());
  }

  // Failed to select numbers in smaller_part * 3 steps, try different approach.
  return NextSampleSlow(max, n, selected);
}

std::vector<uint64_t> RandomNumberGenerator::NextSampleSlow(
    uint64_t max, size_t n, const std::unordered_set<uint64_t>& excluded) {
  CHECK_GE(max - excluded.size(), n);

  std::vector<uint64_t> result;
  result.reserve(max - excluded.size());

  for (uint64_t i = 0; i < max; i++) {
    if (!excluded.count(i)) {
      result.push_back(i);
    }
  }

  // Decrease result vector until it contains values to select or exclude,
  // whatever needs fewer generator calls.
  size_t larger_part = static_cast<size_t>(
      std::max(max - static_cast<uint64_t>(n), static_cast<uint64_t>(n)));

  // Excluded set may cause that initial result is already smaller than
  // larget_part.
  while (result.size() != larger_part && result.size() > n) {
    size_t x = static_cast<size_t>(NextDouble() * result.size());
    CHECK_LT(x, result.size());

    std::swap(result[x], result.back());
    result.pop_back();
  }

  if (result.size() != n) {
    return ComplementSample(
        std::unordered_set<uint64_t>(result.begin(), result.end()), max);
  }
  return result;
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
  h *= uint64_t{0xFF51AFD7ED558CCD};
  h ^= h >> 33;
  h *= uint64_t{0xC4CEB9FE1A85EC53};
  h ^= h >> 33;
  return h;
}

}  // namespace base
}  // namespace v8

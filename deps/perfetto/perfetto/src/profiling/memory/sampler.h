/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_PROFILING_MEMORY_SAMPLER_H_
#define SRC_PROFILING_MEMORY_SAMPLER_H_

#include <stdint.h>

#include <atomic>
#include <random>

#include "perfetto/ext/base/utils.h"

namespace perfetto {
namespace profiling {

constexpr uint64_t kSamplerSeed = 1;

// Poisson sampler for memory allocations. We apply sampling individually to
// each byte. The whole allocation gets accounted as often as the number of
// sampled bytes it contains.
//
// The algorithm is inspired by the Chromium sampling algorithm at
// https://cs.chromium.org/search/?q=f:cc+symbol:AllocatorShimLogAlloc+package:%5Echromium$&type=cs
// Googlers: see go/chrome-shp for more details.
//
// NB: not thread-safe, requires external synchronization.
class Sampler {
 public:
  Sampler(uint64_t sampling_interval)
      : sampling_interval_(sampling_interval),
        sampling_rate_(1.0 / static_cast<double>(sampling_interval)),
        random_engine_(kSamplerSeed),
        interval_to_next_sample_(NextSampleInterval()) {}

  // Returns number of bytes that should be be attributed to the sample.
  // If returned size is 0, the allocation should not be sampled.
  //
  // Due to how the poission sampling works, some samples should be accounted
  // multiple times.
  size_t SampleSize(size_t alloc_sz) {
    if (PERFETTO_UNLIKELY(alloc_sz >= sampling_interval_))
      return alloc_sz;
    return static_cast<size_t>(sampling_interval_ * NumberOfSamples(alloc_sz));
  }

 private:
  int64_t NextSampleInterval() {
    std::exponential_distribution<double> dist(sampling_rate_);
    int64_t next = static_cast<int64_t>(dist(random_engine_));
    // The +1 corrects the distribution of the first value in the interval.
    // TODO(fmayer): Figure out why.
    return next + 1;
  }

  // Returns number of times a sample should be accounted. Due to how the
  // poission sampling works, some samples should be accounted multiple times.
  size_t NumberOfSamples(size_t alloc_sz) {
    interval_to_next_sample_ -= alloc_sz;
    size_t num_samples = 0;
    while (PERFETTO_UNLIKELY(interval_to_next_sample_ <= 0)) {
      interval_to_next_sample_ += NextSampleInterval();
      ++num_samples;
    }
    return num_samples;
  }

  uint64_t sampling_interval_;
  double sampling_rate_;
  std::default_random_engine random_engine_;
  int64_t interval_to_next_sample_;
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_MEMORY_SAMPLER_H_

// Copyright 2025 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HIGHWAY_HWY_AUTO_TUNE_H_
#define HIGHWAY_HWY_AUTO_TUNE_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>  // memmove

#include <cmath>
#include <vector>

#include "hwy/aligned_allocator.h"  // Span
#include "hwy/base.h"               // HWY_MIN

// configuration to allow auto_tune to use std::sort instead of VQSort
// (also enabled in header only mode).
#if defined(HWY_HEADER_ONLY)
#define HWY_AUTOTUNE_STDSORT
#endif

#ifdef HWY_AUTOTUNE_STDSORT
#include <algorithm>  // std::sort
#else
#include "hwy/contrib/sort/vqsort.h"  // VQSort
#endif

// Infrastructure for auto-tuning (choosing optimal parameters at runtime).

namespace hwy {

// O(1) storage to estimate the central tendency of hundreds of independent
// distributions (one per configuration). The number of samples per distribution
// (`kMinSamples`) varies from few to dozens. We support both by first storing
// values in a buffer, and when full, switching to online variance estimation.
// Modified from `hwy/stats.h`.
class CostDistribution {
 public:
  static constexpr size_t kMaxValues = 14;  // for total size of 128 bytes

  void Notify(const double x) {
    if (HWY_UNLIKELY(x < 0.0)) {
      HWY_WARN("Ignoring negative cost %f.", x);
      return;
    }

    // Online phase after filling and warm-up.
    if (HWY_LIKELY(IsOnline())) return OnlineNotify(x);

    // Fill phase: store up to `kMaxValues` values.
    values_[num_values_++] = x;
    HWY_DASSERT(num_values_ <= kMaxValues);
    if (HWY_UNLIKELY(num_values_ == kMaxValues)) {
      WarmUpOnline();
      HWY_DASSERT(IsOnline());
    }
  }

  // Returns an estimate of the true cost, mitigating the impact of noise.
  //
  // Background and observations from time measurements in `thread_pool.h`:
  // - We aim for O(1) storage because there may be hundreds of instances.
  // - The mean is biased upwards by mostly additive noise: particularly
  //   interruptions such as context switches, but also contention.
  // - The minimum is not a robust estimator because there are also "lucky
  //   shots" (1.2-1.6x lower values) where interruptions or contention happen
  //   to be low.
  // - We want to preserve information about contention and a configuration's
  //   sensitivity to it. Otherwise, we are optimizing for the best-case, not
  //   the common case.
  // - It is still important to minimize the influence of outliers, such as page
  //   faults, which can cause multiple times larger measurements.
  // - Detecting outliers based only on the initial variance is too brittle. If
  //   the sample is narrow, measurements will fluctuate across runs because
  //   too many measurements are considered outliers. This would cause the
  //   'best' configuration to vary.
  //
  // Approach:
  // - Use Winsorization to reduce the impact of outliers, while preserving
  //   information on the central tendency.
  // - Continually update the thresholds based on the online variance, with
  //   exponential smoothing for stability.
  // - Trim the initial sample via MAD or skewness for a robust estimate of the
  //   variance.
  double EstimateCost() {
    if (!IsOnline()) {
      WarmUpOnline();
      HWY_DASSERT(IsOnline());
    }
    return Mean();
  }

  // Multiplex online state into values_ to allow higher `kMaxValues`.
  // Public for inspection in tests. Do not use directly.
  double& M1() { return values_[0]; }  // Moments for variance.
  double& M2() { return values_[1]; }
  double& Mean() { return values_[2]; }  // Exponential smoothing.
  double& Stddev() { return values_[3]; }
  double& Lower() { return values_[4]; }
  double& Upper() { return values_[5]; }

 private:
  static double Median(double* to_sort, size_t n) {
    HWY_DASSERT(n >= 2);

#ifdef HWY_AUTOTUNE_STDSORT
    std::sort(to_sort, to_sort + n);
#else
// F64 is supported everywhere except Armv7.
#if !HWY_ARCH_ARM_V7
    VQSort(to_sort, n, SortAscending());
#else
    // Values are known to be finite and non-negative, hence sorting as U64 is
    // equivalent.
    VQSort(reinterpret_cast<uint64_t*>(to_sort), n, SortAscending());
#endif
#endif

    if (n & 1) return to_sort[n / 2];
    // Even length: average of two middle elements.
    return (to_sort[n / 2] + to_sort[n / 2 - 1]) * 0.5;
  }

  static double MAD(const double* values, size_t n, const double median) {
    double abs_dev[kMaxValues];
    for (size_t i = 0; i < n; ++i) {
      abs_dev[i] = ScalarAbs(values[i] - median);
    }
    return Median(abs_dev, n);
  }

  // If `num_values_` is large enough, sorts and discards outliers: either via
  // MAD, or if too many values are equal, by trimming according to skewness.
  void RemoveOutliers() {
    if (num_values_ < 3) return;  // Not enough to discard two.
    HWY_DASSERT(num_values_ <= kMaxValues);

    // Given the noise level in `auto_tune_test`, it can happen that 1/4 of the
    // sample is an outlier *in either direction*. Use median absolute
    // deviation, which is robust to almost half of the sample being outliers.
    const double median = Median(values_, num_values_);  // sorts in-place.
    const double mad = MAD(values_, num_values_, median);
    // At least half the sample is equal.
    if (mad == 0.0) {
      // Estimate skewness to decide which side to trim more.
      const double skewness =
          (values_[num_values_ - 1] - median) - (median - values_[0]);

      const size_t trim = HWY_MAX(num_values_ / 2, size_t{2});
      const size_t left =
          HWY_MAX(skewness < 0.0 ? trim * 3 / 4 : trim / 4, size_t{1});
      num_values_ -= trim;
      HWY_DASSERT(num_values_ >= 1);
      memmove(values_, values_ + left, num_values_ * sizeof(values_[0]));
      return;
    }

    const double upper = median + 5.0 * mad;
    const double lower = median - 5.0 * mad;
    size_t right = num_values_ - 1;
    while (values_[right] > upper) --right;
    // Nonzero MAD implies no more than half are equal, so we did not advance
    // beyond the median.
    HWY_DASSERT(right >= num_values_ / 2);

    size_t left = 0;
    while (left < right && values_[left] < lower) ++left;
    HWY_DASSERT(left <= num_values_ / 2);
    num_values_ = right - left + 1;
    memmove(values_, values_ + left, num_values_ * sizeof(values_[0]));
  }

  double SampleMean() const {
    // Only called in non-online phase, but buffer might not be full.
    HWY_DASSERT(!IsOnline() && 0 != num_values_ && num_values_ <= kMaxValues);
    double sum = 0.0;
    for (size_t i = 0; i < num_values_; ++i) {
      sum += values_[i];
    }
    return sum / static_cast<double>(num_values_);
  }

  // Unbiased estimator for population variance even for small `num_values_`.
  double SampleVariance(double sample_mean) const {
    HWY_DASSERT(sample_mean >= 0.0);  // we checked costs are non-negative.
    // Only called in non-online phase, but buffer might not be full.
    HWY_DASSERT(!IsOnline() && 0 != num_values_ && num_values_ <= kMaxValues);
    if (HWY_UNLIKELY(num_values_ == 1)) return 0.0;  // prevent divide-by-zero.
    double sum2 = 0.0;
    for (size_t i = 0; i < num_values_; ++i) {
      const double d = values_[i] - sample_mean;
      sum2 += d * d;
    }
    return sum2 / static_cast<double>(num_values_ - 1);
  }

  bool IsOnline() const { return online_n_ > 0.0; }

  void OnlineNotify(double x) {
    // Winsorize.
    x = HWY_MIN(HWY_MAX(Lower(), x), Upper());

    // Welford's online variance estimator.
    // https://media.thinkbrg.com/wp-content/uploads/2020/06/19094655/720_720_McCrary_ImplementingAlgorithms_Whitepaper_20151119_WEB.pdf#page=7.09
    const double n_minus_1 = online_n_;
    online_n_ += 1.0;
    const double d = x - M1();
    const double d_div_n = d / online_n_;
    M1() += d_div_n;
    HWY_DASSERT(M1() >= Lower());
    M2() += d * n_minus_1 * d_div_n;  // d^2 * (N-1)/N
    // HWY_MAX avoids divide-by-zero.
    const double stddev = std::sqrt(M2() / HWY_MAX(1.0, n_minus_1));

    // Exponential smoothing.
    constexpr double kNew = 0.2;  // relatively fast update
    constexpr double kOld = 1.0 - kNew;
    Mean() = M1() * kNew + Mean() * kOld;
    Stddev() = stddev * kNew + Stddev() * kOld;

    // Update thresholds from smoothed mean and stddev to enable recovering from
    // a too narrow initial range due to excessive trimming.
    Lower() = Mean() - 3.5 * Stddev();
    Upper() = Mean() + 3.5 * Stddev();
  }

  void WarmUpOnline() {
    RemoveOutliers();

    // Compute and copy before writing to `M1`, which overwrites `values_`!
    const double sample_mean = SampleMean();
    const double sample_variance = SampleVariance(sample_mean);
    double copy[kMaxValues];
    hwy::CopyBytes(values_, copy, num_values_ * sizeof(values_[0]));

    M1() = M2() = 0.0;
    Mean() = sample_mean;
    Stddev() = std::sqrt(sample_variance);
    // For single-value or all-equal sample, widen the range, else we will only
    // accept the same value.
    if (Stddev() == 0.0) Stddev() = Mean() / 2;

    // High tolerance because the distribution is not actually Gaussian, and
    // we trimmed up to *half*, and do not want to reject too many values in
    // the online phase.
    Lower() = Mean() - 4.0 * Stddev();
    Upper() = Mean() + 4.0 * Stddev();
    // Feed copied values into online estimator.
    for (size_t i = 0; i < num_values_; ++i) {
      OnlineNotify(copy[i]);
    }
    HWY_DASSERT(IsOnline());
  }

  size_t num_values_ = 0;  // size of `values_` <= `kMaxValues`
#if SIZE_MAX == 0xFFFFFFFFu
  HWY_MAYBE_UNUSED uint32_t padding_ = 0;
#endif

  double online_n_ = 0.0;  // number of calls to `OnlineNotify`.

  double values_[kMaxValues];
};
static_assert(sizeof(CostDistribution) == 128, "");

// Implements a counter with wrap-around, plus the ability to skip values.
// O(1) time, O(N) space via doubly-linked list of indices.
class NextWithSkip {
 public:
  NextWithSkip() {}
  explicit NextWithSkip(size_t num) {
    links_.reserve(num);
    for (size_t i = 0; i < num; ++i) {
      links_.emplace_back(i, num);
    }
  }

  size_t Next(size_t pos) {
    HWY_DASSERT(pos < links_.size());
    HWY_DASSERT(!links_[pos].IsRemoved());
    return links_[pos].Next();
  }

  // Must not be called for an already skipped position. Ignores an attempt to
  // skip the last remaining position.
  void Skip(size_t pos) {
    HWY_DASSERT(!links_[pos].IsRemoved());  // not already skipped.
    const size_t prev = links_[pos].Prev();
    const size_t next = links_[pos].Next();
    if (prev == pos || next == pos) return;  // last remaining position.
    links_[next].SetPrev(prev);
    links_[prev].SetNext(next);
    links_[pos].Remove();
  }

 private:
  // Combine prev/next into one array to improve locality/reduce allocations.
  class Link {
    // Bit-shifts avoid potentially expensive 16-bit loads. Store `next` at the
    // top and `prev` at the bottom for extraction with a single shift/AND.
    // There may be hundreds of configurations, so 8 bits are not enough.
    static constexpr size_t kBits = 14;
    static constexpr size_t kShift = 32 - kBits;
    static constexpr uint32_t kMaxNum = 1u << kBits;

   public:
    Link(size_t pos, size_t num) {
      HWY_DASSERT(num < kMaxNum);
      const size_t prev = pos == 0 ? num - 1 : pos - 1;
      const size_t next = pos == num - 1 ? 0 : pos + 1;
      bits_ =
          (static_cast<uint32_t>(next) << kShift) | static_cast<uint32_t>(prev);
      HWY_DASSERT(Next() == next && Prev() == prev);
      HWY_DASSERT(!IsRemoved());
    }

    bool IsRemoved() const { return (bits_ & kMaxNum) != 0; }
    void Remove() { bits_ |= kMaxNum; }

    size_t Next() const { return bits_ >> kShift; }
    size_t Prev() const { return bits_ & (kMaxNum - 1); }

    void SetNext(size_t next) {
      HWY_DASSERT(next < kMaxNum);
      bits_ &= (~0u >> kBits);  // clear old next
      bits_ |= static_cast<uint32_t>(next) << kShift;
      HWY_DASSERT(Next() == next);
      HWY_DASSERT(!IsRemoved());
    }
    void SetPrev(size_t prev) {
      HWY_DASSERT(prev < kMaxNum);
      bits_ &= ~(kMaxNum - 1);  // clear old prev
      bits_ |= static_cast<uint32_t>(prev);
      HWY_DASSERT(Prev() == prev);
      HWY_DASSERT(!IsRemoved());
    }

   private:
    uint32_t bits_;
  };
  std::vector<Link> links_;
};

// State machine for choosing at runtime the lowest-cost `Config`, which is
// typically a struct containing multiple parameters. For an introduction, see
// "Auto-Tuning and Performance Portability on Heterogeneous Hardware".
//
// **Which parameters**
// Note that simple parameters such as the L2 cache size can be directly queried
// via `hwy/contrib/thread_pool/topology.h`. Difficult to predict parameters
// such as task granularity are more appropriate for auto-tuning. We also
// suggest that at least some parameters should also be 'algorithm variants'
// such as parallel vs. serial, or 2D tiling vs. 1D striping.
//
// **Search strategy**
// To guarantee the optimal result, we use exhaustive search, which is suitable
// for around 10 parameters and a few hundred combinations of 'candidate'
// configurations.
//
// **How to generate candidates**
// To keep this framework simple and generic, applications enumerate the search
// space and pass the list of all feasible candidates to `SetCandidates` before
// the first call to `NextConfig`. Applications should prune the space as much
// as possible, e.g. by upper-bounding parameters based on the known cache
// sizes, and applying constraints such as one being a multiple of another.
//
// **Usage**
// Applications typically conditionally branch to the code implementing the
// configuration returned by `NextConfig`. They measure the cost of running it
// and pass that to `NotifyCost`. Branching avoids the complexity and
// opaqueness of a JIT. The number of branches can be reduced (at the cost of
// code size) by inlining low-level decisions into larger code regions, e.g. by
// hoisting them outside hot loops.
//
// **What is cost**
// Cost is an arbitrary `uint64_t`, with lower values being better. Most
// applications will use the elapsed time. If the tasks being tuned are short,
// it is important to use a high-resolution timer such as `hwy/timer.h`. Energy
// may also be useful [https://www.osti.gov/servlets/purl/1361296].
//
// **Online vs. offline**
// Although applications can auto-tune once, offline, it may be difficult to
// ensure the stored configuration still applies to the current circumstances.
// Thus we recommend online auto-tuning, re-discovering the configuration on
// each run. We assume the overhead of bookkeeping and measuring cost is
// negligible relative to the actual work. The cost of auto-tuning is then that
// of running sub-optimal configurations. Assuming the best configuration is
// better than baseline, and the work is performed many thousands of times, the
// cost is outweighed by the benefits.
//
// **kMinSamples**
// To further reduce overhead, after `kMinSamples` rounds (= measurements of
// each configuration) we start excluding configurations from further
// measurements if they are sufficiently worse than the current best.
// `kMinSamples` can be several dozen when the tasks being tuned take a few
// microseconds. Even for longer tasks, it should be at least 2 for some noise
// tolerance. After this, there are another `kMinSamples / 2 + 1` rounds before
// declaring the winner.
template <typename Config, size_t kMinSamples = 2>
class AutoTune {
 public:
  // Returns non-null best configuration if auto-tuning has already finished.
  // Otherwise, callers continue calling `NextConfig` and `NotifyCost`.
  // Points into `Candidates()`.
  const Config* Best() const { return best_; }

  // If false, caller must call `SetCandidates` before `NextConfig`.
  // NOTE: also called after Best() is non-null.
  bool HasCandidates() const { return !candidates_.empty(); }

  // WARNING: invalidates `Best()`, do not call if that is non-null.
  void SetCandidates(std::vector<Config> candidates) {
    HWY_DASSERT(!Best() && !HasCandidates());
    candidates_.swap(candidates);
    HWY_DASSERT(HasCandidates());
    costs_.resize(candidates_.size());
    list_ = NextWithSkip(candidates_.size());
  }

  // Typically called after Best() is non-null to compare all candidates' costs.
  Span<const Config> Candidates() const {
    HWY_DASSERT(HasCandidates());
    return Span<const Config>(candidates_.data(), candidates_.size());
  }
  Span<CostDistribution> Costs() {
    return Span<CostDistribution>(costs_.data(), costs_.size());
  }

  // Returns the current `Config` to measure.
  const Config& NextConfig() const {
    HWY_DASSERT(!Best() && HasCandidates());
    return candidates_[config_idx_];
  }

  // O(1) except at the end of each round, which is O(N).
  void NotifyCost(uint64_t cost) {
    HWY_DASSERT(!Best() && HasCandidates());

    costs_[config_idx_].Notify(static_cast<double>(cost));
    // Save now before we update `config_idx_`.
    const size_t my_idx = config_idx_;
    // Only retrieve once we have enough samples, otherwise, we switch to
    // online variance before the buffer is populated.
    const double my_cost = rounds_complete_ >= kMinSamples
                               ? costs_[config_idx_].EstimateCost()
                               : 0.0;

    // Advance to next non-skipped config with wrap-around. This decorrelates
    // measurements by not immediately re-measuring the same config.
    config_idx_ = list_.Next(config_idx_);
    // Might still equal `my_idx` if this is the only non-skipped config.

    // Disqualify from future `NextConfig` if cost was too far beyond the
    // current best. This reduces the number of measurements, while tolerating
    // noise in the first few measurements. Must happen after advancing.
    if (my_cost > skip_if_above_) {
      list_.Skip(my_idx);
    }

    // Wrap-around indicates the round is complete.
    if (HWY_UNLIKELY(config_idx_ <= my_idx)) {
      ++rounds_complete_;

      // Enough samples for stable estimates: update the thresholds.
      if (rounds_complete_ >= kMinSamples) {
        double best_cost = HighestValue<double>();
        size_t idx_min = 0;
        for (size_t i = 0; i < candidates_.size(); ++i) {
          const double estimate = costs_[i].EstimateCost();
          if (estimate < best_cost) {
            best_cost = estimate;
            idx_min = i;
          }
        }
        skip_if_above_ = best_cost * 1.25;

        // After sufficient rounds, declare the winner.
        if (HWY_UNLIKELY(rounds_complete_ == 3 * kMinSamples / 2 + 1)) {
          best_ = &candidates_[idx_min];
          HWY_DASSERT(Best());
        }
      }
    }
  }

  // Avoid printing during the first few rounds, because those might be noisy
  // and not yet skipped.
  bool ShouldPrint() { return rounds_complete_ > kMinSamples; }

 private:
  const Config* best_ = nullptr;
  std::vector<Config> candidates_;
  std::vector<CostDistribution> costs_;  // one per candidate
  size_t config_idx_ = 0;                // [0, candidates_.size())
  NextWithSkip list_;
  size_t rounds_complete_ = 0;

  double skip_if_above_ = 0.0;
};

}  // namespace hwy

#endif  // HIGHWAY_HWY_AUTO_TUNE_H_

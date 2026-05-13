// Copyright 2023 Google LLC
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

// Modified from BSD-licensed code
// Copyright (c) the JPEG XL Project Authors. All rights reserved.
// See https://github.com/libjxl/libjxl/blob/main/LICENSE.

#ifndef HIGHWAY_HWY_CONTRIB_THREAD_POOL_THREAD_POOL_H_
#define HIGHWAY_HWY_CONTRIB_THREAD_POOL_THREAD_POOL_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>  // snprintf
#include <string.h>

#include <array>
#include <atomic>
#include <string>
#include <thread>  // NOLINT
#include <vector>

#include "hwy/aligned_allocator.h"  // HWY_ALIGNMENT
#include "hwy/auto_tune.h"
#include "hwy/base.h"
#include "hwy/cache_control.h"  // Pause
#include "hwy/contrib/thread_pool/futex.h"
#include "hwy/contrib/thread_pool/spin.h"
#include "hwy/contrib/thread_pool/topology.h"
#include "hwy/profiler.h"
#include "hwy/stats.h"
#include "hwy/timer.h"

#if HWY_OS_APPLE
#include <AvailabilityMacros.h>
#endif

#if PROFILER_ENABLED
#include <algorithm>  // std::sort

#include "hwy/bit_set.h"
#endif

namespace hwy {

// Sets the name of the current thread to the format string `format`, which must
// include %d for `thread`. Currently only implemented for pthreads (*nix and
// OSX); Windows involves throwing an exception.
static inline void SetThreadName(const char* format, int thread) {
  char buf[16] = {};  // Linux limit, including \0
  const int chars_written = snprintf(buf, sizeof(buf), format, thread);
  HWY_ASSERT(0 < chars_written &&
             chars_written <= static_cast<int>(sizeof(buf) - 1));

#if (HWY_OS_LINUX && (!defined(__ANDROID__) || __ANDROID_API__ >= 19)) || \
    HWY_OS_FREEBSD
  // Note that FreeBSD pthread_set_name_np does not return a value (#2669).
  HWY_ASSERT(0 == pthread_setname_np(pthread_self(), buf));
#elif HWY_OS_APPLE && (MAC_OS_X_VERSION_MIN_REQUIRED >= 1060)
  // Different interface: single argument, current thread only.
  HWY_ASSERT(0 == pthread_setname_np(buf));
#elif defined(__EMSCRIPTEN__)
  emscripten_set_thread_name(pthread_self(), buf);
#else
  (void)format;
  (void)thread;
#endif
}

// Whether workers should block or spin.
enum class PoolWaitMode : uint8_t { kBlock = 1, kSpin };

enum class Exit : uint32_t { kNone, kLoop, kThread };

// Upper bound on non-empty `ThreadPool` (single-worker pools do not count).
// Turin has 16 clusters. Add one for the across-cluster pool.
HWY_INLINE_VAR constexpr size_t kMaxClusters = 32 + 1;

// Use the last slot so that `PoolWorkerMapping` does not have to know the
// total number of clusters.
HWY_INLINE_VAR constexpr size_t kAllClusters = kMaxClusters - 1;

// Argument to `ThreadPool`: how to map local worker_idx to global.
class PoolWorkerMapping {
 public:
  // Backward-compatible mode: returns local worker index.
  PoolWorkerMapping() : cluster_idx_(0), max_cluster_workers_(0) {}
  PoolWorkerMapping(size_t cluster_idx, size_t max_cluster_workers)
      : cluster_idx_(cluster_idx), max_cluster_workers_(max_cluster_workers) {
    HWY_DASSERT(cluster_idx <= kAllClusters);
    // Only use this ctor for the new global worker index mode. If this were
    // zero, we would still return local indices.
    HWY_DASSERT(max_cluster_workers != 0);
  }

  size_t ClusterIdx() const { return cluster_idx_; }
  size_t MaxClusterWorkers() const { return max_cluster_workers_; }

  // Returns global_idx, or unchanged local worker_idx if default-constructed.
  size_t operator()(size_t worker_idx) const {
    if (cluster_idx_ == kAllClusters) {
      const size_t cluster_idx = worker_idx;
      HWY_DASSERT(cluster_idx < kAllClusters);
      // First index within the N-th cluster. The main thread is the first.
      return cluster_idx * max_cluster_workers_;
    }
    HWY_DASSERT(max_cluster_workers_ == 0 || worker_idx < max_cluster_workers_);
    return cluster_idx_ * max_cluster_workers_ + worker_idx;
  }

 private:
  size_t cluster_idx_;
  size_t max_cluster_workers_;
};

namespace pool {

#ifndef HWY_POOL_VERBOSITY
#define HWY_POOL_VERBOSITY 0
#endif

static constexpr int kVerbosity = HWY_POOL_VERBOSITY;

// Some CPUs already have more than this many threads, but rather than one
// large pool, we assume applications create multiple pools, ideally per
// cluster (cores sharing a cache), because this improves locality and barrier
// latency. In that case, this is a generous upper bound.
static constexpr size_t kMaxThreads = 127;

// Generates a random permutation of [0, size). O(1) storage.
class ShuffledIota {
 public:
  ShuffledIota() : coprime_(1) {}  // for Worker
  explicit ShuffledIota(uint32_t coprime) : coprime_(coprime) {}

  // Returns the next after `current`, using an LCG-like generator.
  uint32_t Next(uint32_t current, const Divisor64& divisor) const {
    HWY_DASSERT(current < divisor.GetDivisor());
    // (coprime * i + current) % size, see https://lemire.me/blog/2017/09/18/.
    return static_cast<uint32_t>(divisor.Remainder(current + coprime_));
  }

  // Returns true if a and b have no common denominator except 1. Based on
  // binary GCD. Assumes a and b are nonzero. Also used in tests.
  static bool CoprimeNonzero(uint32_t a, uint32_t b) {
    const size_t trailing_a = Num0BitsBelowLS1Bit_Nonzero32(a);
    const size_t trailing_b = Num0BitsBelowLS1Bit_Nonzero32(b);
    // If both have at least one trailing zero, they are both divisible by 2.
    if (HWY_MIN(trailing_a, trailing_b) != 0) return false;

    // If one of them has a trailing zero, shift it out.
    a >>= trailing_a;
    b >>= trailing_b;

    for (;;) {
      // Swap such that a >= b.
      const uint32_t tmp_a = a;
      a = HWY_MAX(tmp_a, b);
      b = HWY_MIN(tmp_a, b);

      // When the smaller number is 1, they were coprime.
      if (b == 1) return true;

      a -= b;
      // a == b means there was a common factor, so not coprime.
      if (a == 0) return false;
      a >>= Num0BitsBelowLS1Bit_Nonzero32(a);
    }
  }

  // Returns another coprime >= `start`, or 1 for small `size`.
  // Used to seed independent ShuffledIota instances.
  static uint32_t FindAnotherCoprime(uint32_t size, uint32_t start) {
    if (size <= 2) {
      return 1;
    }

    // Avoids even x for even sizes, which are sure to be rejected.
    const uint32_t inc = (size & 1) ? 1 : 2;

    for (uint32_t x = start | 1; x < start + size * 16; x += inc) {
      if (CoprimeNonzero(x, static_cast<uint32_t>(size))) {
        return x;
      }
    }

    HWY_UNREACHABLE;
  }

  uint32_t coprime_;
};

// 'Policies' suitable for various worker counts and locality. To define a
// new class, add an enum and update `ToString` plus `CallWithConfig`. The
// enumerators must be contiguous so we can iterate over them.
enum class WaitType : uint8_t {
  kBlock,
  kSpin1,
  kSpinSeparate,
  kSentinel  // Must be last.
};

// For printing which is in use.
static inline const char* ToString(WaitType type) {
  switch (type) {
    case WaitType::kBlock:
      return "Block";
    case WaitType::kSpin1:
      return "Single";
    case WaitType::kSpinSeparate:
      return "Separate";
    case WaitType::kSentinel:
      return nullptr;
  }
}

// Parameters governing the main and worker thread behavior. Can be updated at
// runtime via `SetWaitMode`, which calls `SendConfig`. Both have copies which
// are carefully synchronized. 32 bits leave room for two future fields.
// 64 bits would also be fine because this does not go through futex.
struct Config {  // 4 bytes
  static std::vector<Config> AllCandidates(PoolWaitMode wait_mode) {
    std::vector<Config> candidates;

    if (wait_mode == PoolWaitMode::kSpin) {
      std::vector<SpinType> spin_types;
      spin_types.reserve(2);
      spin_types.push_back(DetectSpin());
      // Monitor-based spin may be slower, so also try Pause.
      if (spin_types[0] != SpinType::kPause) {
        spin_types.push_back(SpinType::kPause);
      }

      // All except `kBlock`.
      std::vector<WaitType> wait_types;
      for (size_t wait = 0;; ++wait) {
        const WaitType wait_type = static_cast<WaitType>(wait);
        if (wait_type == WaitType::kSentinel) break;
        if (wait_type != WaitType::kBlock) wait_types.push_back(wait_type);
      }

      candidates.reserve(spin_types.size() * wait_types.size());
      for (const SpinType spin_type : spin_types) {
        for (const WaitType wait_type : wait_types) {
          candidates.emplace_back(spin_type, wait_type);
        }
      }
    } else {
      // kBlock does not use spin, so there is only one candidate.
      candidates.emplace_back(SpinType::kPause, WaitType::kBlock);
    }

    return candidates;
  }

  std::string ToString() const {
    char buf[128];
    snprintf(buf, sizeof(buf), "%-14s %-9s", hwy::ToString(spin_type),
             pool::ToString(wait_type));
    return buf;
  }

  Config(SpinType spin_type_in, WaitType wait_type_in)
      : spin_type(spin_type_in), wait_type(wait_type_in) {}
  // Workers initially spin until ThreadPool sends them their actual config.
  Config() : Config(SpinType::kPause, WaitType::kSpinSeparate) {}

  SpinType spin_type;
  WaitType wait_type;
  HWY_MEMBER_VAR_MAYBE_UNUSED uint8_t reserved[2];
};
static_assert(sizeof(Config) == 4, "");

#if PROFILER_ENABLED

// Accumulates timings and stats from main thread and workers.
class Stats {
  // Up to `HWY_ALIGNMENT / 8` slots/offsets, passed to `PerThread`.
  static constexpr size_t kDWait = 0;
  static constexpr size_t kWaitReps = 1;
  static constexpr size_t kTBeforeRun = 2;
  static constexpr size_t kDRun = 3;
  static constexpr size_t kTasksStatic = 4;
  static constexpr size_t kTasksDynamic = 5;
  static constexpr size_t kTasksStolen = 6;
  static constexpr size_t kDFuncStatic = 7;
  static constexpr size_t kDFuncDynamic = 8;
  static constexpr size_t kSentinel = 9;

 public:
  Stats() {
    for (size_t thread_idx = 0; thread_idx < kMaxThreads; ++thread_idx) {
      for (size_t offset = 0; offset < kSentinel; ++offset) {
        PerThread(thread_idx, offset) = 0;
      }
    }
    Reset();
  }

  // Called by the N lowest-indexed workers that got one of the N tasks, which
  // includes the main thread because its index is 0.
  // `d_*` denotes "difference" (of timestamps) and thus also duration.
  void NotifyRunStatic(size_t worker_idx, timer::Ticks d_func) {
    if (worker_idx == 0) {  // main thread
      num_run_static_++;
      sum_tasks_static_++;
      sum_d_func_static_ += d_func;
    } else {
      const size_t thread_idx = worker_idx - 1;
      // Defer the sums until `NotifyMainRun` to avoid atomic RMW.
      PerThread(thread_idx, kTasksStatic)++;
      PerThread(thread_idx, kDFuncStatic) += d_func;
    }
  }

  // Called by all workers, including the main thread, regardless of whether
  // they actually stole or even ran a task.
  void NotifyRunDynamic(size_t worker_idx, size_t tasks, size_t stolen,
                        timer::Ticks d_func) {
    if (worker_idx == 0) {  // main thread
      num_run_dynamic_++;
      sum_tasks_dynamic_ += tasks;
      sum_tasks_stolen_ += stolen;
      sum_d_func_dynamic_ += d_func;
    } else {
      const size_t thread_idx = worker_idx - 1;
      // Defer the sums until `NotifyMainRun` to avoid atomic RMW.
      PerThread(thread_idx, kTasksDynamic) += tasks;
      PerThread(thread_idx, kTasksStolen) += stolen;
      PerThread(thread_idx, kDFuncDynamic) += d_func;
    }
  }

  // Called concurrently by non-main worker threads after their `WorkerRun` and
  // before the barrier.
  void NotifyThreadRun(size_t worker_idx, timer::Ticks d_wait, size_t wait_reps,
                       timer::Ticks t_before_run, timer::Ticks d_run) {
    HWY_DASSERT(worker_idx != 0);  // Not called by main thread.
    const size_t thread_idx = worker_idx - 1;
    HWY_DASSERT(PerThread(thread_idx, kDWait) == 0);
    HWY_DASSERT(PerThread(thread_idx, kWaitReps) == 0);
    HWY_DASSERT(PerThread(thread_idx, kTBeforeRun) == 0);
    HWY_DASSERT(PerThread(thread_idx, kDRun) == 0);
    PerThread(thread_idx, kDWait) = d_wait;
    PerThread(thread_idx, kWaitReps) = wait_reps;
    PerThread(thread_idx, kTBeforeRun) = t_before_run;  // For wake latency.
    PerThread(thread_idx, kDRun) = d_run;
  }

  // Called by the main thread after the barrier, whose store-release and
  // load-acquire publishes all prior writes. Note: only the main thread can
  // store `after_barrier`. If workers did, which by definition happens after
  // the barrier, then they would race with this function's reads.
  void NotifyMainRun(size_t num_threads, timer::Ticks t_before_wake,
                     timer::Ticks d_wake, timer::Ticks d_main_run,
                     timer::Ticks d_barrier) {
    HWY_DASSERT(num_threads <= kMaxThreads);

    timer::Ticks min_d_run = ~timer::Ticks{0};
    timer::Ticks max_d_run = 0;
    timer::Ticks sum_d_run = 0;
    for (size_t thread_idx = 0; thread_idx < num_threads; ++thread_idx) {
      sum_tasks_static_ += PerThread(thread_idx, kTasksStatic);
      sum_tasks_dynamic_ += PerThread(thread_idx, kTasksDynamic);
      sum_tasks_stolen_ += PerThread(thread_idx, kTasksStolen);
      sum_d_func_static_ += PerThread(thread_idx, kDFuncStatic);
      sum_d_func_dynamic_ += PerThread(thread_idx, kDFuncDynamic);
      sum_d_wait_ += PerThread(thread_idx, kDWait);
      sum_wait_reps_ += PerThread(thread_idx, kWaitReps);
      const timer::Ticks d_thread_run = PerThread(thread_idx, kDRun);
      min_d_run = HWY_MIN(min_d_run, d_thread_run);
      max_d_run = HWY_MAX(max_d_run, d_thread_run);
      sum_d_run += d_thread_run;
      const timer::Ticks t_before_run = PerThread(thread_idx, kTBeforeRun);

      for (size_t offset = 0; offset < kSentinel; ++offset) {
        PerThread(thread_idx, offset) = 0;
      }

      HWY_DASSERT(t_before_run != 0);
      const timer::Ticks d_latency = t_before_run - t_before_wake;
      sum_wake_latency_ += d_latency;
      max_wake_latency_ = HWY_MAX(max_wake_latency_, d_latency);
    }
    const double inv_avg_d_run =
        static_cast<double>(num_threads) / static_cast<double>(sum_d_run);
    // Ratios of min and max run times to the average, for this pool.Run.
    const double r_min = static_cast<double>(min_d_run) * inv_avg_d_run;
    const double r_max = static_cast<double>(max_d_run) * inv_avg_d_run;

    num_run_++;  // `num_run_*` are incremented by `NotifyRun*`.
    sum_d_run_ += sum_d_run;
    sum_r_min_ += r_min;  // For average across all pool.Run.
    sum_r_max_ += r_max;

    sum_d_wake_ += d_wake;  // `*wake_latency_` are updated above.
    sum_d_barrier_ += d_barrier;

    sum_d_run_ += d_main_run;
    sum_d_run_main_ += d_main_run;
  }

  void PrintAndReset(size_t num_threads, timer::Ticks d_thread_lifetime_ticks) {
    // This is unconditionally called via `ProfilerFunc`. If the pool was unused
    // in this invocation, skip it.
    if (num_run_ == 0) return;
    HWY_ASSERT(num_run_ == num_run_static_ + num_run_dynamic_);

    const double d_func_static = Seconds(sum_d_func_static_);
    const double d_func_dynamic = Seconds(sum_d_func_dynamic_);
    const double sum_d_run = Seconds(sum_d_run_);
    const double func_div_run = (d_func_static + d_func_dynamic) / sum_d_run;
    if (!(0.95 <= func_div_run && func_div_run <= 1.0)) {
      HWY_WARN("Func time %f should be similar to total run %f.",
               d_func_static + d_func_dynamic, sum_d_run);
    }
    const double sum_d_run_main = Seconds(sum_d_run_main_);
    const double max_wake_latency = Seconds(max_wake_latency_);
    const double sum_d_wait = Seconds(sum_d_wait_);
    const double d_thread_lifetime = Seconds(d_thread_lifetime_ticks);

    const double inv_run = 1.0 / static_cast<double>(num_run_);
    const auto per_run = [inv_run](double sum) { return sum * inv_run; };
    const double avg_d_wake = per_run(Seconds(sum_d_wake_));
    const double avg_wake_latency = per_run(Seconds(sum_wake_latency_));
    const double avg_d_wait = per_run(sum_d_wait);
    const double avg_wait_reps = per_run(static_cast<double>(sum_wait_reps_));
    const double avg_d_barrier = per_run(Seconds(sum_d_barrier_));
    const double avg_r_min = per_run(sum_r_min_);
    const double avg_r_max = per_run(sum_r_max_);

    const size_t num_workers = 1 + num_threads;
    const double avg_tasks_static =
        Avg(sum_tasks_static_, num_run_static_ * num_workers);
    const double avg_tasks_dynamic =
        Avg(sum_tasks_dynamic_, num_run_dynamic_ * num_workers);
    const double avg_steals =
        Avg(sum_tasks_stolen_, num_run_dynamic_ * num_workers);
    const double avg_d_run = sum_d_run / num_workers;

    const double pc_wait = sum_d_wait / d_thread_lifetime * 100.0;
    const double pc_run = sum_d_run / d_thread_lifetime * 100.0;
    const double pc_main = sum_d_run_main / avg_d_run * 100.0;

    const auto us = [](double sec) { return sec * 1E6; };
    const auto ns = [](double sec) { return sec * 1E9; };
    printf(
        "%3zu: %5d x %.2f/%5d x %4.1f tasks, %.2f steals; "
        "wake %7.3f ns, latency %6.3f < %7.3f us, barrier %7.3f us; "
        "wait %.1f us (%6.0f reps, %4.1f%%), balance %4.1f%%-%5.1f%%, "
        "func: %6.3f + %7.3f, "
        "%.1f%% of thread time %7.3f s; main:worker %5.1f%%\n",
        num_threads, num_run_static_, avg_tasks_static, num_run_dynamic_,
        avg_tasks_dynamic, avg_steals, ns(avg_d_wake), us(avg_wake_latency),
        us(max_wake_latency), us(avg_d_barrier), us(avg_d_wait), avg_wait_reps,
        pc_wait, avg_r_min * 100.0, avg_r_max * 100.0, d_func_static,
        d_func_dynamic, pc_run, d_thread_lifetime, pc_main);

    Reset(num_threads);
  }

  void Reset(size_t num_threads = kMaxThreads) {
    num_run_ = 0;
    num_run_static_ = 0;
    num_run_dynamic_ = 0;

    sum_tasks_stolen_ = 0;
    sum_tasks_static_ = 0;
    sum_tasks_dynamic_ = 0;

    sum_d_wake_ = 0;
    sum_wake_latency_ = 0;
    max_wake_latency_ = 0;
    sum_d_wait_ = 0;
    sum_wait_reps_ = 0;
    sum_d_barrier_ = 0;

    sum_d_func_static_ = 0;
    sum_d_func_dynamic_ = 0;
    sum_r_min_ = 0.0;
    sum_r_max_ = 0.0;
    sum_d_run_ = 0;
    sum_d_run_main_ = 0;
    // ctor and `NotifyMainRun` already reset `PerThread`.
  }

 private:
  template <typename T>
  static double Avg(T sum, size_t div) {
    return div == 0 ? 0.0 : static_cast<double>(sum) / static_cast<double>(div);
  }

  static constexpr size_t kU64PerLine = HWY_ALIGNMENT / sizeof(uint64_t);

  uint64_t& PerThread(size_t thread_idx, size_t offset) {
    HWY_DASSERT(thread_idx < kMaxThreads);
    HWY_DASSERT(offset < kSentinel);
    return per_thread_[thread_idx * kU64PerLine + offset];
  }

  int32_t num_run_;
  int32_t num_run_static_;
  int32_t num_run_dynamic_;

  int32_t sum_tasks_stolen_;
  int64_t sum_tasks_static_;
  int64_t sum_tasks_dynamic_;

  timer::Ticks sum_d_wake_;
  timer::Ticks sum_wake_latency_;
  timer::Ticks max_wake_latency_;
  timer::Ticks sum_d_wait_;
  uint64_t sum_wait_reps_;
  timer::Ticks sum_d_barrier_;

  timer::Ticks sum_d_func_static_;
  timer::Ticks sum_d_func_dynamic_;
  double sum_r_min_;
  double sum_r_max_;
  timer::Ticks sum_d_run_;
  timer::Ticks sum_d_run_main_;

  // One cache line per pool thread to avoid false sharing.
  uint64_t per_thread_[kMaxThreads * kU64PerLine];
};
// Enables shift rather than multiplication.
static_assert(sizeof(Stats) == (kMaxThreads + 1) * HWY_ALIGNMENT, "Wrong size");

// Non-power of two to avoid 2K aliasing.
HWY_INLINE_VAR constexpr size_t kMaxCallers = 60;

// Per-caller stats, stored in `PerCluster`.
class CallerAccumulator {
 public:
  bool Any() const { return calls_ != 0; }

  void Add(size_t tasks, size_t workers, bool is_root, timer::Ticks wait_before,
           timer::Ticks elapsed) {
    calls_++;
    root_ += is_root;
    workers_ += workers;
    min_tasks_ = HWY_MIN(min_tasks_, tasks);
    max_tasks_ = HWY_MAX(max_tasks_, tasks);
    tasks_ += tasks;
    wait_before_ += wait_before;
    elapsed_ += elapsed;
  }

  void AddFrom(const CallerAccumulator& other) {
    calls_ += other.calls_;
    root_ += other.root_;
    workers_ += other.workers_;
    min_tasks_ = HWY_MIN(min_tasks_, other.min_tasks_);
    max_tasks_ = HWY_MAX(max_tasks_, other.max_tasks_);
    tasks_ += other.tasks_;
    wait_before_ += other.wait_before_;
    elapsed_ += other.elapsed_;
  }

  bool operator>(const CallerAccumulator& other) const {
    return elapsed_ > other.elapsed_;
  }

  void PrintAndReset(const char* caller, size_t active_clusters) {
    if (!Any()) return;
    HWY_ASSERT(root_ <= calls_);
    const double inv_calls = 1.0 / static_cast<double>(calls_);
    const double pc_root = static_cast<double>(root_) * inv_calls * 100.0;
    const double avg_workers = static_cast<double>(workers_) * inv_calls;
    const double avg_tasks = static_cast<double>(tasks_) * inv_calls;
    const double avg_tasks_per_worker = avg_tasks / avg_workers;
    const double inv_freq = 1.0 / platform::InvariantTicksPerSecond();
    const double sum_wait_before = static_cast<double>(wait_before_) * inv_freq;
    const double avg_wait_before =
        root_ ? sum_wait_before / static_cast<double>(root_) : 0.0;
    const double elapsed = static_cast<double>(elapsed_) * inv_freq;
    const double avg_elapsed = elapsed * inv_calls;
    const double task_len = avg_elapsed / avg_tasks_per_worker;
    printf(
        "%40s: %7.0f x (%3.0f%%) %2zu clusters, %4.1f workers @ "
        "%5.1f tasks (%5u-%5u), "
        "%5.0f us wait, %6.1E us run (task len %6.1E us), total %6.2f s\n",
        caller, static_cast<double>(calls_), pc_root, active_clusters,
        avg_workers, avg_tasks_per_worker, static_cast<uint32_t>(min_tasks_),
        static_cast<uint32_t>(max_tasks_), avg_wait_before * 1E6,
        avg_elapsed * 1E6, task_len * 1E6, elapsed);
    *this = CallerAccumulator();
  }

  // For the grand total, only print calls and elapsed because averaging the
  // the other stats is not very useful. No need to reset because this is called
  // on a temporary.
  void PrintTotal() {
    if (!Any()) return;
    HWY_ASSERT(root_ <= calls_);
    const double elapsed =
        static_cast<double>(elapsed_) / platform::InvariantTicksPerSecond();
    printf("TOTAL: %7.0f x run %6.2f s\n", static_cast<double>(calls_),
           elapsed);
  }

 private:
  int64_t calls_ = 0;
  int64_t root_ = 0;
  uint64_t workers_ = 0;
  uint64_t min_tasks_ = ~uint64_t{0};
  uint64_t max_tasks_ = 0;
  uint64_t tasks_ = 0;
  // both are wall time for root Run, otherwise CPU time.
  timer::Ticks wait_before_ = 0;
  timer::Ticks elapsed_ = 0;
};
static_assert(sizeof(CallerAccumulator) == 64, "");

class PerCluster {
 public:
  CallerAccumulator& Get(size_t caller_idx) {
    HWY_DASSERT(caller_idx < kMaxCallers);
    callers_.Set(caller_idx);
    return accumulators_[caller_idx];
  }

  template <class Func>
  void ForeachCaller(Func&& func) {
    callers_.Foreach([&](size_t caller_idx) {
      func(caller_idx, accumulators_[caller_idx]);
    });
  }

  // Returns indices (required for `StringTable::Name`) in descending order of
  // elapsed time.
  std::vector<size_t> Sorted() {
    std::vector<size_t> vec;
    vec.reserve(kMaxCallers);
    ForeachCaller([&](size_t caller_idx, CallerAccumulator&) {
      vec.push_back(caller_idx);
    });
    std::sort(vec.begin(), vec.end(), [&](size_t a, size_t b) {
      return accumulators_[a] > accumulators_[b];
    });
    return vec;
  }

  // Caller takes care of resetting `accumulators_`.
  void ResetBits() { callers_ = hwy::BitSet<kMaxCallers>(); }

 private:
  CallerAccumulator accumulators_[kMaxCallers];
  hwy::BitSet<kMaxCallers> callers_;
};

// Type-safe wrapper.
class Caller {
 public:
  Caller() : idx_(0) {}  // `AddCaller` never returns 0.
  explicit Caller(size_t idx) : idx_(idx) { HWY_DASSERT(idx < kMaxCallers); }
  size_t Idx() const { return idx_; }

 private:
  size_t idx_;
};

// Singleton, shared by all ThreadPool.
class Shared {
 public:
  static HWY_CONTRIB_DLLEXPORT Shared& Get();  // Thread-safe.

  Stopwatch MakeStopwatch() const { return Stopwatch(timer_); }
  Stopwatch& LastRootEnd() { return last_root_end_; }

  // Thread-safe. Calls with the same `name` return the same `Caller`.
  Caller AddCaller(const char* name) { return Caller(callers_.Add(name)); }

  PerCluster& Cluster(size_t cluster_idx) {
    HWY_DASSERT(cluster_idx < kMaxClusters);
    return per_cluster_[cluster_idx];
  }

  // Called from the main thread via `Profiler::PrintResults`.
  void PrintAndReset() {
    // Start counting pools (= one per cluster) invoked by each caller.
    size_t active_clusters[kMaxCallers] = {};
    per_cluster_[0].ForeachCaller(
        [&](size_t caller_idx, CallerAccumulator& acc) {
          active_clusters[caller_idx] = acc.Any();
        });
    // Reduce per-cluster accumulators into the first cluster.
    for (size_t cluster_idx = 1; cluster_idx < kMaxClusters; ++cluster_idx) {
      per_cluster_[cluster_idx].ForeachCaller(
          [&](size_t caller_idx, CallerAccumulator& acc) {
            active_clusters[caller_idx] += acc.Any();
            per_cluster_[0].Get(caller_idx).AddFrom(acc);
            acc = CallerAccumulator();
          });
      per_cluster_[cluster_idx].ResetBits();
    }

    CallerAccumulator total;
    for (size_t caller_idx : per_cluster_[0].Sorted()) {
      CallerAccumulator& acc = per_cluster_[0].Get(caller_idx);
      total.AddFrom(acc);  // must be before PrintAndReset.
      acc.PrintAndReset(callers_.Name(caller_idx), active_clusters[caller_idx]);
    }
    total.PrintTotal();
    per_cluster_[0].ResetBits();
  }

 private:
  Shared()  // called via Get().
      : last_root_end_(timer_),
        send_config(callers_.Add("SendConfig")),
        dtor(callers_.Add("PoolDtor")),
        print_stats(callers_.Add("PrintStats")) {
    Profiler::Get().AddFunc(this, [this]() { PrintAndReset(); });
    // Can skip `RemoveFunc` because the singleton never dies.
  }

  const Timer timer_;
  Stopwatch last_root_end_;

  PerCluster per_cluster_[kMaxClusters];
  StringTable<kMaxCallers> callers_;

 public:
  // Returned from `callers_.Add`:
  Caller send_config;
  Caller dtor;
  Caller print_stats;
};

#else

struct Stats {
  void NotifyRunStatic(size_t, timer::Ticks) {}
  void NotifyRunDynamic(size_t, size_t, size_t, timer::Ticks) {}
  void NotifyThreadRun(size_t, timer::Ticks, size_t, timer::Ticks,
                       timer::Ticks) {}
  void NotifyMainRun(size_t, timer::Ticks, timer::Ticks, timer::Ticks,
                     timer::Ticks) {}
  void PrintAndReset(size_t, timer::Ticks) {}
  void Reset(size_t = kMaxThreads) {}
};

struct Caller {};

class Shared {
 public:
  static HWY_CONTRIB_DLLEXPORT Shared& Get();  // Thread-safe.

  Stopwatch MakeStopwatch() const { return Stopwatch(timer_); }

  Caller AddCaller(const char*) { return Caller(); }

 private:
  Shared() {}

  const Timer timer_;

 public:
  Caller send_config;
  Caller dtor;
  Caller print_stats;
};

#endif  // PROFILER_ENABLED

// Per-worker state used by both main and worker threads. `ThreadFunc`
// (threads) and `ThreadPool` (main) have a few additional members of their own.
class alignas(HWY_ALIGNMENT) Worker {  // HWY_ALIGNMENT bytes
  static constexpr size_t kMaxVictims = 4;

  static constexpr auto kAcq = std::memory_order_acquire;
  static constexpr auto kRel = std::memory_order_release;

  bool OwnsGlobalIdx() const {
#if PROFILER_ENABLED
    if (global_idx_ >= profiler::kMaxWorkers) {
      HWY_WARN("Windows-only bug? global_idx %zu >= %zu.", global_idx_,
               profiler::kMaxWorkers);
    }
#endif  // PROFILER_ENABLED
    // Across-cluster pool owns all except the main thread, which is reserved by
    // profiler.cc.
    if (cluster_idx_ == kAllClusters) return global_idx_ != 0;
    // Within-cluster pool owns all except *its* main thread, because that is
    // owned by the across-cluster pool.
    return worker_ != 0;
  }

 public:
  Worker(const size_t worker, const size_t num_threads,
         const PoolWorkerMapping mapping, const Divisor64& div_workers,
         const Stopwatch& stopwatch)
      : workers_(this - worker),
        worker_(worker),
        num_threads_(num_threads),
        stopwatch_(stopwatch),
        // If `num_threads == 0`, we might be in an inner pool and must use
        // the `global_idx` we are currently running on.
        global_idx_(num_threads == 0 ? Profiler::GlobalIdx() : mapping(worker)),
        cluster_idx_(mapping.ClusterIdx()) {
    HWY_DASSERT(IsAligned(this, HWY_ALIGNMENT));
    HWY_DASSERT(worker <= num_threads);
    const size_t num_workers = static_cast<size_t>(div_workers.GetDivisor());
    num_victims_ = static_cast<uint32_t>(HWY_MIN(kMaxVictims, num_workers));

    // Increase gap between coprimes to reduce collisions.
    const uint32_t coprime = ShuffledIota::FindAnotherCoprime(
        static_cast<uint32_t>(num_workers),
        static_cast<uint32_t>((worker + 1) * 257 + worker * 13));
    const ShuffledIota shuffled_iota(coprime);

    // To simplify `WorkerRun`, this worker is the first to 'steal' from.
    victims_[0] = static_cast<uint32_t>(worker);
    for (uint32_t i = 1; i < num_victims_; ++i) {
      victims_[i] = shuffled_iota.Next(victims_[i - 1], div_workers);
      HWY_DASSERT(victims_[i] != worker);
    }

    HWY_IF_CONSTEXPR(PROFILER_ENABLED) {
      if (HWY_LIKELY(OwnsGlobalIdx())) {
        Profiler::Get().ReserveWorker(global_idx_);
      }
    }
  }

  ~Worker() {
    HWY_IF_CONSTEXPR(PROFILER_ENABLED) {
      if (HWY_LIKELY(OwnsGlobalIdx())) {
        Profiler::Get().FreeWorker(global_idx_);
      }
    }
  }

  // Placement-newed by `WorkerLifecycle`, we do not expect any copying.
  Worker(const Worker&) = delete;
  Worker& operator=(const Worker&) = delete;

  size_t Index() const { return worker_; }
  // For work stealing.
  Worker* AllWorkers() { return workers_; }
  const Worker* AllWorkers() const { return workers_; }
  size_t NumThreads() const { return num_threads_; }

  size_t GlobalIdx() const { return global_idx_; }
  size_t ClusterIdx() const { return cluster_idx_; }

  void SetStartTime() { stopwatch_.Reset(); }
  timer::Ticks ElapsedTime() { return stopwatch_.Elapsed(); }

  // ------------------------ Per-worker storage for `SendConfig`

  Config NextConfig() const { return next_config_; }
  // Called during `SendConfig` by workers and now also the main thread. This
  // avoids a separate `ThreadPool` member which risks going out of sync.
  void SetNextConfig(Config copy) { next_config_ = copy; }

  Exit GetExit() const { return exit_; }
  void SetExit(Exit exit) { exit_ = exit; }

  uint32_t WorkerEpoch() const { return worker_epoch_; }
  uint32_t AdvanceWorkerEpoch() { return ++worker_epoch_; }

  // ------------------------ Task assignment

  // Called from the main thread.
  void SetRange(const uint64_t begin, const uint64_t end) {
    my_begin_.store(begin, kRel);
    my_end_.store(end, kRel);
  }

  uint64_t MyEnd() const { return my_end_.load(kAcq); }

  Span<const uint32_t> Victims() const {
    return hwy::Span<const uint32_t>(victims_.data(),
                                     static_cast<size_t>(num_victims_));
  }

  // Returns the next task to execute. If >= MyEnd(), it must be skipped.
  uint64_t WorkerReserveTask() {
    // TODO(janwas): replace with cooperative work-stealing.
    return my_begin_.fetch_add(1, std::memory_order_relaxed);
  }

  // ------------------------ Waiter: Threads wait for tasks

  // WARNING: some `Wait*` do not set this for all Worker instances. For
  // example, `WaitType::kBlock` only uses the first worker's `Waiter` because
  // one futex can wake multiple waiters. Hence we never load this directly
  // without going through `Wait*` policy classes, and must ensure all threads
  // use the same wait mode.

  const std::atomic<uint32_t>& Waiter() const { return wait_epoch_; }
  std::atomic<uint32_t>& MutableWaiter() { return wait_epoch_; }  // futex
  void StoreWaiter(uint32_t epoch) { wait_epoch_.store(epoch, kRel); }

  // ------------------------ Barrier: Main thread waits for workers

  // For use by `HasReached` and `UntilReached`.
  const std::atomic<uint32_t>& Barrier() const { return barrier_epoch_; }
  // Setting to `epoch` signals that the worker has reached the barrier.
  void StoreBarrier(uint32_t epoch) { barrier_epoch_.store(epoch, kRel); }

 private:
  // Set by `SetRange`:
  std::atomic<uint64_t> my_begin_;
  std::atomic<uint64_t> my_end_;

  Worker* const workers_;
  const size_t worker_;
  const size_t num_threads_;

  Stopwatch stopwatch_;  // Reset by `SetStartTime`.
  const size_t global_idx_;
  const size_t cluster_idx_;

  // Use u32 to match futex.h. These must start at the initial value of
  // `worker_epoch_`.
  std::atomic<uint32_t> wait_epoch_{1};
  std::atomic<uint32_t> barrier_epoch_{1};

  uint32_t num_victims_;  // <= kPoolMaxVictims
  std::array<uint32_t, kMaxVictims> victims_;

  // Written and read by the same thread, hence not atomic.
  Config next_config_;
  Exit exit_ = Exit::kNone;
  // thread_pool_test requires nonzero epoch.
  uint32_t worker_epoch_ = 1;

  HWY_MEMBER_VAR_MAYBE_UNUSED uint8_t
      padding_[HWY_ALIGNMENT - 56 - 6 * sizeof(void*) - sizeof(victims_)];
};
static_assert(sizeof(Worker) == HWY_ALIGNMENT, "");

// Creates/destroys `Worker` using preallocated storage. See comment at
// `ThreadPool::worker_bytes_` for why we do not dynamically allocate.
class WorkerLifecycle {  // 0 bytes
 public:
  // Placement new for `Worker` into `storage` because its ctor requires
  // the worker index. Returns array of all workers.
  static Worker* Init(uint8_t* storage, size_t num_threads,
                      PoolWorkerMapping mapping, const Divisor64& div_workers,
                      Shared& shared) {
    Worker* workers = new (storage)
        Worker(0, num_threads, mapping, div_workers, shared.MakeStopwatch());
    for (size_t worker = 1; worker <= num_threads; ++worker) {
      new (Addr(storage, worker)) Worker(worker, num_threads, mapping,
                                         div_workers, shared.MakeStopwatch());
      // Ensure pointer arithmetic is the same (will be used in Destroy).
      HWY_DASSERT(reinterpret_cast<uintptr_t>(workers + worker) ==
                  reinterpret_cast<uintptr_t>(Addr(storage, worker)));
    }

    // Publish non-atomic stores in `workers`.
    std::atomic_thread_fence(std::memory_order_release);

    return workers;
  }

  static void Destroy(Worker* workers, size_t num_threads) {
    for (size_t worker = 0; worker <= num_threads; ++worker) {
      workers[worker].~Worker();
    }
  }

 private:
  static uint8_t* Addr(uint8_t* storage, size_t worker) {
    return storage + worker * sizeof(Worker);
  }
};

// Stores arguments to `Run`: the function and range of task indices. Set by
// the main thread, read by workers including the main thread.
class Tasks {
  static constexpr auto kAcq = std::memory_order_acquire;

  // Signature of the (internal) function called from workers(s) for each
  // `task` in the [`begin`, `end`) passed to Run(). Closures (lambdas) do not
  // receive the first argument, which points to the lambda object.
  typedef void (*RunFunc)(const void* opaque, uint64_t task, size_t worker);

 public:
  Tasks() { HWY_DASSERT(IsAligned(this, 8)); }

  template <class Closure>
  void Set(uint64_t begin, uint64_t end, const Closure& closure) {
    constexpr auto kRel = std::memory_order_release;
    // `TestTasks` and `SetWaitMode` call this with `begin == end`.
    HWY_DASSERT(begin <= end);
    begin_.store(begin, kRel);
    end_.store(end, kRel);
    func_.store(static_cast<RunFunc>(&CallClosure<Closure>), kRel);
    opaque_.store(reinterpret_cast<const void*>(&closure), kRel);
  }

  // Assigns workers their share of `[begin, end)`. Called from the main
  // thread; workers are initializing or waiting for a command.
  // Negligible CPU time.
  static void DivideRangeAmongWorkers(const uint64_t begin, const uint64_t end,
                                      const Divisor64& div_workers,
                                      Worker* workers) {
    const size_t num_workers = static_cast<size_t>(div_workers.GetDivisor());
    HWY_DASSERT(num_workers > 1);  // Else Run() runs on the main thread.
    HWY_DASSERT(begin <= end);
    const size_t num_tasks = static_cast<size_t>(end - begin);

    // Assigning all remainders to the last worker causes imbalance. We instead
    // give one more to each worker whose index is less. This may be zero when
    // called from `TestTasks`.
    const size_t min_tasks = static_cast<size_t>(div_workers.Divide(num_tasks));
    const size_t remainder =
        static_cast<size_t>(div_workers.Remainder(num_tasks));

    uint64_t my_begin = begin;
    for (size_t worker = 0; worker < num_workers; ++worker) {
      const uint64_t my_end = my_begin + min_tasks + (worker < remainder);
      workers[worker].SetRange(my_begin, my_end);
      my_begin = my_end;
    }
    HWY_DASSERT(my_begin == end);
  }

  // Runs the worker's assigned range of tasks, plus work stealing if needed.
  void WorkerRun(Worker* worker, const Shared& shared, Stats& stats) const {
    if (NumTasks() > worker->NumThreads() + 1) {
      WorkerRunDynamic(worker, shared, stats);
    } else {
      WorkerRunStatic(worker, shared, stats);
    }
  }

 private:
  // Special case for <= 1 task per worker, where stealing is unnecessary.
  void WorkerRunStatic(Worker* worker, const Shared& shared,
                       Stats& stats) const {
    const uint64_t begin = begin_.load(kAcq);
    const uint64_t end = end_.load(kAcq);
    HWY_DASSERT(begin <= end);
    const size_t index = worker->Index();

    const uint64_t task = begin + index;
    // We might still have more workers than tasks, so check first.
    if (HWY_LIKELY(task < end)) {
      const void* opaque = Opaque();
      const RunFunc func = Func();
      Stopwatch stopwatch = shared.MakeStopwatch();
      func(opaque, task, index);
      stats.NotifyRunStatic(index, stopwatch.Elapsed());
    }
  }

  // Must be called for each `worker` in [0, num_workers).
  //
  // A prior version of this code attempted to assign only as much work as a
  // worker will actually use. As with OpenMP's 'guided' strategy, we assigned
  // remaining/(k*num_threads) in each iteration. Although the worst-case
  // imbalance is bounded, this required several rounds of work allocation, and
  // the atomic counter did not scale to > 30 threads.
  //
  // We now use work stealing instead, where already-finished workers look for
  // and perform work from others, as if they were that worker. This deals with
  // imbalances as they arise, but care is required to reduce contention. We
  // randomize the order in which threads choose victims to steal from.
  void WorkerRunDynamic(Worker* worker, const Shared& shared,
                        Stats& stats) const {
    Worker* workers = worker->AllWorkers();
    const size_t index = worker->Index();
    const RunFunc func = Func();
    const void* opaque = Opaque();

    size_t sum_tasks = 0;
    size_t sum_stolen = 0;
    timer::Ticks sum_d_func = 0;
    // For each worker in random order, starting with our own, attempt to do
    // all their work.
    for (uint32_t victim : worker->Victims()) {
      Worker* other_worker = workers + victim;

      // Until all of other_worker's work is done:
      const uint64_t other_end = other_worker->MyEnd();
      for (;;) {
        // The worker that first sets `task` to `other_end` exits this loop.
        // After that, `task` can be incremented up to `num_workers - 1` times,
        // once per other worker.
        const uint64_t task = other_worker->WorkerReserveTask();
        if (HWY_UNLIKELY(task >= other_end)) {
          hwy::Pause();  // Reduce coherency traffic while stealing.
          break;
        }
        Stopwatch stopwatch = shared.MakeStopwatch();
        // Pass the index we are actually running on; this is important
        // because it is the TLS index for user code.
        func(opaque, task, index);
        sum_tasks++;
        sum_stolen += worker != other_worker;
        sum_d_func += stopwatch.Elapsed();
      }
    }
    stats.NotifyRunDynamic(index, sum_tasks, sum_stolen, sum_d_func);
  }

  size_t NumTasks() const {
    return static_cast<size_t>(end_.load(kAcq) - begin_.load(kAcq));
  }

  const void* Opaque() const { return opaque_.load(kAcq); }
  RunFunc Func() const { return func_.load(kAcq); }

  // Calls closure(task, worker). Signature must match `RunFunc`.
  template <class Closure>
  static void CallClosure(const void* opaque, uint64_t task, size_t worker) {
    (*reinterpret_cast<const Closure*>(opaque))(task, worker);
  }

  std::atomic<uint64_t> begin_;
  std::atomic<uint64_t> end_;
  std::atomic<RunFunc> func_;
  std::atomic<const void*> opaque_;
};
static_assert(sizeof(Tasks) == 16 + 2 * sizeof(void*), "");

// ------------------------------ Threads wait, main wakes them

// Considerations:
// - uint32_t storage per `Worker` so we can use `futex.h`.
// - avoid atomic read-modify-write. These are implemented on x86 using a LOCK
//   prefix, which interferes with other cores' cache-coherency transactions
//   and drains our core's store buffer. We use only store-release and
//   load-acquire. Although expressed using `std::atomic`, these are normal
//   loads/stores in the strong x86 memory model.
// - prefer to avoid resetting the state. "Sense-reversing" (flipping a flag)
//   would work, but we we prefer an 'epoch' counter because it is more useful
//   and easier to understand/debug, and as fast.

// Both the main thread and each worker maintain their own counter, which are
// implicitly synchronized by the barrier. To wake, the main thread does a
// store-release, and each worker does a load-acquire. The policy classes differ
// in whether they block or spin (with pause/monitor to reduce power), and
// whether workers check their own counter or a shared one.
//
// All methods are const because they only use storage in `Worker`, and we
// prefer to pass const-references to empty classes to enable type deduction.

// Futex: blocking reduces apparent CPU usage, but has higher wake latency.
struct WaitBlock {
  // Wakes all workers by storing the current `epoch`.
  void WakeWorkers(Worker* workers, const uint32_t epoch) const {
    HWY_DASSERT(epoch != 0);
    workers[1].StoreWaiter(epoch);
    WakeAll(workers[1].MutableWaiter());  // futex: expensive syscall
  }

  // Waits until `WakeWorkers(_, epoch)` has been called.
  template <class Spin>
  size_t UntilWoken(const Worker& worker, const Spin& /*spin*/) const {
    HWY_DASSERT(worker.Index() != 0);  // main is 0
    const uint32_t epoch = worker.WorkerEpoch();
    const Worker* workers = worker.AllWorkers();
    BlockUntilDifferent(epoch - 1, workers[1].Waiter());
    return 1;  // iterations
  }
};

// Single u32: single store by the main thread. All worker threads poll this
// one cache line and thus have it in a shared state, which means the store
// will invalidate each of them, leading to more transactions than SpinSeparate.
struct WaitSpin1 {
  void WakeWorkers(Worker* workers, const uint32_t epoch) const {
    workers[1].StoreWaiter(epoch);
  }

  // Returns the number of spin-wait iterations.
  template <class Spin>
  size_t UntilWoken(const Worker& worker, const Spin& spin) const {
    HWY_DASSERT(worker.Index() != 0);  // main is 0
    const Worker* workers = worker.AllWorkers();
    const uint32_t epoch = worker.WorkerEpoch();
    return spin.UntilEqual(epoch, workers[1].Waiter());
  }
};

// Separate u32 per thread: more stores for the main thread, but each worker
// only polls its own cache line, leading to fewer cache-coherency transactions.
struct WaitSpinSeparate {
  void WakeWorkers(Worker* workers, const uint32_t epoch) const {
    for (size_t thread = 0; thread < workers->NumThreads(); ++thread) {
      workers[1 + thread].StoreWaiter(epoch);
    }
  }

  template <class Spin>
  size_t UntilWoken(const Worker& worker, const Spin& spin) const {
    HWY_DASSERT(worker.Index() != 0);  // main is 0
    const uint32_t epoch = worker.WorkerEpoch();
    return spin.UntilEqual(epoch, worker.Waiter());
  }
};

// Calls unrolled code selected by all config enums.
template <class Func, typename... Args>
HWY_INLINE void CallWithConfig(const Config& config, Func&& func,
                               Args&&... args) {
  switch (config.wait_type) {
    case WaitType::kBlock:
      return func(SpinPause(), WaitBlock(), std::forward<Args>(args)...);
    case WaitType::kSpin1:
      return CallWithSpin(config.spin_type, func, WaitSpin1(),
                          std::forward<Args>(args)...);
    case WaitType::kSpinSeparate:
      return CallWithSpin(config.spin_type, func, WaitSpinSeparate(),
                          std::forward<Args>(args)...);
    case WaitType::kSentinel:
      HWY_UNREACHABLE;
  }
}

// ------------------------------ Barrier: Main thread waits for workers

// Similar to `WaitSpinSeparate`, a store-release of the same local epoch
// counter serves as a "have arrived" flag that does not require resetting.
class Barrier {
 public:
  void WorkerReached(Worker& worker, uint32_t epoch) const {
    HWY_DASSERT(worker.Index() != 0);  // main is 0
    worker.StoreBarrier(epoch);
  }

  // Returns true if `worker` (can be the main thread) reached the barrier.
  bool HasReached(const Worker* worker, uint32_t epoch) const {
    const uint32_t barrier = worker->Barrier().load(std::memory_order_acquire);
    HWY_DASSERT(barrier <= epoch);
    return barrier == epoch;
  }

  // Main thread loops over each worker. A "group of 2 or 4" barrier was not
  // competitive on Skylake, Granite Rapids and Zen5.
  template <class Spin>
  void UntilReached(size_t num_threads, Worker* workers, const Spin& spin,
                    uint32_t epoch) const {
    workers[0].StoreBarrier(epoch);  // for main thread HasReached.

    for (size_t i = 0; i < num_threads; ++i) {
      // TODO: log number of spin-wait iterations.
      (void)spin.UntilEqual(epoch, workers[1 + i].Barrier());
    }
  }
};

// In debug builds, detects when functions are re-entered.
class BusyFlag {
 public:
  void Set() { HWY_DASSERT(!busy_.test_and_set()); }
  void Clear() { HWY_IF_CONSTEXPR(HWY_IS_DEBUG_BUILD) busy_.clear(); }

 private:
  std::atomic_flag busy_ = ATOMIC_FLAG_INIT;
};

}  // namespace pool

// Highly efficient parallel-for, intended for workloads with thousands of
// fork-join regions which consist of calling tasks[t](i) for a few hundred i,
// using dozens of threads.
//
// To reduce scheduling overhead, we assume that tasks are statically known and
// that threads do not schedule new work themselves. This allows us to avoid
// queues and only store a counter plus the current task. The latter is a
// pointer to a lambda function, without the allocation/indirection required for
// `std::function`.
//
// To reduce fork/join latency, we choose an efficient barrier, optionally
// enable spin-waits via `SetWaitMode`, and avoid any mutex/lock. We largely
// even avoid atomic RMW operations (LOCK prefix): currently for the wait and
// barrier, in future hopefully also for work stealing.
//
// To eliminate false sharing and enable reasoning about cache line traffic, the
// class is aligned and holds all worker state.
//
// For load-balancing, we use work stealing in random order.
class alignas(HWY_ALIGNMENT) ThreadPool {
  // Used to initialize `num_threads_` from the ctor argument.
  static size_t ClampedNumThreads(size_t num_threads) {
    // Upper bound is required for `worker_bytes_`.
    if (HWY_UNLIKELY(num_threads > pool::kMaxThreads)) {
      HWY_WARN("ThreadPool: clamping num_threads %zu to %zu.", num_threads,
               pool::kMaxThreads);
      num_threads = pool::kMaxThreads;
    }
    return num_threads;
  }

 public:
  // This typically includes hyperthreads, hence it is a loose upper bound.
  // -1 because these are in addition to the main thread.
  static size_t MaxThreads() {
    LogicalProcessorSet lps;
    // This is OS dependent, but more accurate if available because it takes
    // into account restrictions set by cgroups or numactl/taskset.
    if (GetThreadAffinity(lps)) {
      return lps.Count() - 1;
    }
    return static_cast<size_t>(std::thread::hardware_concurrency() - 1);
  }

  // `num_threads` is the number of *additional* threads to spawn, which should
  // not exceed `MaxThreads()`. Note that the main thread also performs work.
  // `mapping` indicates how to map local worker_idx to global.
  ThreadPool(size_t num_threads,
             PoolWorkerMapping mapping = PoolWorkerMapping())
      : num_threads_(ClampedNumThreads(num_threads)),
        div_workers_(1 + num_threads_),
        shared_(pool::Shared::Get()),  // on first call, calls ReserveWorker(0)!
        workers_(pool::WorkerLifecycle::Init(worker_bytes_, num_threads_,
                                             mapping, div_workers_, shared_)) {
    // Leaves the default wait mode as `kBlock`, which means futex, because
    // spinning only makes sense when threads are pinned and wake latency is
    // important, so it must explicitly be requested by calling `SetWaitMode`.
    for (PoolWaitMode mode : {PoolWaitMode::kSpin, PoolWaitMode::kBlock}) {
      wait_mode_ = mode;  // for AutoTuner
      AutoTuner().SetCandidates(
          pool::Config::AllCandidates(mode));
    }

    // Skip empty pools because they do not update stats anyway.
    if (num_threads_ > 0) {
      Profiler::Get().AddFunc(this, [this]() { PrintStats(); });
    }

    threads_.reserve(num_threads_);
    for (size_t thread = 0; thread < num_threads_; ++thread) {
      threads_.emplace_back(
          ThreadFunc(workers_[1 + thread], tasks_, shared_, stats_));
    }

    // Threads' `Config` defaults to spinning. Change to `kBlock` (see above).
    // This also ensures all threads have started before we return, so that
    // startup latency is billed to the ctor, not the first `Run`.
    SendConfig(AutoTuner().Candidates()[0]);
  }

  // If we created threads, waits for them all to exit.
  ~ThreadPool() {
    // There is no portable way to request threads to exit like `ExitThread` on
    // Windows, otherwise we could call that from `Run`. Instead, we must cause
    // the thread to wake up and exit. We can just use `Run`.
    (void)RunWithoutAutotune(
        0, NumWorkers(), shared_.dtor,
        [this](HWY_MAYBE_UNUSED uint64_t task, size_t worker) {
          HWY_DASSERT(task == worker);
          workers_[worker].SetExit(Exit::kThread);
        });

    for (std::thread& thread : threads_) {
      HWY_DASSERT(thread.joinable());
      thread.join();
    }

    if (num_threads_ > 0) {
      Profiler::Get().RemoveFunc(this);
    }

    pool::WorkerLifecycle::Destroy(workers_, num_threads_);
  }

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator&(const ThreadPool&) = delete;

  // Returns number of Worker, i.e., one more than the largest `worker`
  // argument. Useful for callers that want to allocate thread-local storage.
  size_t NumWorkers() const {
    return static_cast<size_t>(div_workers_.GetDivisor());
  }

  // `mode` defaults to `kBlock`, which means futex. Switching to `kSpin`
  // reduces fork-join overhead especially when there are many calls to `Run`,
  // but wastes power when waiting over long intervals. Must not be called
  // concurrently with any `Run`, because this uses the same waiter/barrier.
  void SetWaitMode(PoolWaitMode mode) {
    wait_mode_ = mode;
    SendConfig(AutoTuneComplete() ? *AutoTuner().Best()
                                  : AutoTuner().NextConfig());
  }

  // For printing which is in use.
  pool::Config config() const { return workers_[0].NextConfig(); }

  bool AutoTuneComplete() const { return AutoTuner().Best(); }
  Span<CostDistribution> AutoTuneCosts() { return AutoTuner().Costs(); }

  static pool::Caller AddCaller(const char* name) {
    return pool::Shared::Get().AddCaller(name);
  }

  // parallel-for: Runs `closure(task, worker)` on workers for every `task` in
  // `[begin, end)`. Note that the unit of work should be large enough to
  // amortize the function call overhead, but small enough that each worker
  // processes a few tasks. Thus each `task` is usually a loop.
  //
  // Not thread-safe - concurrent parallel-for in the same `ThreadPool` are
  // forbidden unless `NumWorkers() == 1` or `end <= begin + 1`.
  template <class Closure>
  void Run(uint64_t begin, uint64_t end, pool::Caller caller,
           const Closure& closure) {
    AutoTuneT& auto_tuner = AutoTuner();
    // Already finished tuning: run without time measurement.
    if (HWY_LIKELY(auto_tuner.Best())) {
      // Don't care whether threads ran, we are done either way.
      (void)RunWithoutAutotune(begin, end, caller, closure);
      return;
    }

    // Not yet finished: measure time and notify autotuner.
    Stopwatch stopwatch(shared_.MakeStopwatch());
    // Skip update if threads didn't actually run.
    if (!RunWithoutAutotune(begin, end, caller, closure)) return;
    auto_tuner.NotifyCost(stopwatch.Elapsed());

    pool::Config next = auto_tuner.NextConfig();  // may be overwritten below
    if (auto_tuner.Best()) {  // just finished
      next = *auto_tuner.Best();
      HWY_IF_CONSTEXPR(pool::kVerbosity >= 1) {
        const size_t idx_best = static_cast<size_t>(
            auto_tuner.Best() - auto_tuner.Candidates().data());
        HWY_DASSERT(idx_best < auto_tuner.Costs().size());
        auto& AT = auto_tuner.Costs()[idx_best];
        const double best_cost = AT.EstimateCost();
        HWY_DASSERT(best_cost > 0.0);  // will divide by this below

        Stats s_ratio;
        for (size_t i = 0; i < auto_tuner.Costs().size(); ++i) {
          if (i == idx_best) continue;
          const double cost = auto_tuner.Costs()[i].EstimateCost();
          s_ratio.Notify(static_cast<float>(cost / best_cost));
        }

        fprintf(stderr,
                "Pool %3zu: %s %8.0f +/- %6.0f. Gain %.2fx [%.2fx, %.2fx]\n",
                NumWorkers(), auto_tuner.Best()->ToString().c_str(), best_cost,
                AT.Stddev(), s_ratio.GeometricMean(),
                static_cast<double>(s_ratio.Min()),
                static_cast<double>(s_ratio.Max()));
      }
    }
    SendConfig(next);
  }

  // Backward-compatible version without Caller.
  template <class Closure>
  void Run(uint64_t begin, uint64_t end, const Closure& closure) {
    Run(begin, end, pool::Caller(), closure);
  }

 private:
  // Called via `CallWithConfig`.
  struct MainWakeAndBarrier {
    template <class Spin, class Wait>
    void operator()(const Spin& spin, const Wait& wait, pool::Worker& main,
                    const pool::Tasks& tasks, const pool::Shared& shared,
                    pool::Stats& stats) const {
      const pool::Barrier barrier;
      pool::Worker* workers = main.AllWorkers();
      HWY_DASSERT(&main == main.AllWorkers());  // main is first.
      const size_t num_threads = main.NumThreads();
      const uint32_t epoch = main.AdvanceWorkerEpoch();

      HWY_IF_CONSTEXPR(HWY_IS_DEBUG_BUILD) {
        for (size_t i = 0; i < 1 + num_threads; ++i) {
          HWY_DASSERT(!barrier.HasReached(workers + i, epoch));
        }
      }

      Stopwatch stopwatch(shared.MakeStopwatch());
      const timer::Ticks t_before_wake = stopwatch.Origin();
      wait.WakeWorkers(workers, epoch);
      const timer::Ticks d_wake = stopwatch.Elapsed();

      // Also perform work on the main thread before the barrier.
      tasks.WorkerRun(&main, shared, stats);
      const timer::Ticks d_run = stopwatch.Elapsed();

      // Spin-waits until all worker *threads* (not `main`, because it already
      // knows it is here) called `WorkerReached`.
      barrier.UntilReached(num_threads, workers, spin, epoch);
      const timer::Ticks d_barrier = stopwatch.Elapsed();
      stats.NotifyMainRun(main.NumThreads(), t_before_wake, d_wake, d_run,
                          d_barrier);

      HWY_IF_CONSTEXPR(HWY_IS_DEBUG_BUILD) {
        for (size_t i = 0; i < 1 + num_threads; ++i) {
          HWY_DASSERT(barrier.HasReached(workers + i, epoch));
        }
      }

      // Threads are or will soon be waiting `UntilWoken`, which serves as the
      // 'release' phase of the barrier.
    }
  };

  // Called by `std::thread`. Could also be a lambda.
  class ThreadFunc {
    // Functor called by `CallWithConfig`. Loops until `SendConfig` changes the
    // Spin or Wait policy or the pool is destroyed.
    struct WorkerLoop {
      template <class Spin, class Wait>
      void operator()(const Spin& spin, const Wait& wait, pool::Worker& worker,
                      pool::Tasks& tasks, const pool::Shared& shared,
                      pool::Stats& stats) const {
        do {
          // Main worker also calls this, so their epochs match.
          const uint32_t epoch = worker.AdvanceWorkerEpoch();

          Stopwatch stopwatch(shared.MakeStopwatch());

          const size_t wait_reps = wait.UntilWoken(worker, spin);
          const timer::Ticks d_wait = stopwatch.Elapsed();
          const timer::Ticks t_before_run = stopwatch.Origin();

          tasks.WorkerRun(&worker, shared, stats);
          const timer::Ticks d_run = stopwatch.Elapsed();
          stats.NotifyThreadRun(worker.Index(), d_wait, wait_reps, t_before_run,
                                d_run);

          // Notify barrier after `WorkerRun`. Note that we cannot send an
          // after-barrier timestamp, see above.
          pool::Barrier().WorkerReached(worker, epoch);
          // Check after `WorkerReached`, otherwise the main thread deadlocks.
        } while (worker.GetExit() == Exit::kNone);
      }
    };

   public:
    ThreadFunc(pool::Worker& worker, pool::Tasks& tasks,
               const pool::Shared& shared, pool::Stats& stats)
        : worker_(worker), tasks_(tasks), shared_(shared), stats_(stats) {}

    void operator()() {
      // Ensure main thread's writes are visible (synchronizes with fence in
      // `WorkerLifecycle::Init`).
      std::atomic_thread_fence(std::memory_order_acquire);

      HWY_DASSERT(worker_.Index() != 0);  // main is 0
      SetThreadName("worker%03zu", static_cast<int>(worker_.Index() - 1));

      worker_.SetStartTime();
      Profiler& profiler = Profiler::Get();
      profiler.SetGlobalIdx(worker_.GlobalIdx());
      // No Zone here because it would only exit after `GetExit`, which may be
      // after the main thread's `PROFILER_END_ROOT_RUN`, and thus too late to
      // be counted. Instead, `ProfilerFunc` records the elapsed time.

      // Loop termination via `GetExit` is triggered by `~ThreadPool`.
      for (;;) {
        // Uses the initial config, or the last one set during WorkerRun.
        CallWithConfig(worker_.NextConfig(), WorkerLoop(), worker_, tasks_,
                       shared_, stats_);

        // Exit or reset the flag and return to WorkerLoop with a new config.
        if (worker_.GetExit() == Exit::kThread) break;
        worker_.SetExit(Exit::kNone);
      }

      profiler.SetGlobalIdx(~size_t{0});

      // Defer `FreeWorker` until workers are destroyed to ensure the profiler
      // is not still using the worker.
    }

   private:
    pool::Worker& worker_;
    pool::Tasks& tasks_;
    const pool::Shared& shared_;
    pool::Stats& stats_;
  };

  void PrintStats() {
    // Total run time from all non-main threads.
    std::atomic<timer::Ticks> sum_thread_elapsed{0};
    (void)RunWithoutAutotune(
        0, NumWorkers(), shared_.print_stats,
        [this, &sum_thread_elapsed](HWY_MAYBE_UNUSED uint64_t task,
                                    size_t worker) {
          HWY_DASSERT(task == worker);
          // Skip any main thread(s) because they did not init the stopwatch.
          if (worker != 0) {
            sum_thread_elapsed.fetch_add(workers_[worker].ElapsedTime());
          }
        });
    const timer::Ticks thread_total =
        sum_thread_elapsed.load(std::memory_order_acquire);
    stats_.PrintAndReset(num_threads_, thread_total);
  }

  // Returns whether threads were used. If not, there is no need to update
  // the autotuner config.
  template <class Closure>
  bool RunWithoutAutotune(uint64_t begin, uint64_t end, pool::Caller caller,
                          const Closure& closure) {
    pool::Worker& main = workers_[0];

    const size_t num_tasks = static_cast<size_t>(end - begin);
    const size_t num_workers = NumWorkers();

    // If zero or one task, or no extra threads, run on the main thread without
    // setting any member variables, because we may be re-entering Run.
    if (HWY_UNLIKELY(num_tasks <= 1 || num_workers == 1)) {
      for (uint64_t task = begin; task < end; ++task) {
        closure(task, /*worker=*/0);
      }
      return false;
    }

    busy_.Set();

#if PROFILER_ENABLED
    const bool is_root = PROFILER_IS_ROOT_RUN();
    Stopwatch stopwatch(shared_.MakeStopwatch());
    const timer::Ticks wait_before =
        is_root ? shared_.LastRootEnd().Elapsed() : 0;
#endif

    tasks_.Set(begin, end, closure);

    // More than one task per worker: use work stealing.
    if (HWY_LIKELY(num_tasks > num_workers)) {
      pool::Tasks::DivideRangeAmongWorkers(begin, end, div_workers_, workers_);
    }

    // Runs `MainWakeAndBarrier` with the first worker slot.
    CallWithConfig(config(), MainWakeAndBarrier(), main, tasks_, shared_,
                   stats_);

#if PROFILER_ENABLED
    pool::CallerAccumulator& acc =
        shared_.Cluster(main.ClusterIdx()).Get(caller.Idx());
    acc.Add(num_tasks, num_workers, is_root, wait_before, stopwatch.Elapsed());
    if (is_root) {
      PROFILER_END_ROOT_RUN();
      shared_.LastRootEnd().Reset();
    }
#else
    (void)caller;
#endif

    busy_.Clear();
    return true;
  }

  // Sends `next_config` to workers:
  // - Main wakes threads using the current config.
  // - Threads copy `next_config` into their `Worker` during `WorkerRun`.
  // - Threads notify the (same) barrier and already wait for the next wake
  //   using `next_config`.
  HWY_NOINLINE void SendConfig(pool::Config next_config) {
    (void)RunWithoutAutotune(
        0, NumWorkers(), shared_.send_config,
        [this, next_config](HWY_MAYBE_UNUSED uint64_t task, size_t worker) {
          HWY_DASSERT(task == worker);  // one task per worker
          workers_[worker].SetNextConfig(next_config);
          workers_[worker].SetExit(Exit::kLoop);
        });

    // All have woken and are, or will be, waiting per `next_config`. Now we
    // can entirely switch the main thread's config for the next wake.
    workers_[0].SetNextConfig(next_config);
  }

  using AutoTuneT = AutoTune<pool::Config, 30>;
  AutoTuneT& AutoTuner() {
    static_assert(static_cast<size_t>(PoolWaitMode::kBlock) == 1, "");
    return auto_tune_[static_cast<size_t>(wait_mode_) - 1];
  }
  const AutoTuneT& AutoTuner() const {
    return auto_tune_[static_cast<size_t>(wait_mode_) - 1];
  }

  const size_t num_threads_;  // not including main thread
  const Divisor64 div_workers_;
  pool::Shared& shared_;
  pool::Worker* const workers_;  // points into `worker_bytes_`

  alignas(HWY_ALIGNMENT) pool::Stats stats_;

  // This is written by the main thread and read by workers, via reference
  // passed to `ThreadFunc`. Padding ensures that the workers' cache lines are
  // not unnecessarily invalidated when the main thread writes other members.
  alignas(HWY_ALIGNMENT) pool::Tasks tasks_;
  HWY_MEMBER_VAR_MAYBE_UNUSED char
      padding_[HWY_ALIGNMENT - sizeof(pool::Tasks)];

  pool::BusyFlag busy_;

  // Unmodified after ctor, but cannot be const because we call thread::join().
  std::vector<std::thread> threads_;

  PoolWaitMode wait_mode_;
  AutoTuneT auto_tune_[2];  // accessed via `AutoTuner`

  // Last because it is large. Store inside `ThreadPool` so that callers can
  // bind it to the NUMA node's memory. Not stored inside `WorkerLifecycle`
  // because that class would be initialized after `workers_`.
  alignas(HWY_ALIGNMENT) uint8_t
      worker_bytes_[sizeof(pool::Worker) * (pool::kMaxThreads + 1)];
};

}  // namespace hwy

#endif  // HIGHWAY_HWY_CONTRIB_THREAD_POOL_THREAD_POOL_H_

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

#include "hwy/contrib/thread_pool/thread_pool.h"

#include <math.h>  // sqrtf
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <atomic>
#include <thread>  // NOLINT
#include <vector>

#include "hwy/base.h"  // PopCount
#include "hwy/contrib/thread_pool/spin.h"
#include "hwy/contrib/thread_pool/topology.h"
#include "hwy/profiler.h"
#include "hwy/tests/hwy_gtest.h"
#include "hwy/tests/test_util-inl.h"  // AdjustedReps

namespace hwy {
namespace pool {
namespace {

TEST(ThreadPoolTest, TestCoprime) {
  // 1 is coprime with anything
  for (uint32_t i = 1; i < 500; ++i) {
    HWY_ASSERT(ShuffledIota::CoprimeNonzero(1, i));
    HWY_ASSERT(ShuffledIota::CoprimeNonzero(i, 1));
  }

  // Powers of two >= 2 are not coprime
  for (size_t i = 1; i < 20; ++i) {
    for (size_t j = 1; j < 20; ++j) {
      HWY_ASSERT(!ShuffledIota::CoprimeNonzero(1u << i, 1u << j));
    }
  }

  // 2^x and 2^x +/- 1 are coprime
  for (size_t i = 1; i < 30; ++i) {
    const uint32_t pow2 = 1u << i;
    HWY_ASSERT(ShuffledIota::CoprimeNonzero(pow2, pow2 + 1));
    HWY_ASSERT(ShuffledIota::CoprimeNonzero(pow2, pow2 - 1));
    HWY_ASSERT(ShuffledIota::CoprimeNonzero(pow2 + 1, pow2));
    HWY_ASSERT(ShuffledIota::CoprimeNonzero(pow2 - 1, pow2));
  }

  // Random number x * random y (both >= 2) is not co-prime with x nor y.
  RandomState rng;
  for (size_t i = 1; i < 5000; ++i) {
    const uint32_t x = (Random32(&rng) & 0xFFF7) + 2;
    const uint32_t y = (Random32(&rng) & 0xFFF7) + 2;
    HWY_ASSERT(!ShuffledIota::CoprimeNonzero(x * y, x));
    HWY_ASSERT(!ShuffledIota::CoprimeNonzero(x * y, y));
    HWY_ASSERT(!ShuffledIota::CoprimeNonzero(x, x * y));
    HWY_ASSERT(!ShuffledIota::CoprimeNonzero(y, x * y));
  }

  // Primes are all coprime (list from https://oeis.org/A000040)
  static constexpr uint32_t primes[] = {
      2,   3,   5,   7,   11,  13,  17,  19,  23,  29,  31,  37,  41,  43,  47,
      53,  59,  61,  67,  71,  73,  79,  83,  89,  97,  101, 103, 107, 109, 113,
      127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197,
      199, 211, 223, 227, 229, 233, 239, 241, 251, 257, 263, 269, 271};
  for (size_t i = 0; i < sizeof(primes) / sizeof(primes[0]); ++i) {
    for (size_t j = i + 1; j < sizeof(primes) / sizeof(primes[0]); ++j) {
      HWY_ASSERT(ShuffledIota::CoprimeNonzero(primes[i], primes[j]));
      HWY_ASSERT(ShuffledIota::CoprimeNonzero(primes[j], primes[i]));
    }
  }
}

// Ensures `shuffled` visits [0, size) exactly once starting from `current`.
void VerifyPermutation(uint32_t size, const Divisor64& divisor,
                       const ShuffledIota& shuffled, uint32_t current,
                       uint32_t* visited) {
  for (size_t i = 0; i < size; i++) {
    visited[i] = 0;
  }

  for (size_t i = 0; i < size; i++) {
    ++visited[current];
    current = shuffled.Next(current, divisor);
  }

  for (size_t i = 0; i < size; i++) {
    HWY_ASSERT(visited[i] == 1);
  }
}

// Verifies ShuffledIota generates a permutation of [0, size).
TEST(ThreadPoolTest, TestRandomPermutation) {
  constexpr size_t kMaxSize = 40;
  uint32_t visited[kMaxSize];

  // Exhaustive enumeration of size and starting point.
  for (uint32_t size = 1; size < kMaxSize; ++size) {
    const Divisor64 divisor(size);

    const uint32_t coprime = ShuffledIota::FindAnotherCoprime(size, 1);
    const ShuffledIota shuffled(coprime);

    for (uint32_t start = 0; start < size; ++start) {
      VerifyPermutation(size, divisor, shuffled, start, visited);
    }
  }
}

// Verifies multiple ShuffledIota are relatively independent.
TEST(ThreadPoolTest, TestMultiplePermutations) {
  constexpr size_t kMaxSize = 40;
  uint32_t coprimes[kMaxSize];
  // One per ShuffledIota; initially the starting value, then its Next().
  uint32_t current[kMaxSize];

  for (uint32_t size = 1; size < kMaxSize; ++size) {
    const Divisor64 divisor(size);

    // Create `size` ShuffledIota instances with unique coprimes.
    std::vector<ShuffledIota> shuffled;
    for (size_t i = 0; i < size; ++i) {
      coprimes[i] = ShuffledIota::FindAnotherCoprime(
          size, static_cast<uint32_t>((i + 1) * 257 + i * 13));
      shuffled.emplace_back(coprimes[i]);
    }

    // ShuffledIota[i] starts at i to match the worker thread use case.
    for (uint32_t i = 0; i < size; ++i) {
      current[i] = i;
    }

    size_t num_bad = 0;
    uint32_t all_visited[kMaxSize] = {0};

    // For each step, ensure there are few non-unique current[].
    for (size_t step = 0; step < size; ++step) {
      // How many times is each number visited?
      uint32_t visited[kMaxSize] = {0};
      for (size_t i = 0; i < size; ++i) {
        visited[current[i]] += 1;
        all_visited[current[i]] = 1;  // visited at all across all steps?
      }

      // How many numbers are visited multiple times?
      size_t num_contended = 0;
      uint32_t max_contention = 0;
      for (size_t i = 0; i < size; ++i) {
        num_contended += visited[i] > 1;
        max_contention = HWY_MAX(max_contention, visited[i]);
      }

      // Count/print if excessive collisions.
      const size_t expected =
          static_cast<size_t>(sqrtf(static_cast<float>(size)) * 2.0f);
      if ((num_contended > expected) && (max_contention > 3)) {
        ++num_bad;
        if (true) {
          fprintf(stderr, "size %u step %zu contended %zu max contention %u\n",
                  size, step, num_contended, max_contention);
          for (size_t i = 0; i < size; ++i) {
            fprintf(stderr, "  %u\n", current[i]);
          }
          fprintf(stderr, "coprimes\n");
          for (size_t i = 0; i < size; ++i) {
            fprintf(stderr, "  %u\n", coprimes[i]);
          }
        }
      }

      // Advance all ShuffledIota generators.
      for (size_t i = 0; i < size; ++i) {
        current[i] = shuffled[i].Next(current[i], divisor);
      }
    }  // step

    // Ensure each task was visited during at least one step.
    for (size_t i = 0; i < size; ++i) {
      HWY_ASSERT(all_visited[i] != 0);
    }

    if (num_bad != 0) {
      fprintf(stderr, "size %u total bad: %zu\n", size, num_bad);
    }
    HWY_ASSERT(num_bad < kMaxSize / 10);
  }  // size
}

class DoWait {
 public:
  explicit DoWait(Worker& worker) : worker_(worker) {}

  template <class Spin, class Wait>
  void operator()(const Spin& spin, const Wait& wait) const {
    wait.UntilWoken(worker_, spin);
  }

 private:
  Worker& worker_;
};

class DoWakeWorkers {
 public:
  explicit DoWakeWorkers(Worker* workers) : workers_(workers) {}

  template <class Spin, class Wait>
  void operator()(const Spin&, const Wait& wait) const {
    wait.WakeWorkers(workers_, workers_[0].WorkerEpoch());
  }

 private:
  Worker* const workers_;
};

// Verifies that waiter(s) can be woken by another thread.
TEST(ThreadPoolTest, TestWaiter) {
  if (!hwy::HaveThreadingSupport()) return;

  // Not actual threads, but we allocate and loop over this many workers.
  for (size_t num_threads = 1; num_threads < 6; ++num_threads) {
    const size_t num_workers = 1 + num_threads;
    auto storage = hwy::AllocateAligned<uint8_t>(num_workers * sizeof(Worker));
    HWY_ASSERT(storage);
    const Divisor64 div_workers(num_workers);

    for (WaitType wait_type :
         {WaitType::kBlock, WaitType::kSpin1, WaitType::kSpinSeparate}) {
      Worker* workers =
          pool::WorkerLifecycle::Init(storage.get(), num_threads, div_workers);

      alignas(8) const Config config(SpinType::kPause, wait_type);

      // This thread acts as the "main thread", which will wake the actual main
      // and all its worker instances.
      std::thread thread([&]() {
        hwy::Profiler::InitThread();
        CallWithConfig(config, DoWakeWorkers(workers));
      });

      // main is 0
      for (size_t worker = 1; worker < num_workers; ++worker) {
        CallWithConfig(config, DoWait(workers[1]));
      }
      thread.join();

      pool::WorkerLifecycle::Destroy(workers, num_workers);
    }
  }
}

// Ensures all tasks are run. Similar to TestPool below but without threads.
TEST(ThreadPoolTest, TestTasks) {
  for (size_t num_threads = 1; num_threads <= 8; ++num_threads) {
    const size_t num_workers = num_threads + 1;
    auto storage = hwy::AllocateAligned<uint8_t>(num_workers * sizeof(Worker));
    HWY_ASSERT(storage);
    const Divisor64 div_workers(num_workers);
    Worker* workers =
        WorkerLifecycle::Init(storage.get(), num_threads, div_workers);

    constexpr uint64_t kMaxTasks = 20;
    uint64_t mementos[kMaxTasks];  // non-atomic, no threads involved.
    for (uint64_t num_tasks = 0; num_tasks < 20; ++num_tasks) {
      for (uint64_t begin = 0; begin < AdjustedReps(32); ++begin) {
        const uint64_t end = begin + num_tasks;

        ZeroBytes(mementos, sizeof(mementos));
        const auto func = [begin, end, &mementos](uint64_t task,
                                                  size_t /*worker*/) {
          HWY_ASSERT(begin <= task && task < end);

          // Store mementos ensure we visited each task.
          mementos[task - begin] = 1000 + task;
        };
        Tasks tasks;
        tasks.Set(begin, end, func);

        Tasks::DivideRangeAmongWorkers(begin, end, div_workers, workers);
        // The `tasks < workers` special case requires running by all workers.
        for (size_t worker = 0; worker < num_workers; ++worker) {
          tasks.WorkerRun(workers + worker);
        }

        // Ensure all tasks were run.
        for (uint64_t task = begin; task < end; ++task) {
          HWY_ASSERT_EQ(1000 + task, mementos[task - begin]);
        }
      }
    }

    WorkerLifecycle::Destroy(workers, num_workers);
  }
}

// Ensures task parameter is in bounds, every parameter is reached,
// pool can be reused (multiple consecutive Run calls), pool can be destroyed
// (joining with its threads), num_threads=0 works (runs on current thread).
TEST(ThreadPoolTest, TestPool) {
  if (!hwy::HaveThreadingSupport()) return;

  hwy::ThreadPool inner(0);

  constexpr uint64_t kMaxTasks = 20;
  static std::atomic<uint64_t> mementos[kMaxTasks];
  static std::atomic<uint64_t> a_begin;
  static std::atomic<uint64_t> a_end;
  static std::atomic<uint64_t> a_num_workers;

  // Called by pool; sets mementos and runs a nested but serial Run.
  const auto func = [&inner](uint64_t task, size_t worker) {
    HWY_ASSERT(worker < a_num_workers.load());
    const uint64_t begin = a_begin.load(std::memory_order_acquire);
    const uint64_t end = a_end.load(std::memory_order_acquire);

    if (!(begin <= task && task < end)) {
      HWY_ABORT("Task %d not in [%d, %d]", static_cast<int>(task),
                static_cast<int>(begin), static_cast<int>(end));
    }

    // Store mementos ensure we visited each task.
    mementos[task - begin].store(1000 + task);

    // Re-entering Run is fine on a 0-worker pool.
    inner.Run(begin, end,
              [begin, end](uint64_t inner_task, size_t inner_worker) {
                HWY_ASSERT(inner_worker == 0);
                HWY_ASSERT(begin <= inner_task && inner_task < end);
              });
  };

  for (size_t num_threads = 0; num_threads <= 6; num_threads += 3) {
    hwy::ThreadPool pool(HWY_MIN(ThreadPool::MaxThreads(), num_threads));
    a_num_workers.store(pool.NumWorkers());
    for (bool spin : {true, false}) {
      pool.SetWaitMode(spin ? PoolWaitMode::kSpin : PoolWaitMode::kBlock);

      for (uint64_t num_tasks = 0; num_tasks < kMaxTasks; ++num_tasks) {
        for (uint64_t all_begin = 0; all_begin < AdjustedReps(32);
             ++all_begin) {
          const uint64_t all_end = all_begin + num_tasks;
          a_begin.store(all_begin, std::memory_order_release);
          a_end.store(all_end, std::memory_order_release);

          for (size_t i = 0; i < kMaxTasks; ++i) {
            mementos[i].store(0);
          }

          pool.Run(all_begin, all_end, func);

          for (uint64_t task = all_begin; task < all_end; ++task) {
            const uint64_t expected = 1000 + task;
            const uint64_t actual = mementos[task - all_begin].load();
            if (expected != actual) {
              HWY_ABORT(
                  "threads %zu, tasks %d: task not run, expected %d, got %d\n",
                  num_threads, static_cast<int>(num_tasks),
                  static_cast<int>(expected), static_cast<int>(actual));
            }
          }
        }
      }
    }
  }
}

// Debug tsan builds seem to generate incorrect codegen for [&] of atomics, so
// use a pointer to a state object instead.
struct SmallAssignmentState {
  // (Avoid mutex because it may perturb the worker thread scheduling)
  std::atomic<uint64_t> num_tasks{0};
  std::atomic<uint64_t> num_workers{0};
  std::atomic<uint64_t> id_bits{0};
  std::atomic<uint64_t> num_calls{0};
};

// Verify "thread" parameter when processing few tasks.
TEST(ThreadPoolTest, TestSmallAssignments) {
  if (!hwy::HaveThreadingSupport()) return;

  static SmallAssignmentState state;

  for (size_t num_threads :
       {size_t{0}, size_t{1}, size_t{3}, size_t{5}, size_t{8}}) {
    ThreadPool pool(HWY_MIN(ThreadPool::MaxThreads(), num_threads));
    state.num_workers.store(pool.NumWorkers());

    for (size_t mul = 1; mul <= 2; ++mul) {
      const size_t num_tasks = pool.NumWorkers() * mul;
      state.num_tasks.store(num_tasks);
      state.id_bits.store(0);
      state.num_calls.store(0);

      pool.Run(0, num_tasks, [](uint64_t task, size_t worker) {
        HWY_ASSERT(task < state.num_tasks.load());
        HWY_ASSERT(worker < state.num_workers.load());

        state.num_calls.fetch_add(1);

        uint64_t bits = state.id_bits.load();
        while (!state.id_bits.compare_exchange_weak(bits,
                                                    bits | (1ULL << worker))) {
        }
      });

      // Correct number of tasks.
      const uint64_t actual_calls = state.num_calls.load();
      HWY_ASSERT(num_tasks == actual_calls);

      const size_t num_participants = PopCount(state.id_bits.load());
      // <= because some workers may not manage to run any tasks.
      HWY_ASSERT(num_participants <= pool.NumWorkers());
    }
  }
}

struct Counter {
  Counter() {
    // Suppress "unused-field" warning.
    (void)padding;
  }
  void Assimilate(const Counter& victim) { counter += victim.counter; }
  std::atomic<uint64_t> counter{0};
  uint64_t padding[15];
};

// Can switch between any wait mode, and multiple times.
TEST(ThreadPoolTest, TestWaitMode) {
  if (!hwy::HaveThreadingSupport()) return;

  ThreadPool pool(9);
  RandomState rng;
  for (size_t i = 0; i < 100; ++i) {
    pool.SetWaitMode((Random32(&rng) & 1u) ? PoolWaitMode::kSpin
                                    : PoolWaitMode::kBlock);
  }
}

TEST(ThreadPoolTest, TestCounter) {
  if (!hwy::HaveThreadingSupport()) return;

  const size_t kNumThreads = 12;
  ThreadPool pool(kNumThreads);
  for (PoolWaitMode mode : {PoolWaitMode::kSpin, PoolWaitMode::kBlock}) {
    pool.SetWaitMode(mode);
    alignas(128) Counter counters[1+kNumThreads];

    const uint64_t kNumTasks = kNumThreads * 19;
    pool.Run(0, kNumTasks,
             [&counters](const uint64_t task, const size_t worker) {
               counters[worker].counter.fetch_add(task);
             });

    uint64_t expected = 0;
    for (uint64_t i = 0; i < kNumTasks; ++i) {
      expected += i;
    }

    for (size_t i = 1; i < pool.NumWorkers(); ++i) {
      counters[0].Assimilate(counters[i]);
    }
    HWY_ASSERT_EQ(expected, counters[0].counter.load());
  }
}

}  // namespace
}  // namespace pool
}  // namespace hwy

HWY_TEST_MAIN();

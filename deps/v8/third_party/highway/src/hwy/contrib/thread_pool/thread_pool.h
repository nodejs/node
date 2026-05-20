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

// Define to HWY_NOINLINE to see profiles of `WorkerRun*` and waits.
#define HWY_POOL_PROFILE

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
  HWY_MAYBE_UNUSED uint8_t reserved[2];
};
static_assert(sizeof(Config) == 4, "");

// Per-worker state used by both main and worker threads. `ThreadFunc`
// (threads) and `ThreadPool` (main) have a few additional members of their own.
class alignas(HWY_ALIGNMENT) Worker {  // HWY_ALIGNMENT bytes
  static constexpr size_t kMaxVictims = 4;

  static constexpr auto kAcq = std::memory_order_acquire;
  static constexpr auto kRel = std::memory_order_release;

 public:
  Worker(const size_t worker, const size_t num_threads,
         const Divisor64& div_workers)
      : workers_(this - worker), worker_(worker), num_threads_(num_threads) {
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
  }

  // Placement-newed by `WorkerLifecycle`, we do not expect any copying.
  Worker(const Worker&) = delete;
  Worker& operator=(const Worker&) = delete;

  size_t Index() const { return worker_; }
  // For work stealing.
  Worker* AllWorkers() { return workers_; }
  const Worker* AllWorkers() const { return workers_; }
  size_t NumThreads() const { return num_threads_; }

  // ------------------------ Per-worker storage for `SendConfig`

  Config NextConfig() const { return next_config_; }
  // Called during `SendConfig` by workers and now also the main thread. This
  // avoids a separate `ThreadPool` member which risks going out of sync.
  void SetNextConfig(Config copy) { next_config_ = copy; }

  uint32_t GetExit() const { return exit_; }
  void SetExit(uint32_t exit) { exit_ = exit; }

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

  // Use u32 to match futex.h. These must start at the initial value of
  // `worker_epoch_`.
  std::atomic<uint32_t> wait_epoch_{1};
  std::atomic<uint32_t> barrier_epoch_{1};

  uint32_t num_victims_;  // <= kPoolMaxVictims
  std::array<uint32_t, kMaxVictims> victims_;

  // Written and read by the same thread, hence not atomic.
  Config next_config_;
  uint32_t exit_ = 0;
  // thread_pool_test requires nonzero epoch.
  uint32_t worker_epoch_ = 1;

  HWY_MAYBE_UNUSED uint8_t padding_[HWY_ALIGNMENT - 64 - sizeof(victims_)];
};
static_assert(sizeof(Worker) == HWY_ALIGNMENT, "");

// Creates/destroys `Worker` using preallocated storage. See comment at
// `ThreadPool::worker_bytes_` for why we do not dynamically allocate.
class WorkerLifecycle {  // 0 bytes
 public:
  // Placement new for `Worker` into `storage` because its ctor requires
  // the worker index. Returns array of all workers.
  static Worker* Init(uint8_t* storage, size_t num_threads,
                      const Divisor64& div_workers) {
    Worker* workers = new (storage) Worker(0, num_threads, div_workers);
    for (size_t worker = 1; worker <= num_threads; ++worker) {
      new (Addr(storage, worker)) Worker(worker, num_threads, div_workers);
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
  HWY_POOL_PROFILE void WorkerRun(Worker* worker) const {
    if (NumTasks() > worker->NumThreads() + 1) {
      WorkerRunWithStealing(worker);
    } else {
      WorkerRunSingle(worker->Index());
    }
  }

 private:
  // Special case for <= 1 task per worker, where stealing is unnecessary.
  void WorkerRunSingle(size_t worker) const {
    const uint64_t begin = begin_.load(kAcq);
    const uint64_t end = end_.load(kAcq);
    HWY_DASSERT(begin <= end);

    const uint64_t task = begin + worker;
    // We might still have more workers than tasks, so check first.
    if (HWY_LIKELY(task < end)) {
      const void* opaque = Opaque();
      const RunFunc func = Func();
      func(opaque, task, worker);
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
  HWY_POOL_PROFILE void WorkerRunWithStealing(Worker* worker) const {
    Worker* workers = worker->AllWorkers();
    const size_t index = worker->Index();
    const RunFunc func = Func();
    const void* opaque = Opaque();

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
        // Pass the index we are actually running on; this is important
        // because it is the TLS index for user code.
        func(opaque, task, index);
      }
    }
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
    default:
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

}  // namespace pool

// Highly efficient parallel-for, intended for workloads with thousands of
// fork-join regions which consist of calling tasks[t](i) for a few hundred i,
// using dozens of threads.
//
// To reduce scheduling overhead, we assume that tasks are statically known and
// that threads do not schedule new work themselves. This allows us to avoid
// queues and only store a counter plus the current task. The latter is a
// pointer to a lambda function, without the allocation/indirection required for
// std::function.
//
// To reduce fork/join latency, we choose an efficient barrier, optionally
// enable spin-waits via SetWaitMode, and avoid any mutex/lock. We largely even
// avoid atomic RMW operations (LOCK prefix): currently for the wait and
// barrier, in future hopefully also for work stealing.
//
// To eliminate false sharing and enable reasoning about cache line traffic, the
// class is aligned and holds all worker state.
//
// For load-balancing, we use work stealing in random order.
class alignas(HWY_ALIGNMENT) ThreadPool {
  // Called by `std::thread`. Could also be a lambda, but annotating with
  // `HWY_POOL_PROFILE` makes it easier to inspect the generated code.
  class ThreadFunc {
    // Functor called by `CallWithConfig`.
    // TODO: loop until config changes.
    struct WorkerWait {
      template <class Spin, class Wait>
      void operator()(const Spin& spin, const Wait& wait,
                      pool::Worker& worker) const {
        // TODO: log number of spin-wait iterations.
        (void)wait.UntilWoken(worker, spin);
      }
    };

   public:
    ThreadFunc(pool::Worker& worker, pool::Tasks& tasks)
        : worker_(worker), tasks_(tasks) {}

    HWY_POOL_PROFILE void operator()() {
      // Ensure main thread's writes are visible (synchronizes with fence in
      // `WorkerLifecycle::Init`).
      std::atomic_thread_fence(std::memory_order_acquire);

      HWY_DASSERT(worker_.Index() != 0);  // main is 0
      SetThreadName("worker%03zu", static_cast<int>(worker_.Index() - 1));
      hwy::Profiler::InitThread();

      // Loop termination via `GetExit` is triggered by `~ThreadPool`.
      do {
        // Main worker also calls this, so their epochs match.
        const uint32_t epoch = worker_.AdvanceWorkerEpoch();
        // Uses the initial config, or the last one set during WorkerRun.
        CallWithConfig(worker_.NextConfig(), WorkerWait(), worker_);

        tasks_.WorkerRun(&worker_);

        // Notify barrier after `WorkerRun`.
        pool::Barrier().WorkerReached(worker_, epoch);

        // Check after notifying the barrier, otherwise the main thread
        // deadlocks.
      } while (!worker_.GetExit());
    }

   private:
    pool::Worker& worker_;
    pool::Tasks& tasks_;
  };

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
  explicit ThreadPool(size_t num_threads)
      : num_threads_(ClampedNumThreads(num_threads)),
        div_workers_(1 + num_threads_),
        workers_(pool::WorkerLifecycle::Init(worker_bytes_, num_threads_,
                                             div_workers_)),
        have_timer_stop_(platform::HaveTimerStop(cpu100_)) {
    // Leaves the default wait mode as `kBlock`, which means futex, because
    // spinning only makes sense when threads are pinned and wake latency is
    // important, so it must explicitly be requested by calling `SetWaitMode`.
    for (PoolWaitMode mode : {PoolWaitMode::kSpin, PoolWaitMode::kBlock}) {
      wait_mode_ = mode;  // for AutoTuner
      AutoTuner().SetCandidates(
          pool::Config::AllCandidates(mode));
    }

    threads_.reserve(num_threads_);
    for (size_t thread = 0; thread < num_threads_; ++thread) {
      threads_.emplace_back(ThreadFunc(workers_[1 + thread], tasks_));
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
        0, NumWorkers(), [this](HWY_MAYBE_UNUSED uint64_t task, size_t worker) {
          HWY_DASSERT(task == worker);
          workers_[worker].SetExit(1);
        });

    for (std::thread& thread : threads_) {
      HWY_DASSERT(thread.joinable());
      thread.join();
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

  // parallel-for: Runs `closure(task, worker)` on workers for every `task` in
  // `[begin, end)`. Note that the unit of work should be large enough to
  // amortize the function call overhead, but small enough that each worker
  // processes a few tasks. Thus each `task` is usually a loop.
  //
  // Not thread-safe - concurrent parallel-for in the same `ThreadPool` are
  // forbidden unless `NumWorkers() == 1` or `end <= begin + 1`.
  template <class Closure>
  void Run(uint64_t begin, uint64_t end, const Closure& closure) {
    AutoTuneT& auto_tuner = AutoTuner();
    // Already finished tuning: run without time measurement.
    if (HWY_LIKELY(auto_tuner.Best())) {
      // Don't care whether threads ran, we are done either way.
      (void)RunWithoutAutotune(begin, end, closure);
      return;
    }

    // Not yet finished: measure time and notify autotuner.
    const uint64_t t0 = timer::Start();
    // Skip update if threads didn't actually run.
    if (!RunWithoutAutotune(begin, end, closure)) return;
    const uint64_t t1 = have_timer_stop_ ? timer::Stop() : timer::Start();
    auto_tuner.NotifyCost(t1 - t0);

    if (auto_tuner.Best()) {  // just finished
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
                AT.Stddev(), s_ratio.GeometricMean(), s_ratio.Min(),
                s_ratio.Max());
      }
      SendConfig(*auto_tuner.Best());
    } else {
      HWY_IF_CONSTEXPR(pool::kVerbosity >= 2) {
        fprintf(stderr, "Pool %3zu: %s %9lu\n", NumWorkers(),
                config().ToString().c_str(), t1 - t0);
      }
      SendConfig(auto_tuner.NextConfig());
    }
  }

 private:
  // Debug-only re-entrancy detection.
  void SetBusy() { HWY_DASSERT(!busy_.test_and_set()); }
  void ClearBusy() { HWY_IF_CONSTEXPR(HWY_IS_DEBUG_BUILD) busy_.clear(); }

  // Called via `CallWithConfig`.
  struct MainWakeAndBarrier {
    template <class Spin, class Wait>
    HWY_POOL_PROFILE void operator()(const Spin& spin, const Wait& wait,
                                     pool::Worker& main,
                                     const pool::Tasks& tasks) const {
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

      wait.WakeWorkers(workers, epoch);

      // Also perform work on the main thread before the barrier.
      tasks.WorkerRun(&main);

      // Spin-waits until all worker *threads* (not `main`, because it already
      // knows it is here) called `WorkerReached`.
      barrier.UntilReached(num_threads, workers, spin, epoch);
      HWY_IF_CONSTEXPR(HWY_IS_DEBUG_BUILD) {
        for (size_t i = 0; i < 1 + num_threads; ++i) {
          HWY_DASSERT(barrier.HasReached(workers + i, epoch));
        }
      }

      // Threads are or will soon be waiting `UntilWoken`, which serves as the
      // 'release' phase of the barrier.
    }
  };

  // Returns whether threads were used. If not, there is no need to update
  // the autotuner config.
  template <class Closure>
  bool RunWithoutAutotune(uint64_t begin, uint64_t end,
                          const Closure& closure) {
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

    SetBusy();
    const bool is_root = PROFILER_IS_ROOT_RUN();

    tasks_.Set(begin, end, closure);

    // More than one task per worker: use work stealing.
    if (HWY_LIKELY(num_tasks > num_workers)) {
      pool::Tasks::DivideRangeAmongWorkers(begin, end, div_workers_, workers_);
    }

    // Runs `MainWakeAndBarrier` with the first worker slot.
    CallWithConfig(config(), MainWakeAndBarrier(), workers_[0], tasks_);

    if (is_root) {
      PROFILER_END_ROOT_RUN();
    }
    ClearBusy();
    return true;
  }

  // Sends `next_config` to workers:
  // - Main wakes threads using the current config.
  // - Threads copy `next_config` into their `Worker` during `WorkerRun`.
  // - Threads notify the (same) barrier and already wait for the next wake
  //   using `next_config`.
  HWY_NOINLINE void SendConfig(pool::Config next_config) {
    (void)RunWithoutAutotune(
        0, NumWorkers(),
        [this, next_config](HWY_MAYBE_UNUSED uint64_t task, size_t worker) {
          HWY_DASSERT(task == worker);  // one task per worker
          workers_[worker].SetNextConfig(next_config);
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
  pool::Worker* const workers_;  // points into `worker_bytes_`
  const bool have_timer_stop_;
  char cpu100_[100];  // write-only for `HaveTimerStop` in ctor.

  // This is written by the main thread and read by workers, via reference
  // passed to `ThreadFunc`. Padding ensures that the workers' cache lines are
  // not unnecessarily invalidated when the main thread writes other members.
  alignas(HWY_ALIGNMENT) pool::Tasks tasks_;
  HWY_MAYBE_UNUSED char padding_[HWY_ALIGNMENT - sizeof(pool::Tasks)];

  // In debug builds, detects if functions are re-entered.
  std::atomic_flag busy_ = ATOMIC_FLAG_INIT;

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

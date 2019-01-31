// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBSAMPLER_SAMPLER_H_
#define V8_LIBSAMPLER_SAMPLER_H_

#include <atomic>
#include <unordered_map>

#include "include/v8.h"
#include "src/base/lazy-instance.h"
#include "src/base/macros.h"

#if V8_OS_POSIX && !V8_OS_CYGWIN && !V8_OS_FUCHSIA
#define USE_SIGNALS
#endif

namespace v8 {
namespace sampler {

// ----------------------------------------------------------------------------
// Sampler
//
// A sampler periodically samples the state of the VM and optionally
// (if used for profiling) the program counter and stack pointer for
// the thread that created it.

class Sampler {
 public:
  static const int kMaxFramesCountLog2 = 8;
  static const unsigned kMaxFramesCount = (1u << kMaxFramesCountLog2) - 1;

  // Initialize sampler.
  explicit Sampler(Isolate* isolate);
  virtual ~Sampler();

  Isolate* isolate() const { return isolate_; }

  // Performs stack sampling.
  // Clients should override this method in order to do something on samples,
  // for example buffer samples in a queue.
  virtual void SampleStack(const v8::RegisterState& regs) = 0;

  // Start and stop sampler.
  void Start();
  void Stop();

  // Whether the sampler is running (start has been called).
  bool IsActive() const { return active_.load(std::memory_order_relaxed); }

  void DoSample();

  // Used in tests to make sure that stack sampling is performed.
  unsigned js_sample_count() const { return js_sample_count_; }
  unsigned external_sample_count() const { return external_sample_count_; }
  void StartCountingSamples() {
    js_sample_count_ = 0;
    external_sample_count_ = 0;
    is_counting_samples_ = true;
  }

  class PlatformData;
  PlatformData* platform_data() const { return data_.get(); }

 protected:
  // Counts stack samples taken in various VM states.
  bool is_counting_samples_ = 0;
  unsigned js_sample_count_ = 0;
  unsigned external_sample_count_ = 0;

  void SetActive(bool value) {
    active_.store(value, std::memory_order_relaxed);
  }

  Isolate* isolate_;
  std::atomic_bool active_{false};
  std::unique_ptr<PlatformData> data_;  // Platform specific data.
  DISALLOW_IMPLICIT_CONSTRUCTORS(Sampler);
};

#ifdef USE_SIGNALS

typedef std::atomic_bool AtomicMutex;

// A helper that uses an std::atomic_bool to create a lock that is obtained on
// construction and released on destruction.
class AtomicGuard {
 public:
  // Attempt to obtain the lock represented by |atomic|. |is_blocking|
  // determines whether we will block to obtain the lock, or only make one
  // attempt to gain the lock and then stop. If we fail to gain the lock,
  // is_success will be false.
  explicit AtomicGuard(AtomicMutex* atomic, bool is_blocking = true);

  // Releases the lock represented by atomic, if it is held by this guard.
  ~AtomicGuard();

  // Whether the lock was successfully obtained in the constructor. This will
  // always be true if is_blocking was true.
  bool is_success() const;

 private:
  AtomicMutex* const atomic_;
  bool is_success_;
};

// SamplerManager keeps a list of Samplers per thread, and allows the caller to
// take a sample for every Sampler on the current thread.
class SamplerManager {
 public:
  typedef std::vector<Sampler*> SamplerList;

  // Add |sampler| to the map if it is not already present.
  void AddSampler(Sampler* sampler);

  // If |sampler| exists in the map, remove it and delete the SamplerList if
  // |sampler| was the last sampler in the list.
  void RemoveSampler(Sampler* sampler);

  // Take a sample for every sampler on the current thread. This function can
  // return without taking samples if AddSampler or RemoveSampler are being
  // concurrently called on any thread.
  void DoSample(const v8::RegisterState& state);

  // Get the lazily instantiated, global SamplerManager instance.
  static SamplerManager* instance();

 private:
  SamplerManager() = default;
  // Must be a friend so that it can access the private constructor for the
  // global lazy instance.
  friend class base::LeakyObject<SamplerManager>;

  std::unordered_map<pthread_t, SamplerList> sampler_map_;
  AtomicMutex samplers_access_counter_{false};

  DISALLOW_COPY_AND_ASSIGN(SamplerManager);
};

#endif  // USE_SIGNALS

}  // namespace sampler
}  // namespace v8

#endif  // V8_LIBSAMPLER_SAMPLER_H_

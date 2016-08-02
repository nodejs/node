// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_SAMPLER_H_
#define V8_PROFILER_SAMPLER_H_

#include "include/v8.h"

#include "src/base/atomicops.h"
#include "src/base/macros.h"

namespace v8 {
namespace internal {

class Isolate;
struct TickSample;

// ----------------------------------------------------------------------------
// Sampler
//
// A sampler periodically samples the state of the VM and optionally
// (if used for profiling) the program counter and stack pointer for
// the thread that created it.

class Sampler {
 public:
  // Initializes the Sampler support. Called once at VM startup.
  static void SetUp();
  static void TearDown();

  // Initialize sampler.
  Sampler(Isolate* isolate, int interval);
  virtual ~Sampler();

  Isolate* isolate() const { return isolate_; }
  int interval() const { return interval_; }

  // Performs stack sampling.
  void SampleStack(const v8::RegisterState& regs);

  // Start and stop sampler.
  void Start();
  void Stop();

  // Whether the sampling thread should use this Sampler for CPU profiling?
  bool IsProfiling() const {
    return base::NoBarrier_Load(&profiling_) > 0 &&
        !base::NoBarrier_Load(&has_processing_thread_);
  }
  void IncreaseProfilingDepth();
  void DecreaseProfilingDepth();

  // Whether the sampler is running (that is, consumes resources).
  bool IsActive() const { return base::NoBarrier_Load(&active_); }

  // CpuProfiler collects samples by calling DoSample directly
  // without calling Start. To keep it working, we register the sampler
  // with the CpuProfiler.
  bool IsRegistered() const { return base::NoBarrier_Load(&registered_); }

  void DoSample();
  // If true next sample must be initiated on the profiler event processor
  // thread right after latest sample is processed.
  void SetHasProcessingThread(bool value) {
    base::NoBarrier_Store(&has_processing_thread_, value);
  }

  // Used in tests to make sure that stack sampling is performed.
  unsigned js_sample_count() const { return js_sample_count_; }
  unsigned external_sample_count() const { return external_sample_count_; }
  void StartCountingSamples() {
    js_sample_count_ = 0;
    external_sample_count_ = 0;
    is_counting_samples_ = true;
  }

  class PlatformData;
  PlatformData* platform_data() const { return data_; }

 protected:
  // This method is called for each sampling period with the current
  // program counter.
  virtual void Tick(TickSample* sample) = 0;

 private:
  void SetActive(bool value) { base::NoBarrier_Store(&active_, value); }

  void SetRegistered(bool value) { base::NoBarrier_Store(&registered_, value); }

  Isolate* isolate_;
  const int interval_;
  base::Atomic32 profiling_;
  base::Atomic32 has_processing_thread_;
  base::Atomic32 active_;
  base::Atomic32 registered_;
  PlatformData* data_;  // Platform specific data.
  // Counts stack samples taken in various VM states.
  bool is_counting_samples_;
  unsigned js_sample_count_;
  unsigned external_sample_count_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(Sampler);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_SAMPLER_H_

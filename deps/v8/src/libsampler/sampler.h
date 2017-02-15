// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBSAMPLER_SAMPLER_H_
#define V8_LIBSAMPLER_SAMPLER_H_

#include "include/v8.h"

#include "src/base/atomicops.h"
#include "src/base/macros.h"

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

  // Initializes the Sampler support. Called once at VM startup.
  static void SetUp();
  static void TearDown();

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
  // Counts stack samples taken in various VM states.
  bool is_counting_samples_;
  unsigned js_sample_count_;
  unsigned external_sample_count_;

 private:
  void SetActive(bool value) { base::NoBarrier_Store(&active_, value); }
  void SetRegistered(bool value) { base::NoBarrier_Store(&registered_, value); }

  Isolate* isolate_;
  base::Atomic32 profiling_;
  base::Atomic32 has_processing_thread_;
  base::Atomic32 active_;
  base::Atomic32 registered_;
  PlatformData* data_;  // Platform specific data.
  DISALLOW_IMPLICIT_CONSTRUCTORS(Sampler);
};

}  // namespace sampler
}  // namespace v8

#endif  // V8_LIBSAMPLER_SAMPLER_H_

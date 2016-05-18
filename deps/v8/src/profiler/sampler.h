// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_SAMPLER_H_
#define V8_PROFILER_SAMPLER_H_

#include "include/v8.h"

#include "src/base/atomicops.h"
#include "src/base/platform/time.h"
#include "src/frames.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class Isolate;

// ----------------------------------------------------------------------------
// Sampler
//
// A sampler periodically samples the state of the VM and optionally
// (if used for profiling) the program counter and stack pointer for
// the thread that created it.

// TickSample captures the information collected for each sample.
struct TickSample {
  // Internal profiling (with --prof + tools/$OS-tick-processor) wants to
  // include the runtime function we're calling. Externally exposed tick
  // samples don't care.
  enum RecordCEntryFrame { kIncludeCEntryFrame, kSkipCEntryFrame };

  TickSample()
      : state(OTHER),
        pc(NULL),
        external_callback_entry(NULL),
        frames_count(0),
        has_external_callback(false),
        update_stats(true),
        top_frame_type(StackFrame::NONE) {}
  void Init(Isolate* isolate, const v8::RegisterState& state,
            RecordCEntryFrame record_c_entry_frame, bool update_stats);
  static void GetStackSample(Isolate* isolate, const v8::RegisterState& state,
                             RecordCEntryFrame record_c_entry_frame,
                             void** frames, size_t frames_limit,
                             v8::SampleInfo* sample_info);
  StateTag state;  // The state of the VM.
  Address pc;      // Instruction pointer.
  union {
    Address tos;   // Top stack value (*sp).
    Address external_callback_entry;
  };
  static const unsigned kMaxFramesCountLog2 = 8;
  static const unsigned kMaxFramesCount = (1 << kMaxFramesCountLog2) - 1;
  Address stack[kMaxFramesCount];  // Call stack.
  base::TimeTicks timestamp;
  unsigned frames_count : kMaxFramesCountLog2;  // Number of captured frames.
  bool has_external_callback : 1;
  bool update_stats : 1;  // Whether the sample should update aggregated stats.
  StackFrame::Type top_frame_type : 4;
};

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

  Isolate* isolate_;
  const int interval_;
  base::Atomic32 profiling_;
  base::Atomic32 has_processing_thread_;
  base::Atomic32 active_;
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

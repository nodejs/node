// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SAMPLER_H_
#define V8_SAMPLER_H_

#include "atomicops.h"
#include "frames.h"
#include "v8globals.h"

namespace v8 {
namespace internal {

class Isolate;

// ----------------------------------------------------------------------------
// Sampler
//
// A sampler periodically samples the state of the VM and optionally
// (if used for profiling) the program counter and stack pointer for
// the thread that created it.

struct RegisterState {
  RegisterState() : pc(NULL), sp(NULL), fp(NULL) {}
  Address pc;      // Instruction pointer.
  Address sp;      // Stack pointer.
  Address fp;      // Frame pointer.
};

// TickSample captures the information collected for each sample.
struct TickSample {
  TickSample()
      : state(OTHER),
        pc(NULL),
        external_callback(NULL),
        frames_count(0),
        has_external_callback(false),
        top_frame_type(StackFrame::NONE) {}
  void Init(Isolate* isolate, const RegisterState& state);
  StateTag state;  // The state of the VM.
  Address pc;      // Instruction pointer.
  union {
    Address tos;   // Top stack value (*sp).
    Address external_callback;
  };
  static const int kMaxFramesCount = 64;
  Address stack[kMaxFramesCount];  // Call stack.
  TimeTicks timestamp;
  int frames_count : 8;  // Number of captured frames.
  bool has_external_callback : 1;
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
  void SampleStack(const RegisterState& regs);

  // Start and stop sampler.
  void Start();
  void Stop();

  // Whether the sampling thread should use this Sampler for CPU profiling?
  bool IsProfiling() const {
    return NoBarrier_Load(&profiling_) > 0 &&
        !NoBarrier_Load(&has_processing_thread_);
  }
  void IncreaseProfilingDepth();
  void DecreaseProfilingDepth();

  // Whether the sampler is running (that is, consumes resources).
  bool IsActive() const { return NoBarrier_Load(&active_); }

  void DoSample();
  // If true next sample must be initiated on the profiler event processor
  // thread right after latest sample is processed.
  void SetHasProcessingThread(bool value) {
    NoBarrier_Store(&has_processing_thread_, value);
  }

  // Used in tests to make sure that stack sampling is performed.
  unsigned js_and_external_sample_count() const {
    return js_and_external_sample_count_;
  }
  void StartCountingSamples() {
      is_counting_samples_ = true;
      js_and_external_sample_count_ = 0;
  }

  class PlatformData;
  PlatformData* platform_data() const { return data_; }

 protected:
  // This method is called for each sampling period with the current
  // program counter.
  virtual void Tick(TickSample* sample) = 0;

 private:
  void SetActive(bool value) { NoBarrier_Store(&active_, value); }

  Isolate* isolate_;
  const int interval_;
  Atomic32 profiling_;
  Atomic32 has_processing_thread_;
  Atomic32 active_;
  PlatformData* data_;  // Platform specific data.
  bool is_counting_samples_;
  // Counts stack samples taken in JS VM state.
  unsigned js_and_external_sample_count_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(Sampler);
};


} }  // namespace v8::internal

#endif  // V8_SAMPLER_H_

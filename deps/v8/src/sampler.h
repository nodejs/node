// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

  // Is the sampler used for profiling?
  bool IsProfiling() const { return NoBarrier_Load(&profiling_) > 0; }
  void IncreaseProfilingDepth() { NoBarrier_AtomicIncrement(&profiling_, 1); }
  void DecreaseProfilingDepth() { NoBarrier_AtomicIncrement(&profiling_, -1); }

  // Whether the sampler is running (that is, consumes resources).
  bool IsActive() const { return NoBarrier_Load(&active_); }

  // Used in tests to make sure that stack sampling is performed.
  int samples_taken() const { return samples_taken_; }
  void ResetSamplesTaken() { samples_taken_ = 0; }

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
  Atomic32 active_;
  PlatformData* data_;  // Platform specific data.
  int samples_taken_;  // Counts stack samples taken.
  DISALLOW_IMPLICIT_CONSTRUCTORS(Sampler);
};


} }  // namespace v8::internal

#endif  // V8_SAMPLER_H_

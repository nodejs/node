// Copyright 2012 the V8 project authors. All rights reserved.
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

#ifndef V8_RUNTIME_PROFILER_H_
#define V8_RUNTIME_PROFILER_H_

#include "allocation.h"
#include "atomicops.h"

namespace v8 {
namespace internal {

class Isolate;
class JSFunction;
class Object;
class Semaphore;

class RuntimeProfiler {
 public:
  explicit RuntimeProfiler(Isolate* isolate);

  void OptimizeNow();

  void SetUp();
  void Reset();
  void TearDown();

  Object** SamplerWindowAddress();
  int SamplerWindowSize();

  void NotifyICChanged() { any_ic_changed_ = true; }

  // Rate limiting support.

  void UpdateSamplesAfterScavenge();
  void RemoveDeadSamples();
  void UpdateSamplesAfterCompact(ObjectVisitor* visitor);

  void AttemptOnStackReplacement(JSFunction* function);

 private:
  static const int kSamplerWindowSize = 16;

  void Optimize(JSFunction* function, const char* reason);

  void ClearSampleBuffer();

  void ClearSampleBufferNewSpaceEntries();

  int LookupSample(JSFunction* function);

  void AddSample(JSFunction* function, int weight);

  bool CodeSizeOKForOSR(Code* shared_code);

  Isolate* isolate_;

  int sampler_threshold_;
  int sampler_threshold_size_factor_;
  int sampler_ticks_until_threshold_adjustment_;

  Object* sampler_window_[kSamplerWindowSize];
  int sampler_window_position_;
  int sampler_window_weight_[kSamplerWindowSize];

  bool any_ic_changed_;
  bool code_generated_;
};

} }  // namespace v8::internal

#endif  // V8_RUNTIME_PROFILER_H_

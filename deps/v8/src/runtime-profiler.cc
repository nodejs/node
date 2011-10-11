// Copyright 2011 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "runtime-profiler.h"

#include "assembler.h"
#include "code-stubs.h"
#include "compilation-cache.h"
#include "deoptimizer.h"
#include "execution.h"
#include "global-handles.h"
#include "mark-compact.h"
#include "platform.h"
#include "scopeinfo.h"

namespace v8 {
namespace internal {


// Optimization sampler constants.
static const int kSamplerFrameCount = 2;
static const int kSamplerFrameWeight[kSamplerFrameCount] = { 2, 1 };

static const int kSamplerTicksBetweenThresholdAdjustment = 32;

static const int kSamplerThresholdInit = 3;
static const int kSamplerThresholdMin = 1;
static const int kSamplerThresholdDelta = 1;

static const int kSamplerThresholdSizeFactorInit = 3;

static const int kSizeLimit = 1500;


Atomic32 RuntimeProfiler::state_ = 0;
// TODO(isolates): Create the semaphore lazily and clean it up when no
// longer required.
Semaphore* RuntimeProfiler::semaphore_ = OS::CreateSemaphore(0);

#ifdef DEBUG
bool RuntimeProfiler::has_been_globally_setup_ = false;
#endif
bool RuntimeProfiler::enabled_ = false;


RuntimeProfiler::RuntimeProfiler(Isolate* isolate)
    : isolate_(isolate),
      sampler_threshold_(kSamplerThresholdInit),
      sampler_threshold_size_factor_(kSamplerThresholdSizeFactorInit),
      sampler_ticks_until_threshold_adjustment_(
          kSamplerTicksBetweenThresholdAdjustment),
      sampler_window_position_(0) {
  ClearSampleBuffer();
}


void RuntimeProfiler::GlobalSetup() {
  ASSERT(!has_been_globally_setup_);
  enabled_ = V8::UseCrankshaft() && FLAG_opt;
#ifdef DEBUG
  has_been_globally_setup_ = true;
#endif
}


void RuntimeProfiler::Optimize(JSFunction* function) {
  ASSERT(function->IsOptimizable());
  if (FLAG_trace_opt) {
    PrintF("[marking ");
    function->PrintName();
    PrintF(" 0x%" V8PRIxPTR, reinterpret_cast<intptr_t>(function->address()));
    PrintF(" for recompilation");
    PrintF("]\n");
  }

  // The next call to the function will trigger optimization.
  function->MarkForLazyRecompilation();
}


void RuntimeProfiler::AttemptOnStackReplacement(JSFunction* function) {
  // See AlwaysFullCompiler (in compiler.cc) comment on why we need
  // Debug::has_break_points().
  ASSERT(function->IsMarkedForLazyRecompilation());
  if (!FLAG_use_osr ||
      isolate_->DebuggerHasBreakPoints() ||
      function->IsBuiltin()) {
    return;
  }

  SharedFunctionInfo* shared = function->shared();
  // If the code is not optimizable, don't try OSR.
  if (!shared->code()->optimizable()) return;

  // We are not prepared to do OSR for a function that already has an
  // allocated arguments object.  The optimized code would bypass it for
  // arguments accesses, which is unsound.  Don't try OSR.
  if (shared->uses_arguments()) return;

  // We're using on-stack replacement: patch the unoptimized code so that
  // any back edge in any unoptimized frame will trigger on-stack
  // replacement for that frame.
  if (FLAG_trace_osr) {
    PrintF("[patching stack checks in ");
    function->PrintName();
    PrintF(" for on-stack replacement]\n");
  }

  // Get the stack check stub code object to match against.  We aren't
  // prepared to generate it, but we don't expect to have to.
  StackCheckStub check_stub;
  Object* check_code;
  MaybeObject* maybe_check_code = check_stub.TryGetCode();
  if (maybe_check_code->ToObject(&check_code)) {
    Code* replacement_code =
        isolate_->builtins()->builtin(Builtins::kOnStackReplacement);
    Code* unoptimized_code = shared->code();
    Deoptimizer::PatchStackCheckCode(unoptimized_code,
                                     Code::cast(check_code),
                                     replacement_code);
  }
}


void RuntimeProfiler::ClearSampleBuffer() {
  memset(sampler_window_, 0, sizeof(sampler_window_));
  memset(sampler_window_weight_, 0, sizeof(sampler_window_weight_));
}


int RuntimeProfiler::LookupSample(JSFunction* function) {
  int weight = 0;
  for (int i = 0; i < kSamplerWindowSize; i++) {
    Object* sample = sampler_window_[i];
    if (sample != NULL) {
      if (function == sample) {
        weight += sampler_window_weight_[i];
      }
    }
  }
  return weight;
}


void RuntimeProfiler::AddSample(JSFunction* function, int weight) {
  ASSERT(IsPowerOf2(kSamplerWindowSize));
  sampler_window_[sampler_window_position_] = function;
  sampler_window_weight_[sampler_window_position_] = weight;
  sampler_window_position_ = (sampler_window_position_ + 1) &
      (kSamplerWindowSize - 1);
}


void RuntimeProfiler::OptimizeNow() {
  HandleScope scope(isolate_);

  // Run through the JavaScript frames and collect them. If we already
  // have a sample of the function, we mark it for optimizations
  // (eagerly or lazily).
  JSFunction* samples[kSamplerFrameCount];
  int sample_count = 0;
  int frame_count = 0;
  for (JavaScriptFrameIterator it(isolate_);
       frame_count++ < kSamplerFrameCount && !it.done();
       it.Advance()) {
    JavaScriptFrame* frame = it.frame();
    JSFunction* function = JSFunction::cast(frame->function());

    // Adjust threshold each time we have processed
    // a certain number of ticks.
    if (sampler_ticks_until_threshold_adjustment_ > 0) {
      sampler_ticks_until_threshold_adjustment_--;
      if (sampler_ticks_until_threshold_adjustment_ <= 0) {
        // If the threshold is not already at the minimum
        // modify and reset the ticks until next adjustment.
        if (sampler_threshold_ > kSamplerThresholdMin) {
          sampler_threshold_ -= kSamplerThresholdDelta;
          sampler_ticks_until_threshold_adjustment_ =
              kSamplerTicksBetweenThresholdAdjustment;
        }
      }
    }

    if (function->IsMarkedForLazyRecompilation()) {
      Code* unoptimized = function->shared()->code();
      int nesting = unoptimized->allow_osr_at_loop_nesting_level();
      if (nesting == 0) AttemptOnStackReplacement(function);
      int new_nesting = Min(nesting + 1, Code::kMaxLoopNestingMarker);
      unoptimized->set_allow_osr_at_loop_nesting_level(new_nesting);
    }

    // Do not record non-optimizable functions.
    if (!function->IsOptimizable()) continue;
    samples[sample_count++] = function;

    int function_size = function->shared()->SourceSize();
    int threshold_size_factor = (function_size > kSizeLimit)
        ? sampler_threshold_size_factor_
        : 1;

    int threshold = sampler_threshold_ * threshold_size_factor;

    if (LookupSample(function) >= threshold) {
      Optimize(function);
    }
  }

  // Add the collected functions as samples. It's important not to do
  // this as part of collecting them because this will interfere with
  // the sample lookup in case of recursive functions.
  for (int i = 0; i < sample_count; i++) {
    AddSample(samples[i], kSamplerFrameWeight[i]);
  }
}


void RuntimeProfiler::NotifyTick() {
  isolate_->stack_guard()->RequestRuntimeProfilerTick();
}


void RuntimeProfiler::Setup() {
  ASSERT(has_been_globally_setup_);
  ClearSampleBuffer();
  // If the ticker hasn't already started, make sure to do so to get
  // the ticks for the runtime profiler.
  if (IsEnabled()) isolate_->logger()->EnsureTickerStarted();
}


void RuntimeProfiler::Reset() {
  sampler_threshold_ = kSamplerThresholdInit;
  sampler_threshold_size_factor_ = kSamplerThresholdSizeFactorInit;
  sampler_ticks_until_threshold_adjustment_ =
      kSamplerTicksBetweenThresholdAdjustment;
}


void RuntimeProfiler::TearDown() {
  // Nothing to do.
}


int RuntimeProfiler::SamplerWindowSize() {
  return kSamplerWindowSize;
}


// Update the pointers in the sampler window after a GC.
void RuntimeProfiler::UpdateSamplesAfterScavenge() {
  for (int i = 0; i < kSamplerWindowSize; i++) {
    Object* function = sampler_window_[i];
    if (function != NULL && isolate_->heap()->InNewSpace(function)) {
      MapWord map_word = HeapObject::cast(function)->map_word();
      if (map_word.IsForwardingAddress()) {
        sampler_window_[i] = map_word.ToForwardingAddress();
      } else {
        sampler_window_[i] = NULL;
      }
    }
  }
}


void RuntimeProfiler::HandleWakeUp(Isolate* isolate) {
  // The profiler thread must still be waiting.
  ASSERT(NoBarrier_Load(&state_) >= 0);
  // In IsolateEnteredJS we have already incremented the counter and
  // undid the decrement done by the profiler thread. Increment again
  // to get the right count of active isolates.
  NoBarrier_AtomicIncrement(&state_, 1);
  semaphore_->Signal();
}


bool RuntimeProfiler::IsSomeIsolateInJS() {
  return NoBarrier_Load(&state_) > 0;
}


bool RuntimeProfiler::WaitForSomeIsolateToEnterJS() {
  Atomic32 old_state = NoBarrier_CompareAndSwap(&state_, 0, -1);
  ASSERT(old_state >= -1);
  if (old_state != 0) return false;
  semaphore_->Wait();
  return true;
}


void RuntimeProfiler::StopRuntimeProfilerThreadBeforeShutdown(Thread* thread) {
  // Do a fake increment. If the profiler is waiting on the semaphore,
  // the returned state is 0, which can be left as an initial state in
  // case profiling is restarted later. If the profiler is not
  // waiting, the increment will prevent it from waiting, but has to
  // be undone after the profiler is stopped.
  Atomic32 new_state = NoBarrier_AtomicIncrement(&state_, 1);
  ASSERT(new_state >= 0);
  if (new_state == 0) {
    // The profiler thread is waiting. Wake it up. It must check for
    // stop conditions before attempting to wait again.
    semaphore_->Signal();
  }
  thread->Join();
  // The profiler thread is now stopped. Undo the increment in case it
  // was not waiting.
  if (new_state != 0) {
    NoBarrier_AtomicIncrement(&state_, -1);
  }
}


void RuntimeProfiler::RemoveDeadSamples() {
  for (int i = 0; i < kSamplerWindowSize; i++) {
    Object* function = sampler_window_[i];
    if (function != NULL && !HeapObject::cast(function)->IsMarked()) {
      sampler_window_[i] = NULL;
    }
  }
}


void RuntimeProfiler::UpdateSamplesAfterCompact(ObjectVisitor* visitor) {
  for (int i = 0; i < kSamplerWindowSize; i++) {
    visitor->VisitPointer(&sampler_window_[i]);
  }
}


bool RuntimeProfilerRateLimiter::SuspendIfNecessary() {
  if (!RuntimeProfiler::IsSomeIsolateInJS()) {
    return RuntimeProfiler::WaitForSomeIsolateToEnterJS();
  }
  return false;
}


} }  // namespace v8::internal

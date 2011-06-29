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


class PendingListNode : public Malloced {
 public:
  explicit PendingListNode(JSFunction* function);
  ~PendingListNode() { Destroy(); }

  PendingListNode* next() const { return next_; }
  void set_next(PendingListNode* node) { next_ = node; }
  Handle<JSFunction> function() { return Handle<JSFunction>::cast(function_); }

  // If the function is garbage collected before we've had the chance
  // to optimize it the weak handle will be null.
  bool IsValid() { return !function_.is_null(); }

  // Returns the number of microseconds this node has been pending.
  int Delay() const { return static_cast<int>(OS::Ticks() - start_); }

 private:
  void Destroy();
  static void WeakCallback(v8::Persistent<v8::Value> object, void* data);

  PendingListNode* next_;
  Handle<Object> function_;  // Weak handle.
  int64_t start_;
};


// Optimization sampler constants.
static const int kSamplerFrameCount = 2;
static const int kSamplerFrameWeight[kSamplerFrameCount] = { 2, 1 };

static const int kSamplerTicksBetweenThresholdAdjustment = 32;

static const int kSamplerThresholdInit = 3;
static const int kSamplerThresholdMin = 1;
static const int kSamplerThresholdDelta = 1;

static const int kSamplerThresholdSizeFactorInit = 3;
static const int kSamplerThresholdSizeFactorMin = 1;
static const int kSamplerThresholdSizeFactorDelta = 1;

static const int kSizeLimit = 1500;


PendingListNode::PendingListNode(JSFunction* function) : next_(NULL) {
  GlobalHandles* global_handles = Isolate::Current()->global_handles();
  function_ = global_handles->Create(function);
  start_ = OS::Ticks();
  global_handles->MakeWeak(function_.location(), this, &WeakCallback);
}


void PendingListNode::Destroy() {
  if (!IsValid()) return;
  GlobalHandles* global_handles = Isolate::Current()->global_handles();
  global_handles->Destroy(function_.location());
  function_= Handle<Object>::null();
}


void PendingListNode::WeakCallback(v8::Persistent<v8::Value>, void* data) {
  reinterpret_cast<PendingListNode*>(data)->Destroy();
}


Atomic32 RuntimeProfiler::state_ = 0;
// TODO(isolates): Create the semaphore lazily and clean it up when no
// longer required.
#ifdef ENABLE_LOGGING_AND_PROFILING
Semaphore* RuntimeProfiler::semaphore_ = OS::CreateSemaphore(0);
#endif

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
      js_ratio_(0),
      sampler_window_position_(0),
      optimize_soon_list_(NULL),
      state_window_position_(0),
      state_window_ticks_(0) {
  state_counts_[IN_NON_JS_STATE] = kStateWindowSize;
  state_counts_[IN_JS_STATE] = 0;
  STATIC_ASSERT(IN_NON_JS_STATE == 0);
  memset(state_window_, 0, sizeof(state_window_));
  ClearSampleBuffer();
}


void RuntimeProfiler::GlobalSetup() {
  ASSERT(!has_been_globally_setup_);
  enabled_ = V8::UseCrankshaft() && FLAG_opt;
#ifdef DEBUG
  has_been_globally_setup_ = true;
#endif
}


void RuntimeProfiler::Optimize(JSFunction* function, bool eager, int delay) {
  ASSERT(function->IsOptimizable());
  if (FLAG_trace_opt) {
    PrintF("[marking (%s) ", eager ? "eagerly" : "lazily");
    function->PrintName();
    PrintF(" 0x%" V8PRIxPTR, reinterpret_cast<intptr_t>(function->address()));
    PrintF(" for recompilation");
    if (delay > 0) {
      PrintF(" (delayed %0.3f ms)", static_cast<double>(delay) / 1000);
    }
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
  // If the code is not optimizable or references context slots, don't try OSR.
  if (!shared->code()->optimizable() || !shared->allows_lazy_compilation()) {
    return;
  }

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
  PendingListNode* current = optimize_soon_list_;
  while (current != NULL) {
    PendingListNode* next = current->next();
    if (current->IsValid()) {
      Handle<JSFunction> function = current->function();
      int delay = current->Delay();
      if (function->IsOptimizable()) {
        Optimize(*function, true, delay);
      }
    }
    delete current;
    current = next;
  }
  optimize_soon_list_ = NULL;

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
    int current_js_ratio = NoBarrier_Load(&js_ratio_);

    // Adjust threshold depending on the ratio of time spent
    // in JS code.
    if (current_js_ratio < 20) {
      // If we spend less than 20% of the time in JS code,
      // do not optimize.
      continue;
    } else if (current_js_ratio < 75) {
      // Below 75% of time spent in JS code, only optimize very
      // frequently used functions.
      threshold *= 3;
    }

    if (LookupSample(function) >= threshold) {
      Optimize(function, false, 0);
      isolate_->compilation_cache()->MarkForEagerOptimizing(
          Handle<JSFunction>(function));
    }
  }

  // Add the collected functions as samples. It's important not to do
  // this as part of collecting them because this will interfere with
  // the sample lookup in case of recursive functions.
  for (int i = 0; i < sample_count; i++) {
    AddSample(samples[i], kSamplerFrameWeight[i]);
  }
}


void RuntimeProfiler::OptimizeSoon(JSFunction* function) {
  if (!function->IsOptimizable()) return;
  PendingListNode* node = new PendingListNode(function);
  node->set_next(optimize_soon_list_);
  optimize_soon_list_ = node;
}


#ifdef ENABLE_LOGGING_AND_PROFILING
void RuntimeProfiler::UpdateStateRatio(SamplerState current_state) {
  SamplerState old_state = state_window_[state_window_position_];
  state_counts_[old_state]--;
  state_window_[state_window_position_] = current_state;
  state_counts_[current_state]++;
  ASSERT(IsPowerOf2(kStateWindowSize));
  state_window_position_ = (state_window_position_ + 1) &
      (kStateWindowSize - 1);
  // Note: to calculate correct ratio we have to track how many valid
  // ticks are actually in the state window, because on profiler
  // startup this number can be less than the window size.
  state_window_ticks_ = Min(kStateWindowSize, state_window_ticks_ + 1);
  NoBarrier_Store(&js_ratio_, state_counts_[IN_JS_STATE] * 100 /
                  state_window_ticks_);
}
#endif


void RuntimeProfiler::NotifyTick() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  // Record state sample.
  SamplerState state = IsSomeIsolateInJS()
      ? IN_JS_STATE
      : IN_NON_JS_STATE;
  UpdateStateRatio(state);
  isolate_->stack_guard()->RequestRuntimeProfilerTick();
#endif
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
#ifdef ENABLE_LOGGING_AND_PROFILING
  // The profiler thread must still be waiting.
  ASSERT(NoBarrier_Load(&state_) >= 0);
  // In IsolateEnteredJS we have already incremented the counter and
  // undid the decrement done by the profiler thread. Increment again
  // to get the right count of active isolates.
  NoBarrier_AtomicIncrement(&state_, 1);
  semaphore_->Signal();
  isolate->ResetEagerOptimizingData();
#endif
}


bool RuntimeProfiler::IsSomeIsolateInJS() {
  return NoBarrier_Load(&state_) > 0;
}


bool RuntimeProfiler::WaitForSomeIsolateToEnterJS() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  Atomic32 old_state = NoBarrier_CompareAndSwap(&state_, 0, -1);
  ASSERT(old_state >= -1);
  if (old_state != 0) return false;
  semaphore_->Wait();
#endif
  return true;
}


void RuntimeProfiler::WakeUpRuntimeProfilerThreadBeforeShutdown() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  semaphore_->Signal();
#endif
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
#ifdef ENABLE_LOGGING_AND_PROFILING
  static const int kNonJSTicksThreshold = 100;
  if (RuntimeProfiler::IsSomeIsolateInJS()) {
    non_js_ticks_ = 0;
  } else {
    if (non_js_ticks_ < kNonJSTicksThreshold) {
      ++non_js_ticks_;
    } else {
      return RuntimeProfiler::WaitForSomeIsolateToEnterJS();
    }
  }
#endif
  return false;
}


} }  // namespace v8::internal

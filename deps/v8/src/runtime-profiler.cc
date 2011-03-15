// Copyright 2010 the V8 project authors. All rights reserved.
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
#include "scopeinfo.h"
#include "top.h"

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


enum SamplerState {
  IN_NON_JS_STATE = 0,
  IN_JS_STATE = 1
};


// Optimization sampler constants.
static const int kSamplerFrameCount = 2;
static const int kSamplerFrameWeight[kSamplerFrameCount] = { 2, 1 };
static const int kSamplerWindowSize = 16;

static const int kSamplerTicksBetweenThresholdAdjustment = 32;

static const int kSamplerThresholdInit = 3;
static const int kSamplerThresholdMin = 1;
static const int kSamplerThresholdDelta = 1;

static const int kSamplerThresholdSizeFactorInit = 3;
static const int kSamplerThresholdSizeFactorMin = 1;
static const int kSamplerThresholdSizeFactorDelta = 1;

static const int kSizeLimit = 1500;

static int sampler_threshold = kSamplerThresholdInit;
static int sampler_threshold_size_factor = kSamplerThresholdSizeFactorInit;

static int sampler_ticks_until_threshold_adjustment =
    kSamplerTicksBetweenThresholdAdjustment;

// The ratio of ticks spent in JS code in percent.
static Atomic32 js_ratio;

static Object* sampler_window[kSamplerWindowSize] = { NULL, };
static int sampler_window_position = 0;
static int sampler_window_weight[kSamplerWindowSize] = { 0, };


// Support for pending 'optimize soon' requests.
static PendingListNode* optimize_soon_list = NULL;


PendingListNode::PendingListNode(JSFunction* function) : next_(NULL) {
  function_ = GlobalHandles::Create(function);
  start_ = OS::Ticks();
  GlobalHandles::MakeWeak(function_.location(), this, &WeakCallback);
}


void PendingListNode::Destroy() {
  if (!IsValid()) return;
  GlobalHandles::Destroy(function_.location());
  function_= Handle<Object>::null();
}


void PendingListNode::WeakCallback(v8::Persistent<v8::Value>, void* data) {
  reinterpret_cast<PendingListNode*>(data)->Destroy();
}


static bool IsOptimizable(JSFunction* function) {
  Code* code = function->code();
  return code->kind() == Code::FUNCTION && code->optimizable();
}


static void Optimize(JSFunction* function, bool eager, int delay) {
  ASSERT(IsOptimizable(function));
  if (FLAG_trace_opt) {
    PrintF("[marking (%s) ", eager ? "eagerly" : "lazily");
    function->PrintName();
    PrintF(" for recompilation");
    if (delay > 0) {
      PrintF(" (delayed %0.3f ms)", static_cast<double>(delay) / 1000);
    }
    PrintF("]\n");
  }

  // The next call to the function will trigger optimization.
  function->MarkForLazyRecompilation();
}


static void AttemptOnStackReplacement(JSFunction* function) {
  // See AlwaysFullCompiler (in compiler.cc) comment on why we need
  // Debug::has_break_points().
  ASSERT(function->IsMarkedForLazyRecompilation());
  if (!FLAG_use_osr || Debug::has_break_points() || function->IsBuiltin()) {
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
  if (shared->scope_info()->HasArgumentsShadow()) return;

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
    Code* replacement_code = Builtins::builtin(Builtins::OnStackReplacement);
    Code* unoptimized_code = shared->code();
    Deoptimizer::PatchStackCheckCode(unoptimized_code,
                                     Code::cast(check_code),
                                     replacement_code);
  }
}


static void ClearSampleBuffer() {
  for (int i = 0; i < kSamplerWindowSize; i++) {
    sampler_window[i] = NULL;
    sampler_window_weight[i] = 0;
  }
}


static int LookupSample(JSFunction* function) {
  int weight = 0;
  for (int i = 0; i < kSamplerWindowSize; i++) {
    Object* sample = sampler_window[i];
    if (sample != NULL) {
      if (function == sample) {
        weight += sampler_window_weight[i];
      }
    }
  }
  return weight;
}


static void AddSample(JSFunction* function, int weight) {
  ASSERT(IsPowerOf2(kSamplerWindowSize));
  sampler_window[sampler_window_position] = function;
  sampler_window_weight[sampler_window_position] = weight;
  sampler_window_position = (sampler_window_position + 1) &
      (kSamplerWindowSize - 1);
}


void RuntimeProfiler::OptimizeNow() {
  HandleScope scope;
  PendingListNode* current = optimize_soon_list;
  while (current != NULL) {
    PendingListNode* next = current->next();
    if (current->IsValid()) {
      Handle<JSFunction> function = current->function();
      int delay = current->Delay();
      if (IsOptimizable(*function)) {
        Optimize(*function, true, delay);
      }
    }
    delete current;
    current = next;
  }
  optimize_soon_list = NULL;

  // Run through the JavaScript frames and collect them. If we already
  // have a sample of the function, we mark it for optimizations
  // (eagerly or lazily).
  JSFunction* samples[kSamplerFrameCount];
  int sample_count = 0;
  int frame_count = 0;
  for (JavaScriptFrameIterator it;
       frame_count++ < kSamplerFrameCount && !it.done();
       it.Advance()) {
    JavaScriptFrame* frame = it.frame();
    JSFunction* function = JSFunction::cast(frame->function());

    // Adjust threshold each time we have processed
    // a certain number of ticks.
    if (sampler_ticks_until_threshold_adjustment > 0) {
      sampler_ticks_until_threshold_adjustment--;
      if (sampler_ticks_until_threshold_adjustment <= 0) {
        // If the threshold is not already at the minimum
        // modify and reset the ticks until next adjustment.
        if (sampler_threshold > kSamplerThresholdMin) {
          sampler_threshold -= kSamplerThresholdDelta;
          sampler_ticks_until_threshold_adjustment =
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
    if (!IsOptimizable(function)) continue;
    samples[sample_count++] = function;

    int function_size = function->shared()->SourceSize();
    int threshold_size_factor = (function_size > kSizeLimit)
        ? sampler_threshold_size_factor
        : 1;

    int threshold = sampler_threshold * threshold_size_factor;
    int current_js_ratio = NoBarrier_Load(&js_ratio);

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
      CompilationCache::MarkForEagerOptimizing(Handle<JSFunction>(function));
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
  if (!IsOptimizable(function)) return;
  PendingListNode* node = new PendingListNode(function);
  node->set_next(optimize_soon_list);
  optimize_soon_list = node;
}


#ifdef ENABLE_LOGGING_AND_PROFILING
static void UpdateStateRatio(SamplerState current_state) {
  static const int kStateWindowSize = 128;
  static SamplerState state_window[kStateWindowSize];
  static int state_window_position = 0;
  static int state_counts[2] = { kStateWindowSize, 0 };

  SamplerState old_state = state_window[state_window_position];
  state_counts[old_state]--;
  state_window[state_window_position] = current_state;
  state_counts[current_state]++;
  ASSERT(IsPowerOf2(kStateWindowSize));
  state_window_position = (state_window_position + 1) &
      (kStateWindowSize - 1);
  NoBarrier_Store(&js_ratio, state_counts[IN_JS_STATE] * 100 /
                  kStateWindowSize);
}
#endif


void RuntimeProfiler::NotifyTick() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  // Record state sample.
  SamplerState state = Top::IsInJSState()
      ? IN_JS_STATE
      : IN_NON_JS_STATE;
  UpdateStateRatio(state);
  StackGuard::RequestRuntimeProfilerTick();
#endif
}


void RuntimeProfiler::Setup() {
  ClearSampleBuffer();
  // If the ticker hasn't already started, make sure to do so to get
  // the ticks for the runtime profiler.
  if (IsEnabled()) Logger::EnsureTickerStarted();
}


void RuntimeProfiler::Reset() {
  sampler_threshold = kSamplerThresholdInit;
  sampler_ticks_until_threshold_adjustment =
      kSamplerTicksBetweenThresholdAdjustment;
  sampler_threshold_size_factor = kSamplerThresholdSizeFactorInit;
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
    Object* function = sampler_window[i];
    if (function != NULL && Heap::InNewSpace(function)) {
      MapWord map_word = HeapObject::cast(function)->map_word();
      if (map_word.IsForwardingAddress()) {
        sampler_window[i] = map_word.ToForwardingAddress();
      } else {
        sampler_window[i] = NULL;
      }
    }
  }
}


void RuntimeProfiler::RemoveDeadSamples() {
  for (int i = 0; i < kSamplerWindowSize; i++) {
    Object* function = sampler_window[i];
    if (function != NULL && !HeapObject::cast(function)->IsMarked()) {
      sampler_window[i] = NULL;
    }
  }
}


void RuntimeProfiler::UpdateSamplesAfterCompact(ObjectVisitor* visitor) {
  for (int i = 0; i < kSamplerWindowSize; i++) {
    visitor->VisitPointer(&sampler_window[i]);
  }
}


bool RuntimeProfilerRateLimiter::SuspendIfNecessary() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  static const int kNonJSTicksThreshold = 100;
  // We suspend the runtime profiler thread when not running
  // JavaScript. If the CPU profiler is active we must not do this
  // because it samples both JavaScript and C++ code.
  if (RuntimeProfiler::IsEnabled() &&
      !CpuProfiler::is_profiling() &&
      !(FLAG_prof && FLAG_prof_auto)) {
    if (Top::IsInJSState()) {
      non_js_ticks_ = 0;
    } else {
      if (non_js_ticks_ < kNonJSTicksThreshold) {
        ++non_js_ticks_;
      } else {
        if (Top::WaitForJSState()) return true;
      }
    }
  }
#endif
  return false;
}


} }  // namespace v8::internal

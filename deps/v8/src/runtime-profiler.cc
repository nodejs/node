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

#include "v8.h"

#include "runtime-profiler.h"

#include "assembler.h"
#include "bootstrapper.h"
#include "code-stubs.h"
#include "compilation-cache.h"
#include "deoptimizer.h"
#include "execution.h"
#include "full-codegen.h"
#include "global-handles.h"
#include "isolate-inl.h"
#include "mark-compact.h"
#include "platform.h"
#include "scopeinfo.h"

namespace v8 {
namespace internal {


// Optimization sampler constants.
static const int kSamplerFrameCount = 2;

// Constants for statistical profiler.
static const int kSamplerFrameWeight[kSamplerFrameCount] = { 2, 1 };

static const int kSamplerTicksBetweenThresholdAdjustment = 32;

static const int kSamplerThresholdInit = 3;
static const int kSamplerThresholdMin = 1;
static const int kSamplerThresholdDelta = 1;

static const int kSamplerThresholdSizeFactorInit = 3;

static const int kSizeLimit = 1500;

// Constants for counter based profiler.

// Number of times a function has to be seen on the stack before it is
// optimized.
static const int kProfilerTicksBeforeOptimization = 2;
// If the function optimization was disabled due to high deoptimization count,
// but the function is hot and has been seen on the stack this number of times,
// then we try to reenable optimization for this function.
static const int kProfilerTicksBeforeReenablingOptimization = 250;
// If a function does not have enough type info (according to
// FLAG_type_info_threshold), but has seen a huge number of ticks,
// optimize it as it is.
static const int kTicksWhenNotEnoughTypeInfo = 100;
// We only have one byte to store the number of ticks.
STATIC_ASSERT(kProfilerTicksBeforeOptimization < 256);
STATIC_ASSERT(kProfilerTicksBeforeReenablingOptimization < 256);
STATIC_ASSERT(kTicksWhenNotEnoughTypeInfo < 256);


// Maximum size in bytes of generated code for a function to be optimized
// the very first time it is seen on the stack.
static const int kMaxSizeEarlyOpt =
    5 * FullCodeGenerator::kBackEdgeDistanceUnit;


RuntimeProfiler::RuntimeProfiler(Isolate* isolate)
    : isolate_(isolate),
      sampler_threshold_(kSamplerThresholdInit),
      sampler_threshold_size_factor_(kSamplerThresholdSizeFactorInit),
      sampler_ticks_until_threshold_adjustment_(
          kSamplerTicksBetweenThresholdAdjustment),
      sampler_window_position_(0),
      any_ic_changed_(false),
      code_generated_(false) {
  ClearSampleBuffer();
}


static void GetICCounts(JSFunction* function,
                        int* ic_with_type_info_count,
                        int* ic_total_count,
                        int* percentage) {
  *ic_total_count = 0;
  *ic_with_type_info_count = 0;
  Object* raw_info =
      function->shared()->code()->type_feedback_info();
  if (raw_info->IsTypeFeedbackInfo()) {
    TypeFeedbackInfo* info = TypeFeedbackInfo::cast(raw_info);
    *ic_with_type_info_count = info->ic_with_type_info_count();
    *ic_total_count = info->ic_total_count();
  }
  *percentage = *ic_total_count > 0
      ? 100 * *ic_with_type_info_count / *ic_total_count
      : 100;
}


void RuntimeProfiler::Optimize(JSFunction* function, const char* reason) {
  ASSERT(function->IsOptimizable());

  if (FLAG_trace_opt) {
    PrintF("[marking ");
    function->PrintName();
    PrintF(" 0x%" V8PRIxPTR, reinterpret_cast<intptr_t>(function->address()));
    PrintF(" for recompilation, reason: %s", reason);
    if (FLAG_type_info_threshold > 0) {
      int typeinfo, total, percentage;
      GetICCounts(function, &typeinfo, &total, &percentage);
      PrintF(", ICs with typeinfo: %d/%d (%d%%)", typeinfo, total, percentage);
    }
    PrintF("]\n");
  }

  if (FLAG_parallel_recompilation && !isolate_->bootstrapper()->IsActive()) {
    ASSERT(!function->IsMarkedForInstallingRecompiledCode());
    ASSERT(!function->IsInRecompileQueue());
    function->MarkForParallelRecompilation();
  } else {
    // The next call to the function will trigger optimization.
    function->MarkForLazyRecompilation();
  }
}


void RuntimeProfiler::AttemptOnStackReplacement(JSFunction* function) {
  // See AlwaysFullCompiler (in compiler.cc) comment on why we need
  // Debug::has_break_points().
  ASSERT(function->IsMarkedForLazyRecompilation() ||
         function->IsMarkedForParallelRecompilation() ||
         function->IsOptimized());
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
    PrintF("[patching back edges in ");
    function->PrintName();
    PrintF(" for on-stack replacement]\n");
  }

  // Get the interrupt stub code object to match against.  We aren't
  // prepared to generate it, but we don't expect to have to.
  Code* interrupt_code = NULL;
  InterruptStub interrupt_stub;
  bool found_code = interrupt_stub.FindCodeInCache(&interrupt_code, isolate_);
  if (found_code) {
    Code* replacement_code =
        isolate_->builtins()->builtin(Builtins::kOnStackReplacement);
    Code* unoptimized_code = shared->code();
    Deoptimizer::PatchInterruptCode(
        unoptimized_code, interrupt_code, replacement_code);
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
      bool fits = FLAG_lookup_sample_by_shared
          ? (function->shared() == JSFunction::cast(sample)->shared())
          : (function == JSFunction::cast(sample));
      if (fits) {
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

  if (FLAG_parallel_recompilation) {
    // Take this as opportunity to process the optimizing compiler thread's
    // output queue so that it does not unnecessarily keep objects alive.
    isolate_->optimizing_compiler_thread()->InstallOptimizedFunctions();
  }

  // Run through the JavaScript frames and collect them. If we already
  // have a sample of the function, we mark it for optimizations
  // (eagerly or lazily).
  JSFunction* samples[kSamplerFrameCount];
  int sample_count = 0;
  int frame_count = 0;
  int frame_count_limit = FLAG_watch_ic_patching ? FLAG_frame_count
                                                 : kSamplerFrameCount;
  for (JavaScriptFrameIterator it(isolate_);
       frame_count++ < frame_count_limit && !it.done();
       it.Advance()) {
    JavaScriptFrame* frame = it.frame();
    JSFunction* function = JSFunction::cast(frame->function());

    if (!FLAG_watch_ic_patching) {
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
    }

    SharedFunctionInfo* shared = function->shared();
    Code* shared_code = shared->code();

    if (shared_code->kind() != Code::FUNCTION) continue;
    if (function->IsInRecompileQueue()) continue;

    // Attempt OSR if we are still running unoptimized code even though the
    // the function has long been marked or even already been optimized.
    if (!frame->is_optimized() &&
        (function->IsMarkedForLazyRecompilation() ||
         function->IsMarkedForParallelRecompilation() ||
         function->IsOptimized())) {
      int nesting = shared_code->allow_osr_at_loop_nesting_level();
      if (nesting < Code::kMaxLoopNestingMarker) {
        int new_nesting = nesting + 1;
        shared_code->set_allow_osr_at_loop_nesting_level(new_nesting);
        AttemptOnStackReplacement(function);
      }
    }

    // Only record top-level code on top of the execution stack and
    // avoid optimizing excessively large scripts since top-level code
    // will be executed only once.
    const int kMaxToplevelSourceSize = 10 * 1024;
    if (shared->is_toplevel() &&
        (frame_count > 1 || shared->SourceSize() > kMaxToplevelSourceSize)) {
      continue;
    }

    // Do not record non-optimizable functions.
    if (shared->optimization_disabled()) {
      if (shared->deopt_count() >= FLAG_max_opt_count) {
        // If optimization was disabled due to many deoptimizations,
        // then check if the function is hot and try to reenable optimization.
        int ticks = shared_code->profiler_ticks();
        if (ticks >= kProfilerTicksBeforeReenablingOptimization) {
          shared_code->set_profiler_ticks(0);
          shared->TryReenableOptimization();
        } else {
          shared_code->set_profiler_ticks(ticks + 1);
        }
      }
      continue;
    }
    if (!function->IsOptimizable()) continue;

    if (FLAG_watch_ic_patching) {
      int ticks = shared_code->profiler_ticks();

      if (ticks >= kProfilerTicksBeforeOptimization) {
        int typeinfo, total, percentage;
        GetICCounts(function, &typeinfo, &total, &percentage);
        if (percentage >= FLAG_type_info_threshold) {
          // If this particular function hasn't had any ICs patched for enough
          // ticks, optimize it now.
          Optimize(function, "hot and stable");
        } else if (ticks >= kTicksWhenNotEnoughTypeInfo) {
          Optimize(function, "not much type info but very hot");
        } else {
          shared_code->set_profiler_ticks(ticks + 1);
          if (FLAG_trace_opt_verbose) {
            PrintF("[not yet optimizing ");
            function->PrintName();
            PrintF(", not enough type info: %d/%d (%d%%)]\n",
                   typeinfo, total, percentage);
          }
        }
      } else if (!any_ic_changed_ &&
                 shared_code->instruction_size() < kMaxSizeEarlyOpt) {
        // If no IC was patched since the last tick and this function is very
        // small, optimistically optimize it now.
        Optimize(function, "small function");
      } else {
        shared_code->set_profiler_ticks(ticks + 1);
      }
    } else {  // !FLAG_watch_ic_patching
      samples[sample_count++] = function;

      int function_size = function->shared()->SourceSize();
      int threshold_size_factor = (function_size > kSizeLimit)
          ? sampler_threshold_size_factor_
          : 1;

      int threshold = sampler_threshold_ * threshold_size_factor;

      if (LookupSample(function) >= threshold) {
        Optimize(function, "sampler window lookup");
      }
    }
  }
  if (FLAG_watch_ic_patching) {
    any_ic_changed_ = false;
  } else {  // !FLAG_watch_ic_patching
    // Add the collected functions as samples. It's important not to do
    // this as part of collecting them because this will interfere with
    // the sample lookup in case of recursive functions.
    for (int i = 0; i < sample_count; i++) {
      AddSample(samples[i], kSamplerFrameWeight[i]);
    }
  }
}


void RuntimeProfiler::SetUp() {
  if (!FLAG_watch_ic_patching) {
    ClearSampleBuffer();
  }
}


void RuntimeProfiler::Reset() {
  if (!FLAG_watch_ic_patching) {
    sampler_threshold_ = kSamplerThresholdInit;
    sampler_threshold_size_factor_ = kSamplerThresholdSizeFactorInit;
    sampler_ticks_until_threshold_adjustment_ =
        kSamplerTicksBetweenThresholdAdjustment;
  }
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


void RuntimeProfiler::RemoveDeadSamples() {
  for (int i = 0; i < kSamplerWindowSize; i++) {
    Object* function = sampler_window_[i];
    if (function != NULL &&
        !Marking::MarkBitFrom(HeapObject::cast(function)).Get()) {
      sampler_window_[i] = NULL;
    }
  }
}


void RuntimeProfiler::UpdateSamplesAfterCompact(ObjectVisitor* visitor) {
  for (int i = 0; i < kSamplerWindowSize; i++) {
    visitor->VisitPointer(&sampler_window_[i]);
  }
}


} }  // namespace v8::internal

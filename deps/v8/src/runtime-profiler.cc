// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime-profiler.h"

#include "src/assembler.h"
#include "src/ast/scopeinfo.h"
#include "src/base/platform/platform.h"
#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/compilation-cache.h"
#include "src/execution.h"
#include "src/frames-inl.h"
#include "src/full-codegen/full-codegen.h"
#include "src/global-handles.h"

namespace v8 {
namespace internal {


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

// Maximum size in bytes of generate code for a function to allow OSR.
static const int kOSRCodeSizeAllowanceBase =
    100 * FullCodeGenerator::kCodeSizeMultiplier;

static const int kOSRCodeSizeAllowancePerTick =
    4 * FullCodeGenerator::kCodeSizeMultiplier;

// Maximum size in bytes of generated code for a function to be optimized
// the very first time it is seen on the stack.
static const int kMaxSizeEarlyOpt =
    5 * FullCodeGenerator::kCodeSizeMultiplier;


RuntimeProfiler::RuntimeProfiler(Isolate* isolate)
    : isolate_(isolate),
      any_ic_changed_(false) {
}


static void GetICCounts(SharedFunctionInfo* shared,
                        int* ic_with_type_info_count, int* ic_generic_count,
                        int* ic_total_count, int* type_info_percentage,
                        int* generic_percentage) {
  *ic_total_count = 0;
  *ic_generic_count = 0;
  *ic_with_type_info_count = 0;
  if (shared->code()->kind() == Code::FUNCTION) {
    Code* shared_code = shared->code();
    Object* raw_info = shared_code->type_feedback_info();
    if (raw_info->IsTypeFeedbackInfo()) {
      TypeFeedbackInfo* info = TypeFeedbackInfo::cast(raw_info);
      *ic_with_type_info_count = info->ic_with_type_info_count();
      *ic_generic_count = info->ic_generic_count();
      *ic_total_count = info->ic_total_count();
    }
  }

  // Harvest vector-ics as well
  TypeFeedbackVector* vector = shared->feedback_vector();
  int with = 0, gen = 0;
  vector->ComputeCounts(&with, &gen);
  *ic_with_type_info_count += with;
  *ic_generic_count += gen;

  if (*ic_total_count > 0) {
    *type_info_percentage = 100 * *ic_with_type_info_count / *ic_total_count;
    *generic_percentage = 100 * *ic_generic_count / *ic_total_count;
  } else {
    *type_info_percentage = 100;  // Compared against lower bound.
    *generic_percentage = 0;      // Compared against upper bound.
  }
}


void RuntimeProfiler::Optimize(JSFunction* function, const char* reason) {
  if (FLAG_trace_opt && function->PassesFilter(FLAG_hydrogen_filter)) {
    PrintF("[marking ");
    function->ShortPrint();
    PrintF(" for recompilation, reason: %s", reason);
    if (FLAG_type_info_threshold > 0) {
      int typeinfo, generic, total, type_percentage, generic_percentage;
      GetICCounts(function->shared(), &typeinfo, &generic, &total,
                  &type_percentage, &generic_percentage);
      PrintF(", ICs with typeinfo: %d/%d (%d%%)", typeinfo, total,
             type_percentage);
      PrintF(", generic ICs: %d/%d (%d%%)", generic, total, generic_percentage);
    }
    PrintF("]\n");
  }

  function->AttemptConcurrentOptimization();
}


void RuntimeProfiler::AttemptOnStackReplacement(JSFunction* function,
                                                int loop_nesting_levels) {
  SharedFunctionInfo* shared = function->shared();
  if (!FLAG_use_osr || function->shared()->IsBuiltin()) {
    return;
  }

  // If the code is not optimizable, don't try OSR.
  if (shared->optimization_disabled()) return;

  // We are not prepared to do OSR for a function that already has an
  // allocated arguments object.  The optimized code would bypass it for
  // arguments accesses, which is unsound.  Don't try OSR.
  if (shared->uses_arguments()) return;

  // We're using on-stack replacement: patch the unoptimized code so that
  // any back edge in any unoptimized frame will trigger on-stack
  // replacement for that frame.
  if (FLAG_trace_osr) {
    PrintF("[OSR - patching back edges in ");
    function->PrintName();
    PrintF("]\n");
  }

  for (int i = 0; i < loop_nesting_levels; i++) {
    BackEdgeTable::Patch(isolate_, shared->code());
  }
}

void RuntimeProfiler::MaybeOptimizeFullCodegen(JSFunction* function,
                                               int frame_count,
                                               bool frame_optimized) {
  SharedFunctionInfo* shared = function->shared();
  Code* shared_code = shared->code();
  if (shared_code->kind() != Code::FUNCTION) return;
  if (function->IsInOptimizationQueue()) return;

  if (FLAG_always_osr) {
    AttemptOnStackReplacement(function, Code::kMaxLoopNestingMarker);
    // Fall through and do a normal optimized compile as well.
  } else if (!frame_optimized &&
             (function->IsMarkedForOptimization() ||
              function->IsMarkedForConcurrentOptimization() ||
              function->IsOptimized())) {
    // Attempt OSR if we are still running unoptimized code even though the
    // the function has long been marked or even already been optimized.
    int ticks = shared_code->profiler_ticks();
    int64_t allowance =
        kOSRCodeSizeAllowanceBase +
        static_cast<int64_t>(ticks) * kOSRCodeSizeAllowancePerTick;
    if (shared_code->CodeSize() > allowance &&
        ticks < Code::ProfilerTicksField::kMax) {
      shared_code->set_profiler_ticks(ticks + 1);
    } else {
      AttemptOnStackReplacement(function);
    }
    return;
  }

  // Only record top-level code on top of the execution stack and
  // avoid optimizing excessively large scripts since top-level code
  // will be executed only once.
  const int kMaxToplevelSourceSize = 10 * 1024;
  if (shared->is_toplevel() &&
      (frame_count > 1 || shared->SourceSize() > kMaxToplevelSourceSize)) {
    return;
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
    return;
  }
  if (function->IsOptimized()) return;

  int ticks = shared_code->profiler_ticks();

  if (ticks >= kProfilerTicksBeforeOptimization) {
    int typeinfo, generic, total, type_percentage, generic_percentage;
    GetICCounts(shared, &typeinfo, &generic, &total, &type_percentage,
                &generic_percentage);
    if (type_percentage >= FLAG_type_info_threshold &&
        generic_percentage <= FLAG_generic_ic_threshold) {
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
        PrintF(", not enough type info: %d/%d (%d%%)]\n", typeinfo, total,
               type_percentage);
      }
    }
  } else if (!any_ic_changed_ &&
             shared_code->instruction_size() < kMaxSizeEarlyOpt) {
    // If no IC was patched since the last tick and this function is very
    // small, optimistically optimize it now.
    int typeinfo, generic, total, type_percentage, generic_percentage;
    GetICCounts(shared, &typeinfo, &generic, &total, &type_percentage,
                &generic_percentage);
    if (type_percentage >= FLAG_type_info_threshold &&
        generic_percentage <= FLAG_generic_ic_threshold) {
      Optimize(function, "small function");
    } else {
      shared_code->set_profiler_ticks(ticks + 1);
    }
  } else {
    shared_code->set_profiler_ticks(ticks + 1);
  }
}

void RuntimeProfiler::MaybeOptimizeIgnition(JSFunction* function,
                                            bool frame_optimized) {
  if (function->IsInOptimizationQueue()) return;

  SharedFunctionInfo* shared = function->shared();
  int ticks = shared->profiler_ticks();

  // TODO(rmcilroy): Also ensure we only OSR top-level code if it is smaller
  // than kMaxToplevelSourceSize.
  // TODO(rmcilroy): Consider whether we should optimize small functions when
  // they are first seen on the stack (e.g., kMaxSizeEarlyOpt).

  if (!frame_optimized && (function->IsMarkedForOptimization() ||
                           function->IsMarkedForConcurrentOptimization() ||
                           function->IsOptimized())) {
    // TODO(rmcilroy): Support OSR in these cases.

    return;
  }

  // Do not optimize non-optimizable functions.
  if (shared->optimization_disabled()) {
    if (shared->deopt_count() >= FLAG_max_opt_count) {
      // If optimization was disabled due to many deoptimizations,
      // then check if the function is hot and try to reenable optimization.
      if (ticks >= kProfilerTicksBeforeReenablingOptimization) {
        shared->set_profiler_ticks(0);
        shared->TryReenableOptimization();
      }
    }
    return;
  }

  if (function->IsOptimized()) return;

  if (ticks >= kProfilerTicksBeforeOptimization) {
    int typeinfo, generic, total, type_percentage, generic_percentage;
    GetICCounts(shared, &typeinfo, &generic, &total, &type_percentage,
                &generic_percentage);
    if (type_percentage >= FLAG_type_info_threshold &&
        generic_percentage <= FLAG_generic_ic_threshold) {
      // If this particular function hasn't had any ICs patched for enough
      // ticks, optimize it now.
      Optimize(function, "hot and stable");
    } else if (ticks >= kTicksWhenNotEnoughTypeInfo) {
      Optimize(function, "not much type info but very hot");
    } else {
      if (FLAG_trace_opt_verbose) {
        PrintF("[not yet optimizing ");
        function->PrintName();
        PrintF(", not enough type info: %d/%d (%d%%)]\n", typeinfo, total,
               type_percentage);
      }
    }
  }
}

void RuntimeProfiler::MarkCandidatesForOptimization() {
  HandleScope scope(isolate_);

  if (!isolate_->use_crankshaft()) return;

  DisallowHeapAllocation no_gc;

  // Run through the JavaScript frames and collect them. If we already
  // have a sample of the function, we mark it for optimizations
  // (eagerly or lazily).
  int frame_count = 0;
  int frame_count_limit = FLAG_frame_count;
  for (JavaScriptFrameIterator it(isolate_);
       frame_count++ < frame_count_limit && !it.done();
       it.Advance()) {
    JavaScriptFrame* frame = it.frame();
    JSFunction* function = frame->function();

    List<JSFunction*> functions(4);
    frame->GetFunctions(&functions);
    for (int i = functions.length(); --i >= 0; ) {
      SharedFunctionInfo* shared_function_info = functions[i]->shared();
      int ticks = shared_function_info->profiler_ticks();
      if (ticks < Smi::kMaxValue) {
        shared_function_info->set_profiler_ticks(ticks + 1);
      }
    }

    if (FLAG_ignition) {
      MaybeOptimizeIgnition(function, frame->is_optimized());
    } else {
      MaybeOptimizeFullCodegen(function, frame_count, frame->is_optimized());
    }
  }
  any_ic_changed_ = false;
}


}  // namespace internal
}  // namespace v8

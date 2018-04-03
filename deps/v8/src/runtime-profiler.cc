// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime-profiler.h"

#include "src/assembler.h"
#include "src/base/platform/platform.h"
#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/compilation-cache.h"
#include "src/compiler.h"
#include "src/execution.h"
#include "src/frames-inl.h"
#include "src/global-handles.h"
#include "src/interpreter/interpreter.h"

namespace v8 {
namespace internal {

// Number of times a function has to be seen on the stack before it is
// optimized.
static const int kProfilerTicksBeforeOptimization = 2;

// The number of ticks required for optimizing a function increases with
// the size of the bytecode. This is in addition to the
// kProfilerTicksBeforeOptimization required for any function.
static const int kBytecodeSizeAllowancePerTick = 1200;

// Maximum size in bytes of generate code for a function to allow OSR.
static const int kOSRBytecodeSizeAllowanceBase = 180;

static const int kOSRBytecodeSizeAllowancePerTick = 48;

// Maximum size in bytes of generated code for a function to be optimized
// the very first time it is seen on the stack.
static const int kMaxBytecodeSizeForEarlyOpt = 90;

// Certain functions are simply too big to be worth optimizing.
static const int kMaxBytecodeSizeForOpt = 60 * KB;

#define OPTIMIZATION_REASON_LIST(V)                            \
  V(DoNotOptimize, "do not optimize")                          \
  V(HotAndStable, "hot and stable")                            \
  V(SmallFunction, "small function")

enum class OptimizationReason : uint8_t {
#define OPTIMIZATION_REASON_CONSTANTS(Constant, message) k##Constant,
  OPTIMIZATION_REASON_LIST(OPTIMIZATION_REASON_CONSTANTS)
#undef OPTIMIZATION_REASON_CONSTANTS
};

char const* OptimizationReasonToString(OptimizationReason reason) {
  static char const* reasons[] = {
#define OPTIMIZATION_REASON_TEXTS(Constant, message) message,
      OPTIMIZATION_REASON_LIST(OPTIMIZATION_REASON_TEXTS)
#undef OPTIMIZATION_REASON_TEXTS
  };
  size_t const index = static_cast<size_t>(reason);
  DCHECK_LT(index, arraysize(reasons));
  return reasons[index];
}

std::ostream& operator<<(std::ostream& os, OptimizationReason reason) {
  return os << OptimizationReasonToString(reason);
}

RuntimeProfiler::RuntimeProfiler(Isolate* isolate)
    : isolate_(isolate),
      any_ic_changed_(false) {
}

static void GetICCounts(JSFunction* function, int* ic_with_type_info_count,
                        int* ic_generic_count, int* ic_total_count,
                        int* type_info_percentage, int* generic_percentage) {
  // Harvest vector-ics.
  FeedbackVector* vector = function->feedback_vector();
  vector->ComputeCounts(ic_with_type_info_count, ic_generic_count,
                        ic_total_count);

  if (*ic_total_count > 0) {
    *type_info_percentage = 100 * *ic_with_type_info_count / *ic_total_count;
    *generic_percentage = 100 * *ic_generic_count / *ic_total_count;
  } else {
    *type_info_percentage = 100;  // Compared against lower bound.
    *generic_percentage = 0;      // Compared against upper bound.
  }
}

static void TraceRecompile(JSFunction* function, const char* reason,
                           const char* type) {
  if (FLAG_trace_opt) {
    PrintF("[marking ");
    function->ShortPrint();
    PrintF(" for %s recompilation, reason: %s", type, reason);
    if (FLAG_type_info_threshold > 0) {
      int typeinfo, generic, total, type_percentage, generic_percentage;
      GetICCounts(function, &typeinfo, &generic, &total, &type_percentage,
                  &generic_percentage);
      PrintF(", ICs with typeinfo: %d/%d (%d%%)", typeinfo, total,
             type_percentage);
      PrintF(", generic ICs: %d/%d (%d%%)", generic, total, generic_percentage);
    }
    PrintF("]\n");
  }
}

void RuntimeProfiler::Optimize(JSFunction* function,
                               OptimizationReason reason) {
  DCHECK_NE(reason, OptimizationReason::kDoNotOptimize);
  TraceRecompile(function, OptimizationReasonToString(reason), "optimized");
  function->MarkForOptimization(ConcurrencyMode::kConcurrent);
}

void RuntimeProfiler::AttemptOnStackReplacement(JavaScriptFrame* frame,
                                                int loop_nesting_levels) {
  JSFunction* function = frame->function();
  SharedFunctionInfo* shared = function->shared();
  if (!FLAG_use_osr || !function->shared()->IsUserJavaScript()) {
    return;
  }

  // If the code is not optimizable, don't try OSR.
  if (shared->optimization_disabled()) return;

  // We're using on-stack replacement: Store new loop nesting level in
  // BytecodeArray header so that certain back edges in any interpreter frame
  // for this bytecode will trigger on-stack replacement for that frame.
  if (FLAG_trace_osr) {
    PrintF("[OSR - arming back edges in ");
    function->PrintName();
    PrintF("]\n");
  }

  DCHECK_EQ(StackFrame::INTERPRETED, frame->type());
  DCHECK(shared->HasBytecodeArray());
  int level = shared->bytecode_array()->osr_loop_nesting_level();
  shared->bytecode_array()->set_osr_loop_nesting_level(
      Min(level + loop_nesting_levels, AbstractCode::kMaxLoopNestingMarker));
}

void RuntimeProfiler::MaybeOptimize(JSFunction* function,
                                    JavaScriptFrame* frame) {
  if (function->IsInOptimizationQueue()) {
    if (FLAG_trace_opt_verbose) {
      PrintF("[function ");
      function->PrintName();
      PrintF(" is already in optimization queue]\n");
    }
    return;
  }

  if (FLAG_always_osr) {
    AttemptOnStackReplacement(frame, AbstractCode::kMaxLoopNestingMarker);
    // Fall through and do a normal optimized compile as well.
  } else if (MaybeOSR(function, frame)) {
    return;
  }

  if (function->shared()->optimization_disabled()) return;

  if (frame->is_optimized()) return;

  OptimizationReason reason = ShouldOptimize(function, frame);

  if (reason != OptimizationReason::kDoNotOptimize) {
    Optimize(function, reason);
  }
}

bool RuntimeProfiler::MaybeOSR(JSFunction* function, JavaScriptFrame* frame) {
  SharedFunctionInfo* shared = function->shared();
  int ticks = function->feedback_vector()->profiler_ticks();

  // TODO(rmcilroy): Also ensure we only OSR top-level code if it is smaller
  // than kMaxToplevelSourceSize.

  if (!frame->is_optimized() &&
      (function->IsMarkedForOptimization() ||
       function->IsMarkedForConcurrentOptimization() ||
       function->HasOptimizedCode())) {
    // Attempt OSR if we are still running interpreted code even though the
    // the function has long been marked or even already been optimized.
    int64_t allowance =
        kOSRBytecodeSizeAllowanceBase +
        static_cast<int64_t>(ticks) * kOSRBytecodeSizeAllowancePerTick;
    if (shared->bytecode_array()->length() <= allowance) {
      AttemptOnStackReplacement(frame);
    }
    return true;
  }
  return false;
}

OptimizationReason RuntimeProfiler::ShouldOptimize(JSFunction* function,
                                                   JavaScriptFrame* frame) {
  SharedFunctionInfo* shared = function->shared();
  int ticks = function->feedback_vector()->profiler_ticks();

  if (shared->bytecode_array()->length() > kMaxBytecodeSizeForOpt) {
    return OptimizationReason::kDoNotOptimize;
  }

  int ticks_for_optimization =
      kProfilerTicksBeforeOptimization +
      (shared->bytecode_array()->length() / kBytecodeSizeAllowancePerTick);
  if (ticks >= ticks_for_optimization) {
    return OptimizationReason::kHotAndStable;
  } else if (!any_ic_changed_ &&
             shared->bytecode_array()->length() < kMaxBytecodeSizeForEarlyOpt) {
    // If no IC was patched since the last tick and this function is very
    // small, optimistically optimize it now.
    return OptimizationReason::kSmallFunction;
  } else if (FLAG_trace_opt_verbose) {
    PrintF("[not yet optimizing ");
    function->PrintName();
    PrintF(", not enough ticks: %d/%d and ", ticks,
           kProfilerTicksBeforeOptimization);
    if (any_ic_changed_) {
      PrintF("ICs changed]\n");
    } else {
      PrintF(" too large for small function optimization: %d/%d]\n",
             shared->bytecode_array()->length(), kMaxBytecodeSizeForEarlyOpt);
    }
  }
  return OptimizationReason::kDoNotOptimize;
}

void RuntimeProfiler::MarkCandidatesForOptimization() {
  HandleScope scope(isolate_);

  if (!isolate_->use_optimizer()) return;

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
    if (frame->is_optimized()) continue;

    JSFunction* function = frame->function();
    DCHECK(function->shared()->is_compiled());
    if (!function->shared()->IsInterpreted()) continue;

    MaybeOptimize(function, frame);

    // TODO(leszeks): Move this increment to before the maybe optimize checks,
    // and update the tests to assume the increment has already happened.
    int ticks = function->feedback_vector()->profiler_ticks();
    if (ticks < Smi::kMaxValue) {
      function->feedback_vector()->set_profiler_ticks(ticks + 1);
    }
  }
  any_ic_changed_ = false;
}

}  // namespace internal
}  // namespace v8

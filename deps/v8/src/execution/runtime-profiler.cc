// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/runtime-profiler.h"

#include "src/base/platform/platform.h"
#include "src/codegen/assembler.h"
#include "src/codegen/compilation-cache.h"
#include "src/codegen/compiler.h"
#include "src/codegen/pending-optimization-table.h"
#include "src/diagnostics/code-tracer.h"
#include "src/execution/execution.h"
#include "src/execution/frames-inl.h"
#include "src/handles/global-handles.h"
#include "src/init/bootstrapper.h"
#include "src/interpreter/interpreter.h"
#include "src/tracing/trace-event.h"

namespace v8 {
namespace internal {

// Number of times a function has to be seen on the stack before it is
// optimized.
static const int kProfilerTicksBeforeOptimization = 3;

// The number of ticks required for optimizing a function increases with
// the size of the bytecode. This is in addition to the
// kProfilerTicksBeforeOptimization required for any function.
static const int kBytecodeSizeAllowancePerTick = 1100;

// Maximum size in bytes of generate code for a function to allow OSR.
static const int kOSRBytecodeSizeAllowanceBase = 119;

static const int kOSRBytecodeSizeAllowancePerTick = 44;

// Maximum size in bytes of generated code for a function to be optimized
// the very first time it is seen on the stack.
static const int kMaxBytecodeSizeForEarlyOpt = 81;

// Number of times a function has to be seen on the stack before it is
// OSRed in TurboProp
// This value is chosen so TurboProp OSRs at similar time as TurboFan. The
// current interrupt budger of TurboFan is approximately 10 times that of
// TurboProp and we wait for 4 ticks (3 for marking for optimization and an
// additional tick to mark it for OSR) and hence this is set to 4 * 10.
// TODO(mythria): This value should be based on
// FLAG_ticks_scale_factor_for_top_tier.
static const int kProfilerTicksForTurboPropOSR = 4 * 10;

// These are used to decide when we tiering up to Turboprop.
// The number of ticks required for tiering up to Turboprop is based on how
// "soon" the function becomes hot. We use kMidTierGlobalTicksScaleFactor to
// scale the difference in global ticks since the last time a function saw a
// tick. The scaled difference is used to to increase the number of ticks
// required for tiering up to Turboprop.
static const int kMidTierGlobalTicksScaleFactor = 100;

// This is used to limit the number of additional ticks that the
// kMidTierGlobalTicksScaleFactor can increase threshold for mid-tier tier
// tierup.
static const int kMaxAdditionalMidTierGlobalTicks = 10;

#define OPTIMIZATION_REASON_LIST(V)   \
  V(DoNotOptimize, "do not optimize") \
  V(HotAndStable, "hot and stable")   \
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

#undef OPTIMIZATION_REASON_LIST

std::ostream& operator<<(std::ostream& os, OptimizationReason reason) {
  return os << OptimizationReasonToString(reason);
}

namespace {

void TraceInOptimizationQueue(JSFunction function) {
  if (FLAG_trace_opt_verbose) {
    PrintF("[function ");
    function.PrintName();
    PrintF(" is already in optimization queue]\n");
  }
}

void TraceHeuristicOptimizationDisallowed(JSFunction function) {
  if (FLAG_trace_opt_verbose) {
    PrintF("[function ");
    function.PrintName();
    PrintF(" has been marked manually for optimization]\n");
  }
}

void TraceRecompile(JSFunction function, OptimizationReason reason,
                    CodeKind code_kind, Isolate* isolate) {
  if (FLAG_trace_opt) {
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintF(scope.file(), "[marking ");
    function.ShortPrint(scope.file());
    PrintF(scope.file(), " for optimized recompilation, reason: %s",
           OptimizationReasonToString(reason));
    PrintF(scope.file(), "]\n");
  }
}

}  // namespace

RuntimeProfiler::RuntimeProfiler(Isolate* isolate)
    : isolate_(isolate), any_ic_changed_(false), current_global_ticks_(0) {}

void RuntimeProfiler::Optimize(JSFunction function, OptimizationReason reason,
                               CodeKind code_kind) {
  DCHECK_NE(reason, OptimizationReason::kDoNotOptimize);
  TraceRecompile(function, reason, code_kind, isolate_);
  function.MarkForOptimization(ConcurrencyMode::kConcurrent);
}

void RuntimeProfiler::AttemptOnStackReplacement(UnoptimizedFrame* frame,
                                                int loop_nesting_levels) {
  JSFunction function = frame->function();
  SharedFunctionInfo shared = function.shared();
  if (!FLAG_use_osr || !shared.IsUserJavaScript()) {
    return;
  }

  // If the code is not optimizable, don't try OSR.
  if (shared.optimization_disabled()) return;

  // We're using on-stack replacement: Store new loop nesting level in
  // BytecodeArray header so that certain back edges in any interpreter frame
  // for this bytecode will trigger on-stack replacement for that frame.
  if (FLAG_trace_osr) {
    CodeTracer::Scope scope(isolate_->GetCodeTracer());
    PrintF(scope.file(), "[OSR - arming back edges in ");
    function.PrintName(scope.file());
    PrintF(scope.file(), "]\n");
  }

  DCHECK(frame->is_unoptimized());
  int level = frame->GetBytecodeArray().osr_loop_nesting_level();
  frame->GetBytecodeArray().set_osr_loop_nesting_level(std::min(
      {level + loop_nesting_levels, AbstractCode::kMaxLoopNestingMarker}));
}

void RuntimeProfiler::MaybeOptimizeFrame(JSFunction function,
                                         JavaScriptFrame* frame,
                                         CodeKind code_kind) {
  if (function.IsInOptimizationQueue()) {
    TraceInOptimizationQueue(function);
    return;
  }

  if (FLAG_testing_d8_test_runner &&
      !PendingOptimizationTable::IsHeuristicOptimizationAllowed(isolate_,
                                                                function)) {
    TraceHeuristicOptimizationDisallowed(function);
    return;
  }

  if (function.shared().optimization_disabled()) return;

  // Note: We currently do not trigger OSR compilation from TP code.
  if (frame->is_unoptimized()) {
    if (FLAG_always_osr) {
      AttemptOnStackReplacement(UnoptimizedFrame::cast(frame),
                                AbstractCode::kMaxLoopNestingMarker);
      // Fall through and do a normal optimized compile as well.
    } else if (MaybeOSR(function, UnoptimizedFrame::cast(frame))) {
      return;
    }
  }

  OptimizationReason reason =
      ShouldOptimize(function, function.shared().GetBytecodeArray(isolate_));

  if (reason != OptimizationReason::kDoNotOptimize) {
    Optimize(function, reason, code_kind);
  }
  function.feedback_vector()
      .set_global_ticks_at_last_runtime_profiler_interrupt(
          current_global_ticks_);
}

bool RuntimeProfiler::MaybeOSR(JSFunction function, UnoptimizedFrame* frame) {
  int ticks = function.feedback_vector().profiler_ticks();
  // TODO(rmcilroy): Also ensure we only OSR top-level code if it is smaller
  // than kMaxToplevelSourceSize.

  // Turboprop optimizes quite early. So don't attempt to OSR if the loop isn't
  // hot enough.
  // TODO(mythria): We should decide when to OSR based on number of ticks
  // instead of checking if it has been marked for optimization. That will allow
  // us to unify OSR decisions from different tiers and we can remove this
  // special case here for Turboprop. If we do that also remove the code to
  // reset the marker in Runtime_CompileForOnStackReplacement.
  if (FLAG_turboprop && ticks < kProfilerTicksForTurboPropOSR) {
    return false;
  }

  if (function.IsMarkedForOptimization() ||
      function.IsMarkedForConcurrentOptimization() ||
      function.HasAvailableOptimizedCode()) {
    // Attempt OSR if we are still running interpreted code even though the
    // the function has long been marked or even already been optimized.
    // OSR should happen roughly at the same with or without FLAG_turboprop.
    // Turboprop has much lower interrupt budget so scale the ticks accordingly.
    int scale_factor =
        FLAG_turboprop ? FLAG_interrupt_budget_scale_factor_for_top_tier : 1;
    int64_t scaled_ticks = static_cast<int64_t>(ticks) / scale_factor;
    int64_t allowance = kOSRBytecodeSizeAllowanceBase +
                        scaled_ticks * kOSRBytecodeSizeAllowancePerTick;
    if (function.shared().GetBytecodeArray(isolate_).length() <= allowance) {
      AttemptOnStackReplacement(frame);
    }
    return true;
  }
  return false;
}

namespace {

bool ShouldOptimizeAsSmallFunction(int bytecode_size, int ticks,
                                   bool any_ic_changed,
                                   bool active_tier_is_turboprop) {
  if (FLAG_turboprop) return false;
  if (any_ic_changed || bytecode_size >= kMaxBytecodeSizeForEarlyOpt)
    return false;
  return true;
}

}  // namespace

OptimizationReason RuntimeProfiler::ShouldOptimize(JSFunction function,
                                                   BytecodeArray bytecode) {
  if (function.ActiveTierIsTurbofan()) {
    return OptimizationReason::kDoNotOptimize;
  }
  if (V8_UNLIKELY(FLAG_turboprop) && function.ActiveTierIsToptierTurboprop()) {
    return OptimizationReason::kDoNotOptimize;
  }
  int ticks = function.feedback_vector().profiler_ticks();
  bool active_tier_is_turboprop = function.ActiveTierIsMidtierTurboprop();
  int ticks_for_optimization =
      kProfilerTicksBeforeOptimization +
      (bytecode.length() / kBytecodeSizeAllowancePerTick);
  if (FLAG_turboprop && !active_tier_is_turboprop) {
    DCHECK_EQ(function.NextTier(), CodeKind::TURBOPROP);
    int global_ticks_diff =
        (current_global_ticks_ -
         function.feedback_vector()
             .global_ticks_at_last_runtime_profiler_interrupt());
    ticks_for_optimization =
        ticks_for_optimization +
        std::min(global_ticks_diff / kMidTierGlobalTicksScaleFactor,
                 kMaxAdditionalMidTierGlobalTicks);
  }
  if (ticks >= ticks_for_optimization) {
    return OptimizationReason::kHotAndStable;
  } else if (ShouldOptimizeAsSmallFunction(bytecode.length(), ticks,
                                           any_ic_changed_,
                                           active_tier_is_turboprop)) {
    // If no IC was patched since the last tick and this function is very
    // small, optimistically optimize it now.
    return OptimizationReason::kSmallFunction;
  } else if (FLAG_trace_opt_verbose) {
    PrintF("[not yet optimizing ");
    function.PrintName();
    PrintF(", not enough ticks: %d/%d and ", ticks, ticks_for_optimization);
    if (any_ic_changed_) {
      PrintF("ICs changed]\n");
    } else {
      PrintF(" too large for small function optimization: %d/%d]\n",
             bytecode.length(), kMaxBytecodeSizeForEarlyOpt);
    }
  }
  return OptimizationReason::kDoNotOptimize;
}

RuntimeProfiler::MarkCandidatesForOptimizationScope::
    MarkCandidatesForOptimizationScope(RuntimeProfiler* profiler)
    : handle_scope_(profiler->isolate_), profiler_(profiler) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.MarkCandidatesForOptimization");
  if (profiler_->current_global_ticks_ <
      FeedbackVector::GlobalTicksAtLastRuntimeProfilerInterruptBits::kMax - 1) {
    profiler_->current_global_ticks_ += 1;
  }
}

RuntimeProfiler::MarkCandidatesForOptimizationScope::
    ~MarkCandidatesForOptimizationScope() {
  profiler_->any_ic_changed_ = false;
}

void RuntimeProfiler::MarkCandidatesForOptimization(JavaScriptFrame* frame) {
  if (!isolate_->use_optimizer()) return;
  MarkCandidatesForOptimizationScope scope(this);

  JSFunction function = frame->function();
  CodeKind code_kind = function.GetActiveTier();

  DCHECK(function.shared().is_compiled());
  DCHECK(function.shared().IsInterpreted());

  DCHECK_IMPLIES(CodeKindIsOptimizedJSFunction(code_kind),
                 function.has_feedback_vector());
  if (!function.has_feedback_vector()) return;

  function.feedback_vector().SaturatingIncrementProfilerTicks();
  MaybeOptimizeFrame(function, frame, code_kind);
}

void RuntimeProfiler::MarkCandidatesForOptimizationFromBytecode() {
  JavaScriptFrameIterator it(isolate_);
  DCHECK(it.frame()->is_unoptimized());
  MarkCandidatesForOptimization(it.frame());
}

void RuntimeProfiler::MarkCandidatesForOptimizationFromCode() {
  JavaScriptFrameIterator it(isolate_);
  DCHECK(it.frame()->is_optimized());
  MarkCandidatesForOptimization(it.frame());
}

}  // namespace internal
}  // namespace v8

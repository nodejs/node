// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/tiering-manager.h"

#include "src/base/platform/platform.h"
#include "src/baseline/baseline-batch-compiler.h"
#include "src/baseline/baseline.h"
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
#include "src/objects/code.h"
#include "src/tracing/trace-event.h"

namespace v8 {
namespace internal {

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

class OptimizationDecision {
 public:
  static constexpr OptimizationDecision Maglev() {
    // TODO(v8:7700): Consider using another reason here.
    return {OptimizationReason::kHotAndStable, CodeKind::MAGLEV,
            ConcurrencyMode::kConcurrent};
  }
  static constexpr OptimizationDecision TurbofanHotAndStable() {
    return {OptimizationReason::kHotAndStable, CodeKind::TURBOFAN,
            ConcurrencyMode::kConcurrent};
  }
  static constexpr OptimizationDecision TurbofanSmallFunction() {
    return {OptimizationReason::kSmallFunction, CodeKind::TURBOFAN,
            ConcurrencyMode::kConcurrent};
  }
  static constexpr OptimizationDecision DoNotOptimize() {
    return {OptimizationReason::kDoNotOptimize,
            // These values don't matter but we have to pass something.
            CodeKind::TURBOFAN, ConcurrencyMode::kConcurrent};
  }

  constexpr bool should_optimize() const {
    return optimization_reason != OptimizationReason::kDoNotOptimize;
  }

  OptimizationReason optimization_reason;
  CodeKind code_kind;
  ConcurrencyMode concurrency_mode;

 private:
  OptimizationDecision() = default;
  constexpr OptimizationDecision(OptimizationReason optimization_reason,
                                 CodeKind code_kind,
                                 ConcurrencyMode concurrency_mode)
      : optimization_reason(optimization_reason),
        code_kind(code_kind),
        concurrency_mode(concurrency_mode) {}
};
// Since we pass by value:
static_assert(sizeof(OptimizationDecision) <= kInt32Size);

namespace {

void TraceInOptimizationQueue(JSFunction function) {
  if (v8_flags.trace_opt_verbose) {
    PrintF("[not marking function %s for optimization: already queued]\n",
           function.DebugNameCStr().get());
  }
}

void TraceHeuristicOptimizationDisallowed(JSFunction function) {
  if (v8_flags.trace_opt_verbose) {
    PrintF(
        "[not marking function %s for optimization: marked with "
        "%%PrepareFunctionForOptimization for manual optimization]\n",
        function.DebugNameCStr().get());
  }
}

void TraceRecompile(Isolate* isolate, JSFunction function,
                    OptimizationDecision d) {
  if (v8_flags.trace_opt) {
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintF(scope.file(), "[marking ");
    function.ShortPrint(scope.file());
    PrintF(scope.file(), " for optimization to %s, %s, reason: %s",
           CodeKindToString(d.code_kind), ToString(d.concurrency_mode),
           OptimizationReasonToString(d.optimization_reason));
    PrintF(scope.file(), "]\n");
  }
}

}  // namespace

void TraceManualRecompile(JSFunction function, CodeKind code_kind,
                          ConcurrencyMode concurrency_mode) {
  if (v8_flags.trace_opt) {
    PrintF("[manually marking ");
    function.ShortPrint();
    PrintF(" for optimization to %s, %s]\n", CodeKindToString(code_kind),
           ToString(concurrency_mode));
  }
}

void TieringManager::Optimize(JSFunction function, OptimizationDecision d) {
  DCHECK(d.should_optimize());
  TraceRecompile(isolate_, function, d);
  function.MarkForOptimization(isolate_, d.code_kind, d.concurrency_mode);
}

namespace {

bool TiersUpToMaglev(CodeKind code_kind) {
  // TODO(v8:7700): Flip the UNLIKELY when appropriate.
  return V8_UNLIKELY(v8_flags.maglev) &&
         CodeKindIsUnoptimizedJSFunction(code_kind);
}

bool TiersUpToMaglev(base::Optional<CodeKind> code_kind) {
  return code_kind.has_value() && TiersUpToMaglev(code_kind.value());
}

int InterruptBudgetFor(base::Optional<CodeKind> code_kind) {
  return TiersUpToMaglev(code_kind) ? v8_flags.interrupt_budget_for_maglev
                                    : v8_flags.interrupt_budget;
}

}  // namespace

// static
int TieringManager::InterruptBudgetFor(Isolate* isolate, JSFunction function) {
  if (function.has_feedback_vector()) {
    return ::i::InterruptBudgetFor(function.GetActiveTier());
  }

  DCHECK(!function.has_feedback_vector());
  DCHECK(function.shared().is_compiled());
  return function.shared().GetBytecodeArray(isolate).length() *
         v8_flags.interrupt_budget_factor_for_feedback_allocation;
}

// static
int TieringManager::InitialInterruptBudget() {
  return V8_LIKELY(v8_flags.lazy_feedback_allocation)
             ? v8_flags.interrupt_budget_for_feedback_allocation
             : v8_flags.interrupt_budget;
}

namespace {

bool SmallEnoughForOSR(Isolate* isolate, JSFunction function,
                       CodeKind code_kind) {
  // "The answer to life the universe and everything.. 42? Or was it 44?"
  //
  // Note the OSR allowance's origin is somewhat accidental - with the advent
  // of Ignition it started at 48 and through several rounds of micro-tuning
  // ended up at 42. See
  // https://chromium-review.googlesource.com/649149.
  //
  // The allowance was originally chosen based on the Ignition-to-Turbofan
  // interrupt budget. In the presence of multiple tiers and multiple budgets
  // (which control how often ticks are incremented), it must be scaled to the
  // currently active budget to somewhat preserve old behavior.
  //
  // TODO(all): Since the origins of this constant are so arbitrary, this is
  // worth another re-evaluation. For now, we stick with 44 to preserve
  // behavior for comparability, but feel free to change this in the future.
  static const int kOSRBytecodeSizeAllowanceBase = 119;
  static const int kOSRBytecodeSizeAllowancePerTick = 44;
  const double scale_factor_for_active_tier =
      InterruptBudgetFor(code_kind) /
      static_cast<double>(v8_flags.interrupt_budget);

  const double raw_limit = kOSRBytecodeSizeAllowanceBase +
                           scale_factor_for_active_tier *
                               kOSRBytecodeSizeAllowancePerTick *
                               function.feedback_vector().profiler_ticks();
  const int limit = raw_limit < BytecodeArray::kMaxLength
                        ? static_cast<int>(raw_limit)
                        : BytecodeArray::kMaxLength;
  DCHECK_GT(limit, 0);
  return function.shared().GetBytecodeArray(isolate).length() <= limit;
}

void TrySetOsrUrgency(Isolate* isolate, JSFunction function, int osr_urgency) {
  SharedFunctionInfo shared = function.shared();
  // Guaranteed since we've got a feedback vector.
  DCHECK(shared.IsUserJavaScript());

  if (V8_UNLIKELY(!v8_flags.use_osr)) return;
  if (V8_UNLIKELY(shared.optimization_disabled())) return;

  // We've passed all checks - bump the OSR urgency.

  FeedbackVector fv = function.feedback_vector();
  if (V8_UNLIKELY(v8_flags.trace_osr)) {
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintF(scope.file(),
           "[OSR - setting osr urgency. function: %s, old urgency: %d, new "
           "urgency: %d]\n",
           function.DebugNameCStr().get(), fv.osr_urgency(), osr_urgency);
  }

  DCHECK_GE(osr_urgency, fv.osr_urgency());  // Never lower urgency here.
  fv.set_osr_urgency(osr_urgency);
}

void TryIncrementOsrUrgency(Isolate* isolate, JSFunction function) {
  int old_urgency = function.feedback_vector().osr_urgency();
  int new_urgency = std::min(old_urgency + 1, FeedbackVector::kMaxOsrUrgency);
  TrySetOsrUrgency(isolate, function, new_urgency);
}

void TryRequestOsrAtNextOpportunity(Isolate* isolate, JSFunction function) {
  TrySetOsrUrgency(isolate, function, FeedbackVector::kMaxOsrUrgency);
}

bool ShouldOptimizeAsSmallFunction(int bytecode_size, bool any_ic_changed) {
  return !any_ic_changed &&
         bytecode_size < v8_flags.max_bytecode_size_for_early_opt;
}

}  // namespace

void TieringManager::RequestOsrAtNextOpportunity(JSFunction function) {
  DisallowGarbageCollection no_gc;
  TryRequestOsrAtNextOpportunity(isolate_, function);
}

void TieringManager::MaybeOptimizeFrame(JSFunction function,
                                        CodeKind code_kind) {
  const TieringState tiering_state = function.feedback_vector().tiering_state();
  const TieringState osr_tiering_state =
      function.feedback_vector().osr_tiering_state();
  if (V8_UNLIKELY(IsInProgress(tiering_state)) ||
      V8_UNLIKELY(IsInProgress(osr_tiering_state))) {
    // Note: This effectively disables OSR for the function while it is being
    // compiled.
    TraceInOptimizationQueue(function);
    return;
  }

  if (V8_UNLIKELY(v8_flags.testing_d8_test_runner) &&
      !PendingOptimizationTable::IsHeuristicOptimizationAllowed(isolate_,
                                                                function)) {
    TraceHeuristicOptimizationDisallowed(function);
    return;
  }

  // TODO(v8:7700): Consider splitting this up for Maglev/Turbofan.
  if (V8_UNLIKELY(function.shared().optimization_disabled())) return;

  if (V8_UNLIKELY(v8_flags.always_osr)) {
    TryRequestOsrAtNextOpportunity(isolate_, function);
    // Continue below and do a normal optimized compile as well.
  }

  const bool is_marked_for_any_optimization =
      (static_cast<uint32_t>(tiering_state) & kNoneOrInProgressMask) != 0;
  // Baseline OSR uses a separate mechanism and must not be considered here,
  // therefore we limit to kOptimizedJSFunctionCodeKindsMask.
  // TODO(v8:7700): Change the condition below for Maglev OSR once it is
  // implemented.
  if (is_marked_for_any_optimization ||
      function.HasAvailableHigherTierCodeThanWithFilter(
          code_kind, kOptimizedJSFunctionCodeKindsMask)) {
    // OSR kicks in only once we've previously decided to tier up, but we are
    // still in the lower-tier frame (this implies a long-running loop).
    //
    // TODO(v8:7700): In the presence of Maglev, OSR is triggered much earlier
    // than with the old pipeline since we tier up to Maglev earlier which
    // affects both conditions above. This *seems* fine (when stuck in a loop
    // we want to tier up, regardless of the active tier), but we may want to
    // think about this again at some point.
    if (SmallEnoughForOSR(isolate_, function, code_kind)) {
      TryIncrementOsrUrgency(isolate_, function);
    }

    // Return unconditionally and don't run through the optimization decision
    // again; we've already decided to tier up previously.
    return;
  }

  DCHECK(!is_marked_for_any_optimization &&
         !function.HasAvailableHigherTierCodeThanWithFilter(
             code_kind, kOptimizedJSFunctionCodeKindsMask));
  OptimizationDecision d = ShouldOptimize(function, code_kind);
  if (d.should_optimize()) Optimize(function, d);
}

OptimizationDecision TieringManager::ShouldOptimize(JSFunction function,
                                                    CodeKind code_kind) {
  if (TiersUpToMaglev(code_kind) &&
      function.shared().PassesFilter(v8_flags.maglev_filter) &&
      !function.shared(isolate_).maglev_compilation_failed()) {
    return OptimizationDecision::Maglev();
  } else if (code_kind == CodeKind::TURBOFAN) {
    // Already in the top tier.
    return OptimizationDecision::DoNotOptimize();
  }

  if (!v8_flags.turbofan ||
      !function.shared().PassesFilter(v8_flags.turbo_filter)) {
    return OptimizationDecision::DoNotOptimize();
  }

  BytecodeArray bytecode = function.shared().GetBytecodeArray(isolate_);
  const int ticks = function.feedback_vector().profiler_ticks();
  const int ticks_for_optimization =
      v8_flags.ticks_before_optimization +
      (bytecode.length() / v8_flags.bytecode_size_allowance_per_tick);
  if (ticks >= ticks_for_optimization) {
    return OptimizationDecision::TurbofanHotAndStable();
  } else if (ShouldOptimizeAsSmallFunction(bytecode.length(),
                                           any_ic_changed_)) {
    // If no IC was patched since the last tick and this function is very
    // small, optimistically optimize it now.
    return OptimizationDecision::TurbofanSmallFunction();
  } else if (v8_flags.trace_opt_verbose) {
    PrintF("[not yet optimizing %s, not enough ticks: %d/%d and ",
           function.DebugNameCStr().get(), ticks, ticks_for_optimization);
    if (any_ic_changed_) {
      PrintF("ICs changed]\n");
    } else {
      PrintF(" too large for small function optimization: %d/%d]\n",
             bytecode.length(),
             v8_flags.max_bytecode_size_for_early_opt.value());
    }
  }

  return OptimizationDecision::DoNotOptimize();
}

TieringManager::OnInterruptTickScope::OnInterruptTickScope(
    TieringManager* profiler)
    : profiler_(profiler) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.MarkCandidatesForOptimization");
}

TieringManager::OnInterruptTickScope::~OnInterruptTickScope() {
  profiler_->any_ic_changed_ = false;
}

void TieringManager::OnInterruptTick(Handle<JSFunction> function,
                                     CodeKind code_kind) {
  IsCompiledScope is_compiled_scope(
      function->shared().is_compiled_scope(isolate_));

  // Remember whether the function had a vector at this point. This is relevant
  // later since the configuration 'Ignition without a vector' can be
  // considered a tier on its own. We begin tiering up to tiers higher than
  // Sparkplug only when reaching this point *with* a feedback vector.
  const bool had_feedback_vector = function->has_feedback_vector();

  // Ensure that the feedback vector has been allocated, and reset the
  // interrupt budget in preparation for the next tick.
  if (had_feedback_vector) {
    function->SetInterruptBudget(isolate_);
  } else {
    JSFunction::CreateAndAttachFeedbackVector(isolate_, function,
                                              &is_compiled_scope);
    DCHECK(is_compiled_scope.is_compiled());
    // Also initialize the invocation count here. This is only really needed for
    // OSR. When we OSR functions with lazy feedback allocation we want to have
    // a non zero invocation count so we can inline functions.
    function->feedback_vector().set_invocation_count(1, kRelaxedStore);
  }

  DCHECK(function->has_feedback_vector());
  DCHECK(function->shared().is_compiled());
  DCHECK(function->shared().HasBytecodeArray());

  // TODO(jgruber): Consider integrating this into a linear tiering system
  // controlled by TieringState in which the order is always
  // Ignition-Sparkplug-Turbofan, and only a single tierup is requested at
  // once.
  // It's unclear whether this is possible and/or makes sense - for example,
  // batching compilation can introduce arbitrary latency between the SP
  // compile request and fulfillment, which doesn't work with strictly linear
  // tiering.
  if (CanCompileWithBaseline(isolate_, function->shared()) &&
      function->ActiveTierIsIgnition()) {
    if (v8_flags.baseline_batch_compilation) {
      isolate_->baseline_batch_compiler()->EnqueueFunction(function);
    } else {
      IsCompiledScope is_compiled_scope(
          function->shared().is_compiled_scope(isolate_));
      Compiler::CompileBaseline(isolate_, function, Compiler::CLEAR_EXCEPTION,
                                &is_compiled_scope);
    }
    function->shared().set_sparkplug_compiled(true);
  }

  // We only tier up beyond sparkplug if we already had a feedback vector.
  if (!had_feedback_vector) return;

  // Don't tier up if Turbofan is disabled.
  // TODO(jgruber): Update this for a multi-tier world.
  if (V8_UNLIKELY(!isolate_->use_optimizer())) return;

  // --- We've decided to proceed for now. ---

  DisallowGarbageCollection no_gc;
  OnInterruptTickScope scope(this);
  JSFunction function_obj = *function;

  function_obj.feedback_vector().SaturatingIncrementProfilerTicks();

  MaybeOptimizeFrame(function_obj, code_kind);
}

}  // namespace internal
}  // namespace v8

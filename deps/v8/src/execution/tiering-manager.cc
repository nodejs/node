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

// Maximum size in bytes of generate code for a function to allow OSR.
static const int kOSRBytecodeSizeAllowanceBase = 119;
static const int kOSRBytecodeSizeAllowancePerTick = 44;

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
STATIC_ASSERT(sizeof(OptimizationDecision) <= kInt32Size);

namespace {

void TraceInOptimizationQueue(JSFunction function) {
  if (FLAG_trace_opt_verbose) {
    PrintF("[not marking function %s for optimization: already queued]\n",
           function.DebugNameCStr().get());
  }
}

void TraceHeuristicOptimizationDisallowed(JSFunction function) {
  if (FLAG_trace_opt_verbose) {
    PrintF(
        "[not marking function %s for optimization: marked with "
        "%%PrepareFunctionForOptimization for manual optimization]\n",
        function.DebugNameCStr().get());
  }
}

void TraceRecompile(Isolate* isolate, JSFunction function,
                    OptimizationDecision d) {
  if (FLAG_trace_opt) {
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
  if (FLAG_trace_opt) {
    PrintF("[manually marking ");
    function.ShortPrint();
    PrintF(" for optimization to %s, %s]\n", CodeKindToString(code_kind),
           ToString(concurrency_mode));
  }
}

void TieringManager::Optimize(JSFunction function, CodeKind code_kind,
                              OptimizationDecision d) {
  DCHECK(d.should_optimize());
  TraceRecompile(isolate_, function, d);
  function.MarkForOptimization(isolate_, d.code_kind, d.concurrency_mode);
}

namespace {

bool HaveCachedOSRCodeForCurrentBytecodeOffset(UnoptimizedFrame* frame,
                                               int* osr_urgency_out) {
  JSFunction function = frame->function();
  const int current_offset = frame->GetBytecodeOffset();
  OSROptimizedCodeCache cache = function.native_context().osr_code_cache();
  interpreter::BytecodeArrayIterator iterator(
      handle(frame->GetBytecodeArray(), frame->isolate()));
  for (BytecodeOffset osr_offset : cache.OsrOffsetsFor(function.shared())) {
    DCHECK(!osr_offset.IsNone());
    iterator.SetOffset(osr_offset.ToInt());
    if (base::IsInRange(current_offset, iterator.GetJumpTargetOffset(),
                        osr_offset.ToInt())) {
      int loop_depth = iterator.GetImmediateOperand(1);
      // `+ 1` because osr_urgency is an exclusive upper limit on the depth.
      *osr_urgency_out = loop_depth + 1;
      return true;
    }
  }
  return false;
}

}  // namespace

namespace {

bool TiersUpToMaglev(CodeKind code_kind) {
  // TODO(v8:7700): Flip the UNLIKELY when appropriate.
  return V8_UNLIKELY(FLAG_maglev) && CodeKindIsUnoptimizedJSFunction(code_kind);
}

bool TiersUpToMaglev(base::Optional<CodeKind> code_kind) {
  return code_kind.has_value() && TiersUpToMaglev(code_kind.value());
}

}  // namespace

// static
int TieringManager::InterruptBudgetFor(Isolate* isolate, JSFunction function) {
  if (function.has_feedback_vector()) {
    return TiersUpToMaglev(function.GetActiveTier())
               ? FLAG_interrupt_budget_for_maglev
               : FLAG_interrupt_budget;
  }

  DCHECK(!function.has_feedback_vector());
  DCHECK(function.shared().is_compiled());
  return function.shared().GetBytecodeArray(isolate).length() *
         FLAG_interrupt_budget_factor_for_feedback_allocation;
}

// static
int TieringManager::InitialInterruptBudget() {
  return V8_LIKELY(FLAG_lazy_feedback_allocation)
             ? FLAG_interrupt_budget_for_feedback_allocation
             : FLAG_interrupt_budget;
}

namespace {

bool SmallEnoughForOSR(Isolate* isolate, JSFunction function) {
  return function.shared().GetBytecodeArray(isolate).length() <=
         kOSRBytecodeSizeAllowanceBase +
             function.feedback_vector().profiler_ticks() *
                 kOSRBytecodeSizeAllowancePerTick;
}

void TrySetOsrUrgency(Isolate* isolate, JSFunction function, int osr_urgency) {
  SharedFunctionInfo shared = function.shared();

  if (V8_UNLIKELY(!FLAG_use_osr)) return;
  if (V8_UNLIKELY(!shared.IsUserJavaScript())) return;
  if (V8_UNLIKELY(shared.optimization_disabled())) return;

  // We've passed all checks - bump the OSR urgency.

  BytecodeArray bytecode = shared.GetBytecodeArray(isolate);
  if (V8_UNLIKELY(FLAG_trace_osr)) {
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintF(scope.file(),
           "[OSR - setting osr urgency. function: %s, old urgency: %d, new "
           "urgency: %d]\n",
           function.DebugNameCStr().get(), bytecode.osr_urgency(), osr_urgency);
  }

  DCHECK_GE(osr_urgency, bytecode.osr_urgency());  // Never lower urgency here.
  bytecode.set_osr_urgency(osr_urgency);
}

void TryIncrementOsrUrgency(Isolate* isolate, JSFunction function) {
  int old_urgency = function.shared().GetBytecodeArray(isolate).osr_urgency();
  int new_urgency = std::min(old_urgency + 1, BytecodeArray::kMaxOsrUrgency);
  TrySetOsrUrgency(isolate, function, new_urgency);
}

void TryRequestOsrAtNextOpportunity(Isolate* isolate, JSFunction function) {
  TrySetOsrUrgency(isolate, function, BytecodeArray::kMaxOsrUrgency);
}

void TryRequestOsrForCachedOsrCode(Isolate* isolate, JSFunction function,
                                   int osr_urgency_for_cached_osr_code) {
  DCHECK_LE(osr_urgency_for_cached_osr_code, BytecodeArray::kMaxOsrUrgency);
  int old_urgency = function.shared().GetBytecodeArray(isolate).osr_urgency();
  // Make sure not to decrease the existing urgency.
  int new_urgency = std::max(old_urgency, osr_urgency_for_cached_osr_code);
  TrySetOsrUrgency(isolate, function, new_urgency);
}

bool ShouldOptimizeAsSmallFunction(int bytecode_size, bool any_ic_changed) {
  return !any_ic_changed &&
         bytecode_size < FLAG_max_bytecode_size_for_early_opt;
}

}  // namespace

void TieringManager::RequestOsrAtNextOpportunity(JSFunction function) {
  DisallowGarbageCollection no_gc;
  TryRequestOsrAtNextOpportunity(isolate_, function);
}

void TieringManager::MaybeOptimizeFrame(JSFunction function,
                                        UnoptimizedFrame* frame,
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

  if (V8_UNLIKELY(FLAG_testing_d8_test_runner) &&
      !PendingOptimizationTable::IsHeuristicOptimizationAllowed(isolate_,
                                                                function)) {
    TraceHeuristicOptimizationDisallowed(function);
    return;
  }

  // TODO(v8:7700): Consider splitting this up for Maglev/Turbofan.
  if (V8_UNLIKELY(function.shared().optimization_disabled())) return;

  if (V8_UNLIKELY(FLAG_always_osr)) {
    TryRequestOsrAtNextOpportunity(isolate_, function);
    // Continue below and do a normal optimized compile as well.
  }

  // If we have matching cached OSR'd code, request OSR at the next opportunity.
  int osr_urgency_for_cached_osr_code;
  if (HaveCachedOSRCodeForCurrentBytecodeOffset(
          frame, &osr_urgency_for_cached_osr_code)) {
    TryRequestOsrForCachedOsrCode(isolate_, function,
                                  osr_urgency_for_cached_osr_code);
  }

  const bool is_marked_for_any_optimization =
      (static_cast<uint32_t>(tiering_state) & kNoneOrInProgressMask) != 0;
  if (is_marked_for_any_optimization || function.HasAvailableOptimizedCode()) {
    // OSR kicks in only once we've previously decided to tier up, but we are
    // still in the unoptimized frame (this implies a long-running loop).
    if (SmallEnoughForOSR(isolate_, function)) {
      TryIncrementOsrUrgency(isolate_, function);
    }

    // Return unconditionally and don't run through the optimization decision
    // again; we've already decided to tier up previously.
    return;
  }

  DCHECK(!is_marked_for_any_optimization &&
         !function.HasAvailableOptimizedCode());
  OptimizationDecision d = ShouldOptimize(function, code_kind, frame);
  if (d.should_optimize()) Optimize(function, code_kind, d);
}

OptimizationDecision TieringManager::ShouldOptimize(JSFunction function,
                                                    CodeKind code_kind,
                                                    JavaScriptFrame* frame) {
  DCHECK_EQ(code_kind, function.GetActiveTier().value());

  if (TiersUpToMaglev(code_kind) &&
      !function.shared(isolate_).maglev_compilation_failed()) {
    return OptimizationDecision::Maglev();
  } else if (code_kind == CodeKind::TURBOFAN) {
    // Already in the top tier.
    return OptimizationDecision::DoNotOptimize();
  }

  BytecodeArray bytecode = function.shared().GetBytecodeArray(isolate_);
  const int ticks = function.feedback_vector().profiler_ticks();
  const int ticks_for_optimization =
      FLAG_ticks_before_optimization +
      (bytecode.length() / FLAG_bytecode_size_allowance_per_tick);
  if (ticks >= ticks_for_optimization) {
    return OptimizationDecision::TurbofanHotAndStable();
  } else if (ShouldOptimizeAsSmallFunction(bytecode.length(),
                                           any_ic_changed_)) {
    // If no IC was patched since the last tick and this function is very
    // small, optimistically optimize it now.
    return OptimizationDecision::TurbofanSmallFunction();
  } else if (FLAG_trace_opt_verbose) {
    PrintF("[not yet optimizing %s, not enough ticks: %d/%d and ",
           function.DebugNameCStr().get(), ticks, ticks_for_optimization);
    if (any_ic_changed_) {
      PrintF("ICs changed]\n");
    } else {
      PrintF(" too large for small function optimization: %d/%d]\n",
             bytecode.length(), FLAG_max_bytecode_size_for_early_opt);
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

void TieringManager::OnInterruptTick(Handle<JSFunction> function) {
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
      !function->ActiveTierIsBaseline()) {
    if (FLAG_baseline_batch_compilation) {
      isolate_->baseline_batch_compiler()->EnqueueFunction(function);
    } else {
      IsCompiledScope is_compiled_scope(
          function->shared().is_compiled_scope(isolate_));
      Compiler::CompileBaseline(isolate_, function, Compiler::CLEAR_EXCEPTION,
                                &is_compiled_scope);
    }
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

  JavaScriptFrameIterator it(isolate_);
  UnoptimizedFrame* frame = UnoptimizedFrame::cast(it.frame());
  const CodeKind code_kind = function_obj.GetActiveTier().value();
  MaybeOptimizeFrame(function_obj, frame, code_kind);
}

}  // namespace internal
}  // namespace v8

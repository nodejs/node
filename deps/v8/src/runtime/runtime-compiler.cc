// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/asmjs/asm-js.h"
#include "src/codegen/compilation-cache.h"
#include "src/codegen/compiler.h"
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/common/message-template.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/arguments-inl.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/shared-function-info.h"

namespace v8 {
namespace internal {

namespace {
void LogExecution(Isolate* isolate, Handle<JSFunction> function) {
  DCHECK(v8_flags.log_function_events);
  if (!function->has_feedback_vector()) return;
  if (!function->feedback_vector().log_next_execution()) return;
  Handle<SharedFunctionInfo> sfi(function->shared(), isolate);
  Handle<String> name = SharedFunctionInfo::DebugName(sfi);
  DisallowGarbageCollection no_gc;
  auto raw_sfi = *sfi;
  std::string event_name = "first-execution";
  CodeKind kind = function->abstract_code(isolate).kind(isolate);
  // Not adding "-interpreter" for tooling backwards compatiblity.
  if (kind != CodeKind::INTERPRETED_FUNCTION) {
    event_name += "-";
    event_name += CodeKindToString(kind);
  }
  LOG(isolate,
      FunctionEvent(event_name.c_str(), Script::cast(raw_sfi.script()).id(), 0,
                    raw_sfi.StartPosition(), raw_sfi.EndPosition(), *name));
  function->feedback_vector().set_log_next_execution(false);
}
}  // namespace

RUNTIME_FUNCTION(Runtime_CompileLazy) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<JSFunction> function = args.at<JSFunction>(0);
  StackLimitCheck check(isolate);
  if (V8_UNLIKELY(
          check.JsHasOverflowed(kStackSpaceRequiredForCompilation * KB))) {
    return isolate->StackOverflow();
  }

  Handle<SharedFunctionInfo> sfi(function->shared(), isolate);

  DCHECK(!function->is_compiled());
#ifdef DEBUG
  if (v8_flags.trace_lazy && sfi->is_compiled()) {
    PrintF("[unoptimized: %s]\n", function->DebugNameCStr().get());
  }
#endif
  IsCompiledScope is_compiled_scope;
  if (!Compiler::Compile(isolate, function, Compiler::KEEP_EXCEPTION,
                         &is_compiled_scope)) {
    return ReadOnlyRoots(isolate).exception();
  }
  if (V8_UNLIKELY(v8_flags.log_function_events)) {
    LogExecution(isolate, function);
  }
  DCHECK(function->is_compiled());
  return function->code();
}

RUNTIME_FUNCTION(Runtime_InstallBaselineCode) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<JSFunction> function = args.at<JSFunction>(0);
  Handle<SharedFunctionInfo> sfi(function->shared(), isolate);
  DCHECK(sfi->HasBaselineCode());
  IsCompiledScope is_compiled_scope(*sfi, isolate);
  DCHECK(!function->HasAvailableOptimizedCode());
  DCHECK(!function->has_feedback_vector());
  JSFunction::CreateAndAttachFeedbackVector(isolate, function,
                                            &is_compiled_scope);
  {
    DisallowGarbageCollection no_gc;
    CodeT baseline_code = sfi->baseline_code(kAcquireLoad);
    function->set_code(baseline_code);
    if V8_LIKELY (!v8_flags.log_function_events) return baseline_code;
  }
  DCHECK(v8_flags.log_function_events);
  LogExecution(isolate, function);
  // LogExecution might allocate, reload the baseline code
  return sfi->baseline_code(kAcquireLoad);
}

RUNTIME_FUNCTION(Runtime_CompileOptimized) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<JSFunction> function = args.at<JSFunction>(0);

  CodeKind target_kind;
  ConcurrencyMode mode;
  DCHECK(function->has_feedback_vector());
  switch (function->tiering_state()) {
    case TieringState::kRequestMaglev_Synchronous:
      target_kind = CodeKind::MAGLEV;
      mode = ConcurrencyMode::kSynchronous;
      break;
    case TieringState::kRequestMaglev_Concurrent:
      target_kind = CodeKind::MAGLEV;
      mode = ConcurrencyMode::kConcurrent;
      break;
    case TieringState::kRequestTurbofan_Synchronous:
      target_kind = CodeKind::TURBOFAN;
      mode = ConcurrencyMode::kSynchronous;
      break;
    case TieringState::kRequestTurbofan_Concurrent:
      target_kind = CodeKind::TURBOFAN;
      mode = ConcurrencyMode::kConcurrent;
      break;
    case TieringState::kNone:
    case TieringState::kInProgress:
      UNREACHABLE();
  }

  // As a pre- and post-condition of CompileOptimized, the function *must* be
  // compiled, i.e. the installed Code object must not be CompileLazy.
  IsCompiledScope is_compiled_scope(function->shared(), isolate);
  DCHECK(is_compiled_scope.is_compiled());

  StackLimitCheck check(isolate);
  // Concurrent optimization runs on another thread, thus no additional gap.
  const int gap =
      IsConcurrent(mode) ? 0 : kStackSpaceRequiredForCompilation * KB;
  if (check.JsHasOverflowed(gap)) return isolate->StackOverflow();

  Compiler::CompileOptimized(isolate, function, mode, target_kind);

  DCHECK(function->is_compiled());
  if (V8_UNLIKELY(v8_flags.log_function_events)) {
    LogExecution(isolate, function);
  }
  return function->code();
}

RUNTIME_FUNCTION(Runtime_FunctionLogNextExecution) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<JSFunction> js_function = args.at<JSFunction>(0);
  DCHECK(v8_flags.log_function_events);
  LogExecution(isolate, js_function);
  return js_function->code();
}

RUNTIME_FUNCTION(Runtime_HealOptimizedCodeSlot) {
  SealHandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<JSFunction> function = args.at<JSFunction>(0);

  DCHECK(function->shared().is_compiled());

  function->feedback_vector().EvictOptimizedCodeMarkedForDeoptimization(
      function->shared(), "Runtime_HealOptimizedCodeSlot");
  return function->code();
}

RUNTIME_FUNCTION(Runtime_InstantiateAsmJs) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 4);
  Handle<JSFunction> function = args.at<JSFunction>(0);

  Handle<JSReceiver> stdlib;
  if (args[1].IsJSReceiver()) {
    stdlib = args.at<JSReceiver>(1);
  }
  Handle<JSReceiver> foreign;
  if (args[2].IsJSReceiver()) {
    foreign = args.at<JSReceiver>(2);
  }
  Handle<JSArrayBuffer> memory;
  if (args[3].IsJSArrayBuffer()) {
    memory = args.at<JSArrayBuffer>(3);
  }
  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
#if V8_ENABLE_WEBASSEMBLY
  if (shared->HasAsmWasmData()) {
    Handle<AsmWasmData> data(shared->asm_wasm_data(), isolate);
    MaybeHandle<Object> result = AsmJs::InstantiateAsmWasm(
        isolate, shared, data, stdlib, foreign, memory);
    if (!result.is_null()) return *result.ToHandleChecked();
    // Remove wasm data, mark as broken for asm->wasm, replace function code
    // with UncompiledData, and return a smi 0 to indicate failure.
    SharedFunctionInfo::DiscardCompiled(isolate, shared);
  }
  shared->set_is_asm_wasm_broken(true);
#endif
  DCHECK_EQ(function->code(), *BUILTIN_CODE(isolate, InstantiateAsmJs));
  function->set_code(*BUILTIN_CODE(isolate, CompileLazy));
  DCHECK(!isolate->has_pending_exception());
  return Smi::zero();
}

namespace {

// Whether the deopt exit is contained by the outermost loop containing the
// osr'd loop. For example:
//
//  for (;;) {
//    for (;;) {
//    }  // OSR is triggered on this backedge.
//  }  // This is the outermost loop containing the osr'd loop.
bool DeoptExitIsInsideOsrLoop(Isolate* isolate, JSFunction function,
                              BytecodeOffset deopt_exit_offset,
                              BytecodeOffset osr_offset) {
  DisallowGarbageCollection no_gc;
  DCHECK(!deopt_exit_offset.IsNone());
  DCHECK(!osr_offset.IsNone());

  Handle<BytecodeArray> bytecode_array(
      function.shared().GetBytecodeArray(isolate), isolate);
  DCHECK(interpreter::BytecodeArrayIterator::IsValidOffset(
      bytecode_array, deopt_exit_offset.ToInt()));

  interpreter::BytecodeArrayIterator it(bytecode_array, osr_offset.ToInt());
  DCHECK_EQ(it.current_bytecode(), interpreter::Bytecode::kJumpLoop);

  for (; !it.done(); it.Advance()) {
    const int current_offset = it.current_offset();
    // If we've reached the deopt exit, it's contained in the current loop
    // (this is covered by IsInRange below, but this check lets us avoid
    // useless iteration).
    if (current_offset == deopt_exit_offset.ToInt()) return true;
    // We're only interested in loop ranges.
    if (it.current_bytecode() != interpreter::Bytecode::kJumpLoop) continue;
    // Is the deopt exit contained in the current loop?
    if (base::IsInRange(deopt_exit_offset.ToInt(), it.GetJumpTargetOffset(),
                        current_offset)) {
      return true;
    }
    // We've reached nesting level 0, i.e. the current JumpLoop concludes a
    // top-level loop.
    const int loop_nesting_level = it.GetImmediateOperand(1);
    if (loop_nesting_level == 0) return false;
  }

  UNREACHABLE();
}

bool TryGetOptimizedOsrCode(Isolate* isolate, FeedbackVector vector,
                            const interpreter::BytecodeArrayIterator& it,
                            CodeT* code_out) {
  base::Optional<CodeT> maybe_code =
      vector.GetOptimizedOsrCode(isolate, it.GetSlotOperand(2));
  if (maybe_code.has_value()) {
    *code_out = maybe_code.value();
    return true;
  }
  return false;
}

// Deoptimize all osr'd loops which is in the same outermost loop with deopt
// exit. For example:
//  for (;;) {
//    for (;;) {
//    }  // Type a: loop start < OSR backedge < deopt exit
//    for (;;) {
//      <- Deopt
//      for (;;) {
//      }  // Type b: deopt exit < loop start < OSR backedge
//    } // Type c: loop start < deopt exit < OSR backedge
//  }  // The outermost loop
void DeoptAllOsrLoopsContainingDeoptExit(Isolate* isolate, JSFunction function,
                                         BytecodeOffset deopt_exit_offset) {
  DisallowGarbageCollection no_gc;
  DCHECK(!deopt_exit_offset.IsNone());

  if (!v8_flags.use_ic ||
      !function.feedback_vector().maybe_has_optimized_osr_code()) {
    return;
  }
  Handle<BytecodeArray> bytecode_array(
      function.shared().GetBytecodeArray(isolate), isolate);
  DCHECK(interpreter::BytecodeArrayIterator::IsValidOffset(
      bytecode_array, deopt_exit_offset.ToInt()));

  interpreter::BytecodeArrayIterator it(bytecode_array,
                                        deopt_exit_offset.ToInt());

  FeedbackVector vector = function.feedback_vector();
  CodeT code;
  base::SmallVector<CodeT, 8> osr_codes;
  // Visit before the first loop-with-deopt is found
  for (; !it.done(); it.Advance()) {
    // We're only interested in loop ranges.
    if (it.current_bytecode() != interpreter::Bytecode::kJumpLoop) continue;
    // Is the deopt exit contained in the current loop?
    if (base::IsInRange(deopt_exit_offset.ToInt(), it.GetJumpTargetOffset(),
                        it.current_offset())) {
      break;
    }
    // We've reached nesting level 0, i.e. the current JumpLoop concludes a
    // top-level loop, return as the deopt exit is not in any loop. For example:
    //  <- Deopt
    //  for (;;) {
    //  } // The outermost loop
    const int loop_nesting_level = it.GetImmediateOperand(1);
    if (loop_nesting_level == 0) return;
    if (TryGetOptimizedOsrCode(isolate, vector, it, &code)) {
      // Collect type b osr'd loops
      osr_codes.push_back(code);
    }
  }
  if (it.done()) return;
  for (size_t i = 0, size = osr_codes.size(); i < size; i++) {
    // Deoptimize type b osr'd loops
    Deoptimizer::DeoptimizeFunction(function, osr_codes[i]);
  }
  // Visit after the first loop-with-deopt is found
  int last_deopt_in_range_loop_jump_target;
  for (; !it.done(); it.Advance()) {
    // We're only interested in loop ranges.
    if (it.current_bytecode() != interpreter::Bytecode::kJumpLoop) continue;
    // We've reached a new nesting loop in the case of the deopt exit is in a
    // loop whose outermost loop was removed. For example:
    //  for (;;) {
    //    <- Deopt
    //  } // The non-outermost loop
    //  for (;;) {
    //  } // The outermost loop
    if (it.GetJumpTargetOffset() > deopt_exit_offset.ToInt()) break;
    last_deopt_in_range_loop_jump_target = it.GetJumpTargetOffset();
    if (TryGetOptimizedOsrCode(isolate, vector, it, &code)) {
      // Deoptimize type c osr'd loops
      Deoptimizer::DeoptimizeFunction(function, code);
    }
    // We've reached nesting level 0, i.e. the current JumpLoop concludes a
    // top-level loop.
    const int loop_nesting_level = it.GetImmediateOperand(1);
    if (loop_nesting_level == 0) break;
  }
  if (it.done()) return;
  // Revisit from start of the last deopt in range loop to deopt
  for (it.SetOffset(last_deopt_in_range_loop_jump_target);
       it.current_offset() < deopt_exit_offset.ToInt(); it.Advance()) {
    // We're only interested in loop ranges.
    if (it.current_bytecode() != interpreter::Bytecode::kJumpLoop) continue;
    if (TryGetOptimizedOsrCode(isolate, vector, it, &code)) {
      // Deoptimize type a osr'd loops
      Deoptimizer::DeoptimizeFunction(function, code);
    }
  }
}

}  // namespace

RUNTIME_FUNCTION(Runtime_NotifyDeoptimized) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  Deoptimizer* deoptimizer = Deoptimizer::Grab(isolate);
  DCHECK(CodeKindCanDeoptimize(deoptimizer->compiled_code()->kind()));
  DCHECK(AllowGarbageCollection::IsAllowed());
  DCHECK(isolate->context().is_null());

  TimerEventScope<TimerEventDeoptimizeCode> timer(isolate);
  TRACE_EVENT0("v8", "V8.DeoptimizeCode");
  Handle<JSFunction> function = deoptimizer->function();
  // For OSR the optimized code isn't installed on the function, so get the
  // code object from deoptimizer.
  Handle<Code> optimized_code = deoptimizer->compiled_code();
  const DeoptimizeKind deopt_kind = deoptimizer->deopt_kind();
  const DeoptimizeReason deopt_reason =
      deoptimizer->GetDeoptInfo().deopt_reason;

  // TODO(turbofan): We currently need the native context to materialize
  // the arguments object, but only to get to its map.
  isolate->set_context(deoptimizer->function()->native_context());

  // Make sure to materialize objects before causing any allocation.
  deoptimizer->MaterializeHeapObjects();
  const BytecodeOffset deopt_exit_offset =
      deoptimizer->bytecode_offset_in_outermost_frame();
  delete deoptimizer;

  // Ensure the context register is updated for materialized objects.
  JavaScriptFrameIterator top_it(isolate);
  JavaScriptFrame* top_frame = top_it.frame();
  isolate->set_context(Context::cast(top_frame->context()));

  // Lazy deopts don't invalidate the underlying optimized code since the code
  // object itself is still valid (as far as we know); the called function
  // caused the deopt, not the function we're currently looking at.
  if (deopt_kind == DeoptimizeKind::kLazy) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  // Some eager deopts also don't invalidate Code (e.g. when preparing for OSR
  // from Maglev to Turbofan).
  if (IsDeoptimizationWithoutCodeInvalidation(deopt_reason)) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  // Non-OSR'd code is deoptimized unconditionally. If the deoptimization occurs
  // inside the outermost loop containning a loop that can trigger OSR
  // compilation, we remove the OSR code, it will avoid hit the out of date OSR
  // code and soon later deoptimization.
  //
  // For OSR'd code, we keep the optimized code around if deoptimization occurs
  // outside the outermost loop containing the loop that triggered OSR
  // compilation. The reasoning is that OSR is intended to speed up the
  // long-running loop; so if the deoptimization occurs outside this loop it is
  // still worth jumping to the OSR'd code on the next run. The reduced cost of
  // the loop should pay for the deoptimization costs.
  const BytecodeOffset osr_offset = optimized_code->osr_offset();
  if (osr_offset.IsNone()) {
    Deoptimizer::DeoptimizeFunction(*function, ToCodeT(*optimized_code));
    DeoptAllOsrLoopsContainingDeoptExit(isolate, *function, deopt_exit_offset);
  } else if (DeoptExitIsInsideOsrLoop(isolate, *function, deopt_exit_offset,
                                      osr_offset)) {
    Deoptimizer::DeoptimizeFunction(*function, ToCodeT(*optimized_code));
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_ObserveNode) {
  // The %ObserveNode intrinsic only tracks the changes to an observed node in
  // code compiled by TurboFan.
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<Object> obj = args.at(0);
  return *obj;
}

RUNTIME_FUNCTION(Runtime_VerifyType) {
  // %VerifyType has no effect in the interpreter.
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<Object> obj = args.at(0);
  return *obj;
}

namespace {

void GetOsrOffsetAndFunctionForOSR(Isolate* isolate, BytecodeOffset* osr_offset,
                                   Handle<JSFunction>* function) {
  DCHECK(osr_offset->IsNone());
  DCHECK(function->is_null());

  // Determine the frame that triggered the OSR request.
  JavaScriptFrameIterator it(isolate);
  UnoptimizedFrame* frame = UnoptimizedFrame::cast(it.frame());
  DCHECK_IMPLIES(frame->is_interpreted(),
                 frame->LookupCodeT().is_interpreter_trampoline_builtin());
  DCHECK_IMPLIES(frame->is_baseline(),
                 frame->LookupCodeT().kind() == CodeKind::BASELINE);

  *osr_offset = BytecodeOffset(frame->GetBytecodeOffset());
  *function = handle(frame->function(), isolate);

  DCHECK(!osr_offset->IsNone());
  DCHECK((*function)->shared().HasBytecodeArray());
}

Object CompileOptimizedOSR(Isolate* isolate, Handle<JSFunction> function,
                           BytecodeOffset osr_offset) {
  const ConcurrencyMode mode =
      V8_LIKELY(isolate->concurrent_recompilation_enabled() &&
                v8_flags.concurrent_osr)
          ? ConcurrencyMode::kConcurrent
          : ConcurrencyMode::kSynchronous;

  Handle<CodeT> result;
  if (!Compiler::CompileOptimizedOSR(isolate, function, osr_offset, mode)
           .ToHandle(&result)) {
    // An empty result can mean one of two things:
    // 1) we've started a concurrent compilation job - everything is fine.
    // 2) synchronous compilation failed for some reason.

    if (!function->HasAttachedOptimizedCode()) {
      function->set_code(function->shared().GetCode(), kReleaseStore);
    }

    return {};
  }

  DCHECK(!result.is_null());
  DCHECK(result->is_turbofanned());  // TODO(v8:7700): Support Maglev.
  DCHECK(CodeKindIsOptimizedJSFunction(result->kind()));

#ifdef DEBUG
  DeoptimizationData data =
      DeoptimizationData::cast(result->deoptimization_data());
  DCHECK_EQ(BytecodeOffset(data.OsrBytecodeOffset().value()), osr_offset);
  DCHECK_GE(data.OsrPcOffset().value(), 0);
#endif  // DEBUG

  if (function->feedback_vector().invocation_count() <= 1 &&
      !IsNone(function->tiering_state()) &&
      !IsInProgress(function->tiering_state())) {
    // With lazy feedback allocation we may not have feedback for the
    // initial part of the function that was executed before we allocated a
    // feedback vector. Reset any tiering states for such functions.
    //
    // TODO(mythria): Instead of resetting the tiering state here we
    // should only mark a function for optimization if it has sufficient
    // feedback. We cannot do this currently since we OSR only after we mark
    // a function for optimization. We should instead change it to be based
    // based on number of ticks.
    function->reset_tiering_state();
  }
  // First execution logging happens in LogOrTraceOptimizedOSREntry
  return *result;
}

}  // namespace

RUNTIME_FUNCTION(Runtime_CompileOptimizedOSR) {
  HandleScope handle_scope(isolate);
  DCHECK_EQ(0, args.length());
  DCHECK(v8_flags.use_osr);

  BytecodeOffset osr_offset = BytecodeOffset::None();
  Handle<JSFunction> function;
  GetOsrOffsetAndFunctionForOSR(isolate, &osr_offset, &function);

  return CompileOptimizedOSR(isolate, function, osr_offset);
}

RUNTIME_FUNCTION(Runtime_CompileOptimizedOSRFromMaglev) {
  HandleScope handle_scope(isolate);
  DCHECK_EQ(1, args.length());
  DCHECK(v8_flags.use_osr);

  const BytecodeOffset osr_offset(args.positive_smi_value_at(0));

  JavaScriptFrameIterator it(isolate);
  MaglevFrame* frame = MaglevFrame::cast(it.frame());
  DCHECK_EQ(frame->LookupCodeT().kind(), CodeKind::MAGLEV);
  Handle<JSFunction> function = handle(frame->function(), isolate);

  return CompileOptimizedOSR(isolate, function, osr_offset);
}

RUNTIME_FUNCTION(Runtime_LogOrTraceOptimizedOSREntry) {
  HandleScope handle_scope(isolate);
  DCHECK_EQ(0, args.length());
  CHECK(v8_flags.trace_osr || v8_flags.log_function_events);

  BytecodeOffset osr_offset = BytecodeOffset::None();
  Handle<JSFunction> function;
  GetOsrOffsetAndFunctionForOSR(isolate, &osr_offset, &function);

  if (v8_flags.trace_osr) {
    PrintF(CodeTracer::Scope{isolate->GetCodeTracer()}.file(),
           "[OSR - entry. function: %s, osr offset: %d]\n",
           function->DebugNameCStr().get(), osr_offset.ToInt());
  }
  if (V8_UNLIKELY(v8_flags.log_function_events)) {
    LogExecution(isolate, function);
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

static Object CompileGlobalEval(Isolate* isolate,
                                Handle<i::Object> source_object,
                                Handle<SharedFunctionInfo> outer_info,
                                LanguageMode language_mode,
                                int eval_scope_position, int eval_position) {
  Handle<Context> context(isolate->context(), isolate);
  Handle<Context> native_context(context->native_context(), isolate);

  // Check if native context allows code generation from
  // strings. Throw an exception if it doesn't.
  MaybeHandle<String> source;
  bool unknown_object;
  std::tie(source, unknown_object) = Compiler::ValidateDynamicCompilationSource(
      isolate, native_context, source_object);
  // If the argument is an unhandled string time, bounce to GlobalEval.
  if (unknown_object) {
    return native_context->global_eval_fun();
  }
  if (source.is_null()) {
    Handle<Object> error_message =
        native_context->ErrorMessageForCodeGenerationFromStrings();
    Handle<Object> error;
    MaybeHandle<Object> maybe_error = isolate->factory()->NewEvalError(
        MessageTemplate::kCodeGenFromStrings, error_message);
    if (maybe_error.ToHandle(&error)) isolate->Throw(*error);
    return ReadOnlyRoots(isolate).exception();
  }

  // Deal with a normal eval call with a string argument. Compile it
  // and return the compiled function bound in the local context.
  static const ParseRestriction restriction = NO_PARSE_RESTRICTION;
  Handle<JSFunction> compiled;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, compiled,
      Compiler::GetFunctionFromEval(
          source.ToHandleChecked(), outer_info, context, language_mode,
          restriction, kNoSourcePosition, eval_scope_position, eval_position),
      ReadOnlyRoots(isolate).exception());
  return *compiled;
}

RUNTIME_FUNCTION(Runtime_ResolvePossiblyDirectEval) {
  HandleScope scope(isolate);
  DCHECK_EQ(6, args.length());

  Handle<Object> callee = args.at(0);

  // If "eval" didn't refer to the original GlobalEval, it's not a
  // direct call to eval.
  if (*callee != isolate->native_context()->global_eval_fun()) {
    return *callee;
  }

  DCHECK(is_valid_language_mode(args.smi_value_at(3)));
  LanguageMode language_mode = static_cast<LanguageMode>(args.smi_value_at(3));
  Handle<SharedFunctionInfo> outer_info(args.at<JSFunction>(2)->shared(),
                                        isolate);
  return CompileGlobalEval(isolate, args.at<Object>(1), outer_info,
                           language_mode, args.smi_value_at(4),
                           args.smi_value_at(5));
}

}  // namespace internal
}  // namespace v8

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/asmjs/asm-js.h"
#include "src/baseline/baseline.h"
#include "src/codegen/compilation-cache.h"
#include "src/codegen/compiler.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/common/message-template.h"
#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"
#include "src/compiler/pipeline.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/arguments-inl.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/v8threads.h"
#include "src/execution/vm-state-inl.h"
#include "src/heap/parked-scope.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

namespace {

// Returns false iff an exception was thrown.
bool MaybeSpawnNativeContextIndependentCompilationJob(
    Isolate* isolate, Handle<JSFunction> function, ConcurrencyMode mode) {
  if (!FLAG_turbo_nci) return true;  // Nothing to do.
  return Compiler::CompileOptimized(isolate, function, mode,
                                    CodeKind::NATIVE_CONTEXT_INDEPENDENT);
}

Object CompileOptimized(Isolate* isolate, Handle<JSFunction> function,
                        ConcurrencyMode mode) {
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed(kStackSpaceRequiredForCompilation * KB)) {
    return isolate->StackOverflow();
  }

  // Compile for the next tier.
  if (!Compiler::CompileOptimized(isolate, function, mode,
                                  function->NextTier())) {
    return ReadOnlyRoots(isolate).exception();
  }

  // Possibly compile for NCI caching.
  if (!MaybeSpawnNativeContextIndependentCompilationJob(isolate, function,
                                                        mode)) {
    return ReadOnlyRoots(isolate).exception();
  }

  // As a post-condition of CompileOptimized, the function *must* be compiled,
  // i.e. the installed Code object must not be the CompileLazy builtin.
  DCHECK(function->is_compiled());
  return function->code();
}

void TryInstallNCICode(Isolate* isolate, Handle<JSFunction> function,
                       Handle<SharedFunctionInfo> sfi,
                       IsCompiledScope* is_compiled_scope) {
  // This function should only be called if there's a possibility that cached
  // code exists.
  DCHECK(sfi->may_have_cached_code());
  DCHECK_EQ(function->shared(), *sfi);

  Handle<Code> code;
  if (sfi->TryGetCachedCode(isolate).ToHandle(&code)) {
    function->set_code(*code, kReleaseStore);
    JSFunction::EnsureFeedbackVector(function, is_compiled_scope);
    if (FLAG_trace_turbo_nci) CompilationCacheCode::TraceHit(sfi, code);
  }
}

}  // namespace

RUNTIME_FUNCTION(Runtime_CompileLazy) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

  Handle<SharedFunctionInfo> sfi(function->shared(), isolate);

#ifdef DEBUG
  if (FLAG_trace_lazy && !sfi->is_compiled()) {
    PrintF("[unoptimized: ");
    function->PrintName();
    PrintF("]\n");
  }
#endif

  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed(kStackSpaceRequiredForCompilation * KB)) {
    return isolate->StackOverflow();
  }
  IsCompiledScope is_compiled_scope;
  if (!Compiler::Compile(isolate, function, Compiler::KEEP_EXCEPTION,
                         &is_compiled_scope)) {
    return ReadOnlyRoots(isolate).exception();
  }
  if (sfi->may_have_cached_code()) {
    TryInstallNCICode(isolate, function, sfi, &is_compiled_scope);
  }
  DCHECK(function->is_compiled());
  return function->code();
}

RUNTIME_FUNCTION(Runtime_InstallBaselineCode) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  Handle<SharedFunctionInfo> sfi(function->shared(), isolate);
  DCHECK(sfi->HasBaselineData());
  IsCompiledScope is_compiled_scope(*sfi, isolate);
  DCHECK(!function->HasAvailableOptimizedCode());
  DCHECK(!function->HasOptimizationMarker());
  DCHECK(!function->has_feedback_vector());
  JSFunction::EnsureFeedbackVector(function, &is_compiled_scope);
  Code baseline_code = sfi->baseline_data().baseline_code();
  function->set_code(baseline_code);
  return baseline_code;
}

RUNTIME_FUNCTION(Runtime_TryInstallNCICode) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  DCHECK(function->is_compiled());
  Handle<SharedFunctionInfo> sfi(function->shared(), isolate);
  IsCompiledScope is_compiled_scope(*sfi, isolate);
  TryInstallNCICode(isolate, function, sfi, &is_compiled_scope);
  DCHECK(function->is_compiled());
  return function->code();
}

RUNTIME_FUNCTION(Runtime_CompileOptimized_Concurrent) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  return CompileOptimized(isolate, function, ConcurrencyMode::kConcurrent);
}

RUNTIME_FUNCTION(Runtime_CompileOptimized_NotConcurrent) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  return CompileOptimized(isolate, function, ConcurrencyMode::kNotConcurrent);
}

RUNTIME_FUNCTION(Runtime_FunctionFirstExecution) {
  HandleScope scope(isolate);
  StackLimitCheck check(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  DCHECK_EQ(function->feedback_vector().optimization_marker(),
            OptimizationMarker::kLogFirstExecution);
  DCHECK(FLAG_log_function_events);
  Handle<SharedFunctionInfo> sfi(function->shared(), isolate);
  Handle<String> name = SharedFunctionInfo::DebugName(sfi);
  LOG(isolate,
      FunctionEvent("first-execution", Script::cast(sfi->script()).id(), 0,
                    sfi->StartPosition(), sfi->EndPosition(), *name));
  function->feedback_vector().ClearOptimizationMarker();
  // Return the code to continue execution, we don't care at this point whether
  // this is for lazy compilation or has been eagerly complied.
  return function->code();
}

RUNTIME_FUNCTION(Runtime_HealOptimizedCodeSlot) {
  SealHandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

  DCHECK(function->shared().is_compiled());

  function->feedback_vector().EvictOptimizedCodeMarkedForDeoptimization(
      function->raw_feedback_cell(), function->shared(),
      "Runtime_HealOptimizedCodeSlot");
  return function->code();
}

RUNTIME_FUNCTION(Runtime_InstantiateAsmJs) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 4);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

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
  DCHECK(function->code() ==
         isolate->builtins()->builtin(Builtins::kInstantiateAsmJs));
  function->set_code(isolate->builtins()->builtin(Builtins::kCompileLazy));
  DCHECK(!isolate->has_pending_exception());
  return Smi::zero();
}

RUNTIME_FUNCTION(Runtime_NotifyDeoptimized) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  Deoptimizer* deoptimizer = Deoptimizer::Grab(isolate);
  DCHECK(CodeKindCanDeoptimize(deoptimizer->compiled_code()->kind()));
  DCHECK(deoptimizer->compiled_code()->is_turbofanned());
  DCHECK(AllowGarbageCollection::IsAllowed());
  DCHECK(isolate->context().is_null());

  TimerEventScope<TimerEventDeoptimizeCode> timer(isolate);
  TRACE_EVENT0("v8", "V8.DeoptimizeCode");
  Handle<JSFunction> function = deoptimizer->function();
  // For OSR the optimized code isn't installed on the function, so get the
  // code object from deoptimizer.
  Handle<Code> optimized_code = deoptimizer->compiled_code();
  DeoptimizeKind type = deoptimizer->deopt_kind();
  bool should_reuse_code = deoptimizer->should_reuse_code();

  // TODO(turbofan): We currently need the native context to materialize
  // the arguments object, but only to get to its map.
  isolate->set_context(deoptimizer->function()->native_context());

  // Make sure to materialize objects before causing any allocation.
  deoptimizer->MaterializeHeapObjects();
  delete deoptimizer;

  // Ensure the context register is updated for materialized objects.
  JavaScriptFrameIterator top_it(isolate);
  JavaScriptFrame* top_frame = top_it.frame();
  isolate->set_context(Context::cast(top_frame->context()));

  if (should_reuse_code) {
    optimized_code->increment_deoptimization_count();
    return ReadOnlyRoots(isolate).undefined_value();
  }

  // Invalidate the underlying optimized code on eager and soft deopts.
  if (type == DeoptimizeKind::kEager || type == DeoptimizeKind::kSoft) {
    Deoptimizer::DeoptimizeFunction(*function, *optimized_code);
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_ObserveNode) {
  // The %ObserveNode intrinsic only tracks the changes to an observed node in
  // code compiled by TurboFan.
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, obj, 0);
  return *obj;
}

static bool IsSuitableForOnStackReplacement(Isolate* isolate,
                                            Handle<JSFunction> function) {
  // Keep track of whether we've succeeded in optimizing.
  if (function->shared().optimization_disabled()) return false;
  // TODO(chromium:1031479): Currently, OSR triggering mechanism is tied to the
  // bytecode array. So, it might be possible to mark closure in one native
  // context and optimize a closure from a different native context. So check if
  // there is a feedback vector before OSRing. We don't expect this to happen
  // often.
  if (!function->has_feedback_vector()) return false;
  // If we are trying to do OSR when there are already optimized
  // activations of the function, it means (a) the function is directly or
  // indirectly recursive and (b) an optimized invocation has been
  // deoptimized so that we are currently in an unoptimized activation.
  // Check for optimized activations of this function.
  for (JavaScriptFrameIterator it(isolate); !it.done(); it.Advance()) {
    JavaScriptFrame* frame = it.frame();
    if (frame->is_optimized() && frame->function() == *function) return false;
  }

  return true;
}

namespace {

BytecodeOffset DetermineEntryAndDisarmOSRForUnoptimized(
    JavaScriptFrame* js_frame) {
  UnoptimizedFrame* frame = reinterpret_cast<UnoptimizedFrame*>(js_frame);

  // Note that the bytecode array active on the stack might be different from
  // the one installed on the function (e.g. patched by debugger). This however
  // is fine because we guarantee the layout to be in sync, hence any
  // BytecodeOffset representing the entry point will be valid for any copy of
  // the bytecode.
  Handle<BytecodeArray> bytecode(frame->GetBytecodeArray(), frame->isolate());

  DCHECK_IMPLIES(frame->is_interpreted(),
                 frame->LookupCode().is_interpreter_trampoline_builtin());
  DCHECK_IMPLIES(frame->is_baseline(),
                 frame->LookupCode().kind() == CodeKind::BASELINE);
  DCHECK(frame->is_unoptimized());
  DCHECK(frame->function().shared().HasBytecodeArray());

  // Reset the OSR loop nesting depth to disarm back edges.
  bytecode->set_osr_loop_nesting_level(0);

  // Return a BytecodeOffset representing the bytecode offset of the back
  // branch.
  return BytecodeOffset(frame->GetBytecodeOffset());
}

}  // namespace

RUNTIME_FUNCTION(Runtime_CompileForOnStackReplacement) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());

  // Only reachable when OST is enabled.
  CHECK(FLAG_use_osr);

  // Determine frame triggering OSR request.
  JavaScriptFrameIterator it(isolate);
  JavaScriptFrame* frame = it.frame();
  DCHECK(frame->is_unoptimized());

  // Determine the entry point for which this OSR request has been fired and
  // also disarm all back edges in the calling code to stop new requests.
  BytecodeOffset osr_offset = DetermineEntryAndDisarmOSRForUnoptimized(frame);
  DCHECK(!osr_offset.IsNone());

  MaybeHandle<Code> maybe_result;
  Handle<JSFunction> function(frame->function(), isolate);
  if (IsSuitableForOnStackReplacement(isolate, function)) {
    if (FLAG_trace_osr) {
      CodeTracer::Scope scope(isolate->GetCodeTracer());
      PrintF(scope.file(), "[OSR - Compiling: ");
      function->PrintName(scope.file());
      PrintF(scope.file(), " at OSR bytecode offset %d]\n", osr_offset.ToInt());
    }
    maybe_result =
        Compiler::GetOptimizedCodeForOSR(function, osr_offset, frame);

    // Possibly compile for NCI caching.
    if (!MaybeSpawnNativeContextIndependentCompilationJob(
            isolate, function,
            isolate->concurrent_recompilation_enabled()
                ? ConcurrencyMode::kConcurrent
                : ConcurrencyMode::kNotConcurrent)) {
      return Object();
    }
  }

  // Check whether we ended up with usable optimized code.
  Handle<Code> result;
  if (maybe_result.ToHandle(&result) &&
      CodeKindIsOptimizedJSFunction(result->kind())) {
    DeoptimizationData data =
        DeoptimizationData::cast(result->deoptimization_data());

    if (data.OsrPcOffset().value() >= 0) {
      DCHECK(BytecodeOffset(data.OsrBytecodeOffset().value()) == osr_offset);
      if (FLAG_trace_osr) {
        CodeTracer::Scope scope(isolate->GetCodeTracer());
        PrintF(scope.file(),
               "[OSR - Entry at OSR bytecode offset %d, offset %d in optimized "
               "code]\n",
               osr_offset.ToInt(), data.OsrPcOffset().value());
      }

      DCHECK(result->is_turbofanned());
      if (function->feedback_vector().invocation_count() <= 1 &&
          function->HasOptimizationMarker()) {
        // With lazy feedback allocation we may not have feedback for the
        // initial part of the function that was executed before we allocated a
        // feedback vector. Reset any optimization markers for such functions.
        //
        // TODO(mythria): Instead of resetting the optimization marker here we
        // should only mark a function for optimization if it has sufficient
        // feedback. We cannot do this currently since we OSR only after we mark
        // a function for optimization. We should instead change it to be based
        // based on number of ticks.
        DCHECK(!function->IsInOptimizationQueue());
        function->ClearOptimizationMarker();
      }
      // TODO(mythria): Once we have OSR code cache we may not need to mark
      // the function for non-concurrent compilation. We could arm the loops
      // early so the second execution uses the already compiled OSR code and
      // the optimization occurs concurrently off main thread.
      if (!function->HasAvailableOptimizedCode() &&
          function->feedback_vector().invocation_count() > 1) {
        // If we're not already optimized, set to optimize non-concurrently on
        // the next call, otherwise we'd run unoptimized once more and
        // potentially compile for OSR again.
        if (FLAG_trace_osr) {
          CodeTracer::Scope scope(isolate->GetCodeTracer());
          PrintF(scope.file(), "[OSR - Re-marking ");
          function->PrintName(scope.file());
          PrintF(scope.file(), " for non-concurrent optimization]\n");
        }
        function->SetOptimizationMarker(OptimizationMarker::kCompileOptimized);
      }
      return *result;
    }
  }

  // Failed.
  if (FLAG_trace_osr) {
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintF(scope.file(), "[OSR - Failed: ");
    function->PrintName(scope.file());
    PrintF(scope.file(), " at OSR bytecode offset %d]\n", osr_offset.ToInt());
  }

  if (!function->HasAttachedOptimizedCode()) {
    function->set_code(function->shared().GetCode(), kReleaseStore);
  }
  return Object();
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

  DCHECK(args[3].IsSmi());
  DCHECK(is_valid_language_mode(args.smi_at(3)));
  LanguageMode language_mode = static_cast<LanguageMode>(args.smi_at(3));
  DCHECK(args[4].IsSmi());
  Handle<SharedFunctionInfo> outer_info(args.at<JSFunction>(2)->shared(),
                                        isolate);
  return CompileGlobalEval(isolate, args.at<Object>(1), outer_info,
                           language_mode, args.smi_at(4), args.smi_at(5));
}

}  // namespace internal
}  // namespace v8

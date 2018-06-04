// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/asmjs/asm-js.h"
#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"
#include "src/compiler.h"
#include "src/deoptimizer.h"
#include "src/frames-inl.h"
#include "src/isolate-inl.h"
#include "src/messages.h"
#include "src/v8threads.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_CompileLazy) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

#ifdef DEBUG
  if (FLAG_trace_lazy && !function->shared()->is_compiled()) {
    PrintF("[unoptimized: ");
    function->PrintName();
    PrintF("]\n");
  }
#endif

  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed(kStackSpaceRequiredForCompilation * KB)) {
    return isolate->StackOverflow();
  }
  if (!Compiler::Compile(function, Compiler::KEEP_EXCEPTION)) {
    return isolate->heap()->exception();
  }
  DCHECK(function->is_compiled());
  return function->code();
}

RUNTIME_FUNCTION(Runtime_CompileOptimized_Concurrent) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed(kStackSpaceRequiredForCompilation * KB)) {
    return isolate->StackOverflow();
  }
  if (!Compiler::CompileOptimized(function, ConcurrencyMode::kConcurrent)) {
    return isolate->heap()->exception();
  }
  DCHECK(function->is_compiled());
  return function->code();
}

RUNTIME_FUNCTION(Runtime_FunctionFirstExecution) {
  HandleScope scope(isolate);
  StackLimitCheck check(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  DCHECK_EQ(function->feedback_vector()->optimization_marker(),
            OptimizationMarker::kLogFirstExecution);
  DCHECK(FLAG_log_function_events);
  Handle<SharedFunctionInfo> sfi(function->shared());
  LOG(isolate, FunctionEvent("first-execution", Script::cast(sfi->script()), -1,
                             0, sfi->StartPosition(), sfi->EndPosition(),
                             sfi->DebugName()));
  function->feedback_vector()->ClearOptimizationMarker();
  // Return the code to continue execution, we don't care at this point whether
  // this is for lazy compilation or has been eagerly complied.
  return function->code();
}

RUNTIME_FUNCTION(Runtime_CompileOptimized_NotConcurrent) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed(kStackSpaceRequiredForCompilation * KB)) {
    return isolate->StackOverflow();
  }
  if (!Compiler::CompileOptimized(function, ConcurrencyMode::kNotConcurrent)) {
    return isolate->heap()->exception();
  }
  DCHECK(function->is_compiled());
  return function->code();
}

RUNTIME_FUNCTION(Runtime_EvictOptimizedCodeSlot) {
  SealHandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

  DCHECK(function->shared()->is_compiled());

  function->feedback_vector()->EvictOptimizedCodeMarkedForDeoptimization(
      function->shared(), "Runtime_EvictOptimizedCodeSlot");
  return function->code();
}

RUNTIME_FUNCTION(Runtime_InstantiateAsmJs) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 4);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

  Handle<JSReceiver> stdlib;
  if (args[1]->IsJSReceiver()) {
    stdlib = args.at<JSReceiver>(1);
  }
  Handle<JSReceiver> foreign;
  if (args[2]->IsJSReceiver()) {
    foreign = args.at<JSReceiver>(2);
  }
  Handle<JSArrayBuffer> memory;
  if (args[3]->IsJSArrayBuffer()) {
    memory = args.at<JSArrayBuffer>(3);
  }
  if (function->shared()->HasAsmWasmData()) {
    Handle<SharedFunctionInfo> shared(function->shared());
    Handle<FixedArray> data(shared->asm_wasm_data());
    MaybeHandle<Object> result = AsmJs::InstantiateAsmWasm(
        isolate, shared, data, stdlib, foreign, memory);
    if (!result.is_null()) {
      return *result.ToHandleChecked();
    }
  }
  // Remove wasm data, mark as broken for asm->wasm, replace function code with
  // CompileLazy, and return a smi 0 to indicate failure.
  if (function->shared()->HasAsmWasmData()) {
    function->shared()->FlushCompiled();
  }
  function->shared()->set_is_asm_wasm_broken(true);
  DCHECK(function->code() ==
         isolate->builtins()->builtin(Builtins::kInstantiateAsmJs));
  function->set_code(isolate->builtins()->builtin(Builtins::kCompileLazy));
  return Smi::kZero;
}

RUNTIME_FUNCTION(Runtime_NotifyDeoptimized) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  Deoptimizer* deoptimizer = Deoptimizer::Grab(isolate);
  DCHECK(deoptimizer->compiled_code()->kind() == Code::OPTIMIZED_FUNCTION);
  DCHECK(deoptimizer->compiled_code()->is_turbofanned());
  DCHECK(AllowHeapAllocation::IsAllowed());
  DCHECK_NULL(isolate->context());

  TimerEventScope<TimerEventDeoptimizeCode> timer(isolate);
  TRACE_EVENT0("v8", "V8.DeoptimizeCode");
  Handle<JSFunction> function = deoptimizer->function();
  Deoptimizer::BailoutType type = deoptimizer->bailout_type();

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

  // Invalidate the underlying optimized code on non-lazy deopts.
  if (type != Deoptimizer::LAZY) {
    Deoptimizer::DeoptimizeFunction(*function);
  }

  return isolate->heap()->undefined_value();
}


static bool IsSuitableForOnStackReplacement(Isolate* isolate,
                                            Handle<JSFunction> function) {
  // Keep track of whether we've succeeded in optimizing.
  if (function->shared()->optimization_disabled()) return false;
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

BailoutId DetermineEntryAndDisarmOSRForInterpreter(JavaScriptFrame* frame) {
  InterpretedFrame* iframe = reinterpret_cast<InterpretedFrame*>(frame);

  // Note that the bytecode array active on the stack might be different from
  // the one installed on the function (e.g. patched by debugger). This however
  // is fine because we guarantee the layout to be in sync, hence any BailoutId
  // representing the entry point will be valid for any copy of the bytecode.
  Handle<BytecodeArray> bytecode(iframe->GetBytecodeArray());

  DCHECK(frame->LookupCode()->is_interpreter_trampoline_builtin());
  DCHECK(frame->function()->shared()->HasBytecodeArray());
  DCHECK(frame->is_interpreted());

  // Reset the OSR loop nesting depth to disarm back edges.
  bytecode->set_osr_loop_nesting_level(0);

  // Return a BailoutId representing the bytecode offset of the back branch.
  return BailoutId(iframe->GetBytecodeOffset());
}

}  // namespace

RUNTIME_FUNCTION(Runtime_CompileForOnStackReplacement) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

  // Only reachable when OST is enabled.
  CHECK(FLAG_use_osr);

  // Determine frame triggering OSR request.
  JavaScriptFrameIterator it(isolate);
  JavaScriptFrame* frame = it.frame();
  DCHECK_EQ(frame->function(), *function);
  DCHECK(frame->is_interpreted());

  // Determine the entry point for which this OSR request has been fired and
  // also disarm all back edges in the calling code to stop new requests.
  BailoutId ast_id = DetermineEntryAndDisarmOSRForInterpreter(frame);
  DCHECK(!ast_id.IsNone());

  MaybeHandle<Code> maybe_result;
  if (IsSuitableForOnStackReplacement(isolate, function)) {
    if (FLAG_trace_osr) {
      PrintF("[OSR - Compiling: ");
      function->PrintName();
      PrintF(" at AST id %d]\n", ast_id.ToInt());
    }
    maybe_result = Compiler::GetOptimizedCodeForOSR(function, ast_id, frame);
  }

  // Check whether we ended up with usable optimized code.
  Handle<Code> result;
  if (maybe_result.ToHandle(&result) &&
      result->kind() == Code::OPTIMIZED_FUNCTION) {
    DeoptimizationData* data =
        DeoptimizationData::cast(result->deoptimization_data());

    if (data->OsrPcOffset()->value() >= 0) {
      DCHECK(BailoutId(data->OsrBytecodeOffset()->value()) == ast_id);
      if (FLAG_trace_osr) {
        PrintF("[OSR - Entry at AST id %d, offset %d in optimized code]\n",
               ast_id.ToInt(), data->OsrPcOffset()->value());
      }

      DCHECK(result->is_turbofanned());
      if (!function->HasOptimizedCode()) {
        // If we're not already optimized, set to optimize non-concurrently on
        // the next call, otherwise we'd run unoptimized once more and
        // potentially compile for OSR again.
        if (FLAG_trace_osr) {
          PrintF("[OSR - Re-marking ");
          function->PrintName();
          PrintF(" for non-concurrent optimization]\n");
        }
        function->SetOptimizationMarker(OptimizationMarker::kCompileOptimized);
      }
      return *result;
    }
  }

  // Failed.
  if (FLAG_trace_osr) {
    PrintF("[OSR - Failed: ");
    function->PrintName();
    PrintF(" at AST id %d]\n", ast_id.ToInt());
  }

  if (!function->IsOptimized()) {
    function->set_code(function->shared()->GetCode());
  }
  return nullptr;
}

static Object* CompileGlobalEval(Isolate* isolate, Handle<String> source,
                                 Handle<SharedFunctionInfo> outer_info,
                                 LanguageMode language_mode,
                                 int eval_scope_position, int eval_position) {
  Handle<Context> context = Handle<Context>(isolate->context());
  Handle<Context> native_context = Handle<Context>(context->native_context());

  // Check if native context allows code generation from
  // strings. Throw an exception if it doesn't.
  if (native_context->allow_code_gen_from_strings()->IsFalse(isolate) &&
      !Compiler::CodeGenerationFromStringsAllowed(isolate, native_context,
                                                  source)) {
    Handle<Object> error_message =
        native_context->ErrorMessageForCodeGenerationFromStrings();
    Handle<Object> error;
    MaybeHandle<Object> maybe_error = isolate->factory()->NewEvalError(
        MessageTemplate::kCodeGenFromStrings, error_message);
    if (maybe_error.ToHandle(&error)) isolate->Throw(*error);
    return isolate->heap()->exception();
  }

  // Deal with a normal eval call with a string argument. Compile it
  // and return the compiled function bound in the local context.
  static const ParseRestriction restriction = NO_PARSE_RESTRICTION;
  Handle<JSFunction> compiled;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, compiled,
      Compiler::GetFunctionFromEval(source, outer_info, context, language_mode,
                                    restriction, kNoSourcePosition,
                                    eval_scope_position, eval_position),
      isolate->heap()->exception());
  return *compiled;
}


RUNTIME_FUNCTION(Runtime_ResolvePossiblyDirectEval) {
  HandleScope scope(isolate);
  DCHECK_EQ(6, args.length());

  Handle<Object> callee = args.at(0);

  // If "eval" didn't refer to the original GlobalEval, it's not a
  // direct call to eval.
  // (And even if it is, but the first argument isn't a string, just let
  // execution default to an indirect call to eval, which will also return
  // the first argument without doing anything).
  if (*callee != isolate->native_context()->global_eval_fun() ||
      !args[1]->IsString()) {
    return *callee;
  }

  DCHECK(args[3]->IsSmi());
  DCHECK(is_valid_language_mode(args.smi_at(3)));
  LanguageMode language_mode = static_cast<LanguageMode>(args.smi_at(3));
  DCHECK(args[4]->IsSmi());
  Handle<SharedFunctionInfo> outer_info(args.at<JSFunction>(2)->shared(),
                                        isolate);
  return CompileGlobalEval(isolate, args.at<String>(1), outer_info,
                           language_mode, args.smi_at(4), args.smi_at(5));
}
}  // namespace internal
}  // namespace v8

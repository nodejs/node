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
#include "src/full-codegen/full-codegen.h"
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
  if (check.JsHasOverflowed(1 * KB)) return isolate->StackOverflow();
  if (!Compiler::Compile(function, Compiler::KEEP_EXCEPTION)) {
    return isolate->heap()->exception();
  }
  DCHECK(function->is_compiled());
  return function->code();
}

RUNTIME_FUNCTION(Runtime_CompileBaseline) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed(1 * KB)) return isolate->StackOverflow();
  if (!Compiler::CompileBaseline(function)) {
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
  if (check.JsHasOverflowed(1 * KB)) return isolate->StackOverflow();
  if (!Compiler::CompileOptimized(function, Compiler::CONCURRENT)) {
    return isolate->heap()->exception();
  }
  DCHECK(function->is_compiled());
  return function->code();
}


RUNTIME_FUNCTION(Runtime_CompileOptimized_NotConcurrent) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed(1 * KB)) return isolate->StackOverflow();
  if (!Compiler::CompileOptimized(function, Compiler::NOT_CONCURRENT)) {
    return isolate->heap()->exception();
  }
  DCHECK(function->is_compiled());
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
  Handle<JSObject> foreign;
  if (args[2]->IsJSObject()) {
    foreign = args.at<JSObject>(2);
  }
  Handle<JSArrayBuffer> memory;
  if (args[3]->IsJSArrayBuffer()) {
    memory = args.at<JSArrayBuffer>(3);
  }
  if (function->shared()->HasAsmWasmData() &&
      AsmJs::IsStdlibValid(isolate, handle(function->shared()->asm_wasm_data()),
                           stdlib)) {
    MaybeHandle<Object> result;
    result = AsmJs::InstantiateAsmWasm(
        isolate, handle(function->shared()->asm_wasm_data()), memory, foreign);
    if (!result.is_null()) {
      return *result.ToHandleChecked();
    }
  }
  // Remove wasm data, mark as broken for asm->wasm,
  // replace code with CompileLazy, and return a smi 0 to indicate failure.
  if (function->shared()->HasAsmWasmData()) {
    function->shared()->ClearAsmWasmData();
  }
  function->shared()->set_is_asm_wasm_broken(true);
  DCHECK(function->code() ==
         isolate->builtins()->builtin(Builtins::kInstantiateAsmJs));
  function->ReplaceCode(isolate->builtins()->builtin(Builtins::kCompileLazy));
  if (function->shared()->code() ==
      isolate->builtins()->builtin(Builtins::kInstantiateAsmJs)) {
    function->shared()->ReplaceCode(
        isolate->builtins()->builtin(Builtins::kCompileLazy));
  }
  return Smi::kZero;
}

RUNTIME_FUNCTION(Runtime_NotifyStubFailure) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  Deoptimizer* deoptimizer = Deoptimizer::Grab(isolate);
  DCHECK(AllowHeapAllocation::IsAllowed());
  delete deoptimizer;
  return isolate->heap()->undefined_value();
}

class ActivationsFinder : public ThreadVisitor {
 public:
  Code* code_;
  bool has_code_activations_;

  explicit ActivationsFinder(Code* code)
      : code_(code), has_code_activations_(false) {}

  void VisitThread(Isolate* isolate, ThreadLocalTop* top) {
    JavaScriptFrameIterator it(isolate, top);
    VisitFrames(&it);
  }

  void VisitFrames(JavaScriptFrameIterator* it) {
    for (; !it->done(); it->Advance()) {
      JavaScriptFrame* frame = it->frame();
      if (code_->contains(frame->pc())) has_code_activations_ = true;
    }
  }
};


RUNTIME_FUNCTION(Runtime_NotifyDeoptimized) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_SMI_ARG_CHECKED(type_arg, 0);
  Deoptimizer::BailoutType type =
      static_cast<Deoptimizer::BailoutType>(type_arg);
  Deoptimizer* deoptimizer = Deoptimizer::Grab(isolate);
  DCHECK(AllowHeapAllocation::IsAllowed());
  TimerEventScope<TimerEventDeoptimizeCode> timer(isolate);
  TRACE_EVENT0("v8", "V8.DeoptimizeCode");

  Handle<JSFunction> function = deoptimizer->function();
  Handle<Code> optimized_code = deoptimizer->compiled_code();

  DCHECK(optimized_code->kind() == Code::OPTIMIZED_FUNCTION);
  DCHECK(type == deoptimizer->bailout_type());
  DCHECK_NULL(isolate->context());

  // TODO(turbofan): For Crankshaft we restore the context before objects are
  // being materialized, because it never de-materializes the context but it
  // requires a context to materialize arguments objects. This is specific to
  // Crankshaft and can be removed once only TurboFan goes through here.
  if (!optimized_code->is_turbofanned()) {
    JavaScriptFrameIterator top_it(isolate);
    JavaScriptFrame* top_frame = top_it.frame();
    isolate->set_context(Context::cast(top_frame->context()));
  }

  // Make sure to materialize objects before causing any allocation.
  JavaScriptFrameIterator it(isolate);
  deoptimizer->MaterializeHeapObjects(&it);
  delete deoptimizer;

  // Ensure the context register is updated for materialized objects.
  if (optimized_code->is_turbofanned()) {
    JavaScriptFrameIterator top_it(isolate);
    JavaScriptFrame* top_frame = top_it.frame();
    isolate->set_context(Context::cast(top_frame->context()));
  }

  if (type == Deoptimizer::LAZY) {
    return isolate->heap()->undefined_value();
  }

  // Search for other activations of the same optimized code.
  // At this point {it} is at the topmost frame of all the frames materialized
  // by the deoptimizer. Note that this frame does not necessarily represent
  // an activation of {function} because of potential inlined tail-calls.
  ActivationsFinder activations_finder(*optimized_code);
  activations_finder.VisitFrames(&it);
  isolate->thread_manager()->IterateArchivedThreads(&activations_finder);

  if (!activations_finder.has_code_activations_) {
    if (function->code() == *optimized_code) {
      if (FLAG_trace_deopt) {
        PrintF("[removing optimized code for: ");
        function->PrintName();
        PrintF("]\n");
      }
      function->ReplaceCode(function->shared()->code());
    }
    // Evict optimized code for this function from the cache so that it
    // doesn't get used for new closures.
    function->shared()->EvictFromOptimizedCodeMap(*optimized_code,
                                                  "notify deoptimized");
  } else {
    // TODO(titzer): we should probably do DeoptimizeCodeList(code)
    // unconditionally if the code is not already marked for deoptimization.
    // If there is an index by shared function info, all the better.
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

BailoutId DetermineEntryAndDisarmOSRForBaseline(JavaScriptFrame* frame) {
  Handle<Code> caller_code(frame->function()->shared()->code());

  // Passing the PC in the JavaScript frame from the caller directly is
  // not GC safe, so we walk the stack to get it.
  if (!caller_code->contains(frame->pc())) {
    // Code on the stack may not be the code object referenced by the shared
    // function info.  It may have been replaced to include deoptimization data.
    caller_code = Handle<Code>(frame->LookupCode());
  }

  DCHECK_EQ(frame->LookupCode(), *caller_code);
  DCHECK_EQ(Code::FUNCTION, caller_code->kind());
  DCHECK(caller_code->contains(frame->pc()));

  // Revert the patched back edge table, regardless of whether OSR succeeds.
  BackEdgeTable::Revert(frame->isolate(), *caller_code);

  // Return a BailoutId representing an AST id of the {IterationStatement}.
  uint32_t pc_offset =
      static_cast<uint32_t>(frame->pc() - caller_code->instruction_start());
  return caller_code->TranslatePcOffsetToAstId(pc_offset);
}

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
  DCHECK(FLAG_ignition_osr);

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

  // We're not prepared to handle a function with arguments object.
  DCHECK(!function->shared()->uses_arguments());

  // Only reachable when OST is enabled.
  CHECK(FLAG_use_osr);

  // Determine frame triggering OSR request.
  JavaScriptFrameIterator it(isolate);
  JavaScriptFrame* frame = it.frame();
  DCHECK_EQ(frame->function(), *function);

  // Determine the entry point for which this OSR request has been fired and
  // also disarm all back edges in the calling code to stop new requests.
  BailoutId ast_id = frame->is_interpreted()
                         ? DetermineEntryAndDisarmOSRForInterpreter(frame)
                         : DetermineEntryAndDisarmOSRForBaseline(frame);
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
    DeoptimizationInputData* data =
        DeoptimizationInputData::cast(result->deoptimization_data());

    if (data->OsrPcOffset()->value() >= 0) {
      DCHECK(BailoutId(data->OsrAstId()->value()) == ast_id);
      if (FLAG_trace_osr) {
        PrintF("[OSR - Entry at AST id %d, offset %d in optimized code]\n",
               ast_id.ToInt(), data->OsrPcOffset()->value());
      }
      // TODO(titzer): this is a massive hack to make the deopt counts
      // match. Fix heuristics for reenabling optimizations!
      function->shared()->increment_deopt_count();

      if (result->is_turbofanned()) {
        // When we're waiting for concurrent optimization, set to compile on
        // the next call - otherwise we'd run unoptimized once more
        // and potentially compile for OSR another time as well.
        if (function->IsMarkedForConcurrentOptimization()) {
          if (FLAG_trace_osr) {
            PrintF("[OSR - Re-marking ");
            function->PrintName();
            PrintF(" for non-concurrent optimization]\n");
          }
          function->ReplaceCode(
              isolate->builtins()->builtin(Builtins::kCompileOptimized));
        }
      } else {
        // Crankshafted OSR code can be installed into the function.
        function->ReplaceCode(*result);
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
    function->ReplaceCode(function->shared()->code());
  }
  return NULL;
}


RUNTIME_FUNCTION(Runtime_TryInstallOptimizedCode) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

  // First check if this is a real stack overflow.
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed()) {
    SealHandleScope shs(isolate);
    return isolate->StackOverflow();
  }

  isolate->optimizing_compile_dispatcher()->InstallOptimizedFunctions();
  return (function->IsOptimized()) ? function->code()
                                   : function->shared()->code();
}


bool CodeGenerationFromStringsAllowed(Isolate* isolate,
                                      Handle<Context> context) {
  DCHECK(context->allow_code_gen_from_strings()->IsFalse(isolate));
  // Check with callback if set.
  AllowCodeGenerationFromStringsCallback callback =
      isolate->allow_code_gen_callback();
  if (callback == NULL) {
    // No callback set and code generation disallowed.
    return false;
  } else {
    // Callback set. Let it decide if code generation is allowed.
    VMState<EXTERNAL> state(isolate);
    return callback(v8::Utils::ToLocal(context));
  }
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
      !CodeGenerationFromStringsAllowed(isolate, native_context)) {
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
      isolate, compiled, Compiler::GetFunctionFromEval(
                             source, outer_info, context, language_mode,
                             restriction, eval_scope_position, eval_position),
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

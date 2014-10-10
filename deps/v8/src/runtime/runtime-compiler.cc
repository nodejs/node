// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/arguments.h"
#include "src/compiler.h"
#include "src/deoptimizer.h"
#include "src/frames.h"
#include "src/full-codegen.h"
#include "src/isolate.h"
#include "src/isolate-inl.h"
#include "src/runtime/runtime.h"
#include "src/runtime/runtime-utils.h"
#include "src/v8threads.h"
#include "src/vm-state.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_CompileLazy) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
#ifdef DEBUG
  if (FLAG_trace_lazy && !function->shared()->is_compiled()) {
    PrintF("[unoptimized: ");
    function->PrintName();
    PrintF("]\n");
  }
#endif

  // Compile the target function.
  DCHECK(function->shared()->allows_lazy_compilation());

  Handle<Code> code;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, code,
                                     Compiler::GetLazyCode(function));
  DCHECK(code->kind() == Code::FUNCTION ||
         code->kind() == Code::OPTIMIZED_FUNCTION);
  function->ReplaceCode(*code);
  return *code;
}


RUNTIME_FUNCTION(Runtime_CompileOptimized) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CONVERT_BOOLEAN_ARG_CHECKED(concurrent, 1);

  Handle<Code> unoptimized(function->shared()->code());
  if (!isolate->use_crankshaft() ||
      function->shared()->optimization_disabled() ||
      isolate->DebuggerHasBreakPoints()) {
    // If the function is not optimizable or debugger is active continue
    // using the code from the full compiler.
    if (FLAG_trace_opt) {
      PrintF("[failed to optimize ");
      function->PrintName();
      PrintF(": is code optimizable: %s, is debugger enabled: %s]\n",
             function->shared()->optimization_disabled() ? "F" : "T",
             isolate->DebuggerHasBreakPoints() ? "T" : "F");
    }
    function->ReplaceCode(*unoptimized);
    return function->code();
  }

  Compiler::ConcurrencyMode mode =
      concurrent ? Compiler::CONCURRENT : Compiler::NOT_CONCURRENT;
  Handle<Code> code;
  if (Compiler::GetOptimizedCode(function, unoptimized, mode).ToHandle(&code)) {
    function->ReplaceCode(*code);
  } else {
    function->ReplaceCode(function->shared()->code());
  }

  DCHECK(function->code()->kind() == Code::FUNCTION ||
         function->code()->kind() == Code::OPTIMIZED_FUNCTION ||
         function->IsInOptimizationQueue());
  return function->code();
}


RUNTIME_FUNCTION(Runtime_NotifyStubFailure) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
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
  DCHECK(args.length() == 1);
  CONVERT_SMI_ARG_CHECKED(type_arg, 0);
  Deoptimizer::BailoutType type =
      static_cast<Deoptimizer::BailoutType>(type_arg);
  Deoptimizer* deoptimizer = Deoptimizer::Grab(isolate);
  DCHECK(AllowHeapAllocation::IsAllowed());

  Handle<JSFunction> function = deoptimizer->function();
  Handle<Code> optimized_code = deoptimizer->compiled_code();

  DCHECK(optimized_code->kind() == Code::OPTIMIZED_FUNCTION);
  DCHECK(type == deoptimizer->bailout_type());

  // Make sure to materialize objects before causing any allocation.
  JavaScriptFrameIterator it(isolate);
  deoptimizer->MaterializeHeapObjects(&it);
  delete deoptimizer;

  JavaScriptFrame* frame = it.frame();
  RUNTIME_ASSERT(frame->function()->IsJSFunction());
  DCHECK(frame->function() == *function);

  // Avoid doing too much work when running with --always-opt and keep
  // the optimized code around.
  if (FLAG_always_opt || type == Deoptimizer::LAZY) {
    return isolate->heap()->undefined_value();
  }

  // Search for other activations of the same function and code.
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
      // Evict optimized code for this function from the cache so that it
      // doesn't get used for new closures.
      function->shared()->EvictFromOptimizedCodeMap(*optimized_code,
                                                    "notify deoptimized");
    }
  } else {
    // TODO(titzer): we should probably do DeoptimizeCodeList(code)
    // unconditionally if the code is not already marked for deoptimization.
    // If there is an index by shared function info, all the better.
    Deoptimizer::DeoptimizeFunction(*function);
  }

  return isolate->heap()->undefined_value();
}


static bool IsSuitableForOnStackReplacement(Isolate* isolate,
                                            Handle<JSFunction> function,
                                            Handle<Code> current_code) {
  // Keep track of whether we've succeeded in optimizing.
  if (!isolate->use_crankshaft() || !current_code->optimizable()) return false;
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


RUNTIME_FUNCTION(Runtime_CompileForOnStackReplacement) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  Handle<Code> caller_code(function->shared()->code());

  // We're not prepared to handle a function with arguments object.
  DCHECK(!function->shared()->uses_arguments());

  RUNTIME_ASSERT(FLAG_use_osr);

  // Passing the PC in the javascript frame from the caller directly is
  // not GC safe, so we walk the stack to get it.
  JavaScriptFrameIterator it(isolate);
  JavaScriptFrame* frame = it.frame();
  if (!caller_code->contains(frame->pc())) {
    // Code on the stack may not be the code object referenced by the shared
    // function info.  It may have been replaced to include deoptimization data.
    caller_code = Handle<Code>(frame->LookupCode());
  }

  uint32_t pc_offset =
      static_cast<uint32_t>(frame->pc() - caller_code->instruction_start());

#ifdef DEBUG
  DCHECK_EQ(frame->function(), *function);
  DCHECK_EQ(frame->LookupCode(), *caller_code);
  DCHECK(caller_code->contains(frame->pc()));
#endif  // DEBUG


  BailoutId ast_id = caller_code->TranslatePcOffsetToAstId(pc_offset);
  DCHECK(!ast_id.IsNone());

  Compiler::ConcurrencyMode mode =
      isolate->concurrent_osr_enabled() &&
              (function->shared()->ast_node_count() > 512)
          ? Compiler::CONCURRENT
          : Compiler::NOT_CONCURRENT;
  Handle<Code> result = Handle<Code>::null();

  OptimizedCompileJob* job = NULL;
  if (mode == Compiler::CONCURRENT) {
    // Gate the OSR entry with a stack check.
    BackEdgeTable::AddStackCheck(caller_code, pc_offset);
    // Poll already queued compilation jobs.
    OptimizingCompilerThread* thread = isolate->optimizing_compiler_thread();
    if (thread->IsQueuedForOSR(function, ast_id)) {
      if (FLAG_trace_osr) {
        PrintF("[OSR - Still waiting for queued: ");
        function->PrintName();
        PrintF(" at AST id %d]\n", ast_id.ToInt());
      }
      return NULL;
    }

    job = thread->FindReadyOSRCandidate(function, ast_id);
  }

  if (job != NULL) {
    if (FLAG_trace_osr) {
      PrintF("[OSR - Found ready: ");
      function->PrintName();
      PrintF(" at AST id %d]\n", ast_id.ToInt());
    }
    result = Compiler::GetConcurrentlyOptimizedCode(job);
  } else if (IsSuitableForOnStackReplacement(isolate, function, caller_code)) {
    if (FLAG_trace_osr) {
      PrintF("[OSR - Compiling: ");
      function->PrintName();
      PrintF(" at AST id %d]\n", ast_id.ToInt());
    }
    MaybeHandle<Code> maybe_result =
        Compiler::GetOptimizedCode(function, caller_code, mode, ast_id);
    if (maybe_result.ToHandle(&result) &&
        result.is_identical_to(isolate->builtins()->InOptimizationQueue())) {
      // Optimization is queued.  Return to check later.
      return NULL;
    }
  }

  // Revert the patched back edge table, regardless of whether OSR succeeds.
  BackEdgeTable::Revert(isolate, *caller_code);

  // Check whether we ended up with usable optimized code.
  if (!result.is_null() && result->kind() == Code::OPTIMIZED_FUNCTION) {
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

      // TODO(titzer): Do not install code into the function.
      function->ReplaceCode(*result);
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
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

  // First check if this is a real stack overflow.
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed()) {
    SealHandleScope shs(isolate);
    return isolate->StackOverflow();
  }

  isolate->optimizing_compiler_thread()->InstallOptimizedFunctions();
  return (function->IsOptimized()) ? function->code()
                                   : function->shared()->code();
}


bool CodeGenerationFromStringsAllowed(Isolate* isolate,
                                      Handle<Context> context) {
  DCHECK(context->allow_code_gen_from_strings()->IsFalse());
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


RUNTIME_FUNCTION(Runtime_CompileString) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(String, source, 0);
  CONVERT_BOOLEAN_ARG_CHECKED(function_literal_only, 1);

  // Extract native context.
  Handle<Context> context(isolate->native_context());

  // Check if native context allows code generation from
  // strings. Throw an exception if it doesn't.
  if (context->allow_code_gen_from_strings()->IsFalse() &&
      !CodeGenerationFromStringsAllowed(isolate, context)) {
    Handle<Object> error_message =
        context->ErrorMessageForCodeGenerationFromStrings();
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewEvalError("code_gen_from_strings",
                              HandleVector<Object>(&error_message, 1)));
  }

  // Compile source string in the native context.
  ParseRestriction restriction = function_literal_only
                                     ? ONLY_SINGLE_FUNCTION_LITERAL
                                     : NO_PARSE_RESTRICTION;
  Handle<JSFunction> fun;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, fun,
      Compiler::GetFunctionFromEval(source, context, SLOPPY, restriction,
                                    RelocInfo::kNoPosition));
  return *fun;
}


static ObjectPair CompileGlobalEval(Isolate* isolate, Handle<String> source,
                                    Handle<Object> receiver,
                                    StrictMode strict_mode,
                                    int scope_position) {
  Handle<Context> context = Handle<Context>(isolate->context());
  Handle<Context> native_context = Handle<Context>(context->native_context());

  // Check if native context allows code generation from
  // strings. Throw an exception if it doesn't.
  if (native_context->allow_code_gen_from_strings()->IsFalse() &&
      !CodeGenerationFromStringsAllowed(isolate, native_context)) {
    Handle<Object> error_message =
        native_context->ErrorMessageForCodeGenerationFromStrings();
    Handle<Object> error;
    MaybeHandle<Object> maybe_error = isolate->factory()->NewEvalError(
        "code_gen_from_strings", HandleVector<Object>(&error_message, 1));
    if (maybe_error.ToHandle(&error)) isolate->Throw(*error);
    return MakePair(isolate->heap()->exception(), NULL);
  }

  // Deal with a normal eval call with a string argument. Compile it
  // and return the compiled function bound in the local context.
  static const ParseRestriction restriction = NO_PARSE_RESTRICTION;
  Handle<JSFunction> compiled;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, compiled,
      Compiler::GetFunctionFromEval(source, context, strict_mode, restriction,
                                    scope_position),
      MakePair(isolate->heap()->exception(), NULL));
  return MakePair(*compiled, *receiver);
}


RUNTIME_FUNCTION_RETURN_PAIR(Runtime_ResolvePossiblyDirectEval) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 5);

  Handle<Object> callee = args.at<Object>(0);

  // If "eval" didn't refer to the original GlobalEval, it's not a
  // direct call to eval.
  // (And even if it is, but the first argument isn't a string, just let
  // execution default to an indirect call to eval, which will also return
  // the first argument without doing anything).
  if (*callee != isolate->native_context()->global_eval_fun() ||
      !args[1]->IsString()) {
    return MakePair(*callee, isolate->heap()->undefined_value());
  }

  DCHECK(args[3]->IsSmi());
  DCHECK(args.smi_at(3) == SLOPPY || args.smi_at(3) == STRICT);
  StrictMode strict_mode = static_cast<StrictMode>(args.smi_at(3));
  DCHECK(args[4]->IsSmi());
  return CompileGlobalEval(isolate, args.at<String>(1), args.at<Object>(2),
                           strict_mode, args.smi_at(4));
}
}
}  // namespace v8::internal

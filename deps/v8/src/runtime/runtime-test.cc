// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-inl.h"
#include "src/base/platform/mutex.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/compiler.h"
#include "src/codegen/pending-optimization-table.h"
#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"
#include "src/debug/debug-evaluate.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/arguments-inl.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/protectors-inl.h"
#include "src/execution/runtime-profiler.h"
#include "src/heap/heap-inl.h"  // For ToBoolean. TODO(jkummerow): Drop.
#include "src/heap/heap-write-barrier-inl.h"
#include "src/ic/stub-cache.h"
#include "src/logging/counters.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-function-inl.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/smi.h"
#include "src/regexp/regexp.h"
#include "src/runtime/runtime-utils.h"
#include "src/snapshot/snapshot.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-engine.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

namespace {
V8_WARN_UNUSED_RESULT Object CrashUnlessFuzzing(Isolate* isolate) {
  CHECK(FLAG_fuzzing);
  return ReadOnlyRoots(isolate).undefined_value();
}

// Assert that the given argument is a number within the Int32 range
// and convert it to int32_t.  If the argument is not an Int32 we crash if not
// in fuzzing mode.
#define CONVERT_INT32_ARG_FUZZ_SAFE(name, index)                   \
  if (!args[index].IsNumber()) return CrashUnlessFuzzing(isolate); \
  int32_t name = 0;                                                \
  if (!args[index].ToInt32(&name)) return CrashUnlessFuzzing(isolate);

// Cast the given object to a boolean and store it in a variable with
// the given name.  If the object is not a boolean we crash if not in
// fuzzing mode.
#define CONVERT_BOOLEAN_ARG_FUZZ_SAFE(name, index)                  \
  if (!args[index].IsBoolean()) return CrashUnlessFuzzing(isolate); \
  bool name = args[index].IsTrue(isolate);

}  // namespace

RUNTIME_FUNCTION(Runtime_ClearMegamorphicStubCache) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  isolate->load_stub_cache()->Clear();
  isolate->store_stub_cache()->Clear();
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_ConstructDouble) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_NUMBER_CHECKED(uint32_t, hi, Uint32, args[0]);
  CONVERT_NUMBER_CHECKED(uint32_t, lo, Uint32, args[1]);
  uint64_t result = (static_cast<uint64_t>(hi) << 32) | lo;
  return *isolate->factory()->NewNumber(uint64_to_double(result));
}

RUNTIME_FUNCTION(Runtime_ConstructConsString) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, left, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, right, 1);

  CHECK(left->IsOneByteRepresentation());
  CHECK(right->IsOneByteRepresentation());

  const bool kIsOneByte = true;
  const int length = left->length() + right->length();
  return *isolate->factory()->NewConsString(left, right, length, kIsOneByte);
}

RUNTIME_FUNCTION(Runtime_ConstructSlicedString) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, string, 0);
  CONVERT_ARG_HANDLE_CHECKED(Smi, index, 1);

  CHECK(string->IsOneByteRepresentation());
  CHECK_LT(index->value(), string->length());

  Handle<String> sliced_string = isolate->factory()->NewSubString(
      string, index->value(), string->length());
  CHECK(sliced_string->IsSlicedString());
  return *sliced_string;
}

RUNTIME_FUNCTION(Runtime_DeoptimizeFunction) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(Object, function_object, 0);
  if (!function_object->IsJSFunction()) return CrashUnlessFuzzing(isolate);
  Handle<JSFunction> function = Handle<JSFunction>::cast(function_object);

  if (function->HasAttachedOptimizedCode()) {
    Deoptimizer::DeoptimizeFunction(*function);
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_DeoptimizeNow) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());

  Handle<JSFunction> function;

  // Find the JavaScript function on the top of the stack.
  JavaScriptFrameIterator it(isolate);
  if (!it.done()) function = handle(it.frame()->function(), isolate);
  if (function.is_null()) return CrashUnlessFuzzing(isolate);

  if (function->HasAttachedOptimizedCode()) {
    Deoptimizer::DeoptimizeFunction(*function);
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_RunningInSimulator) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
#if defined(USE_SIMULATOR)
  return ReadOnlyRoots(isolate).true_value();
#else
  return ReadOnlyRoots(isolate).false_value();
#endif
}

RUNTIME_FUNCTION(Runtime_RuntimeEvaluateREPL) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, source, 0);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      DebugEvaluate::Global(isolate, source,
                            debug::EvaluateGlobalMode::kDefault,
                            REPLMode::kYes));

  return *result;
}

RUNTIME_FUNCTION(Runtime_ICsAreEnabled) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(FLAG_use_ic);
}

RUNTIME_FUNCTION(Runtime_IsConcurrentRecompilationSupported) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(
      isolate->concurrent_recompilation_enabled());
}

RUNTIME_FUNCTION(Runtime_DynamicCheckMapsEnabled) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(FLAG_turbo_dynamic_map_checks);
}

RUNTIME_FUNCTION(Runtime_IsTopTierTurboprop) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(FLAG_turboprop_as_toptier);
}

RUNTIME_FUNCTION(Runtime_IsMidTierTurboprop) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(FLAG_turboprop &&
                                    !FLAG_turboprop_as_toptier);
}

namespace {

enum class TierupKind { kTierupBytecode, kTierupBytecodeOrMidTier };

Object OptimizeFunctionOnNextCall(RuntimeArguments& args, Isolate* isolate,
                                  TierupKind tierup_kind) {
  if (args.length() != 1 && args.length() != 2) {
    return CrashUnlessFuzzing(isolate);
  }

  CONVERT_ARG_HANDLE_CHECKED(Object, function_object, 0);
  if (!function_object->IsJSFunction()) return CrashUnlessFuzzing(isolate);
  Handle<JSFunction> function = Handle<JSFunction>::cast(function_object);

  // The following conditions were lifted (in part) from the DCHECK inside
  // JSFunction::MarkForOptimization().

  if (!function->shared().allows_lazy_compilation()) {
    return CrashUnlessFuzzing(isolate);
  }

  // If function isn't compiled, compile it now.
  IsCompiledScope is_compiled_scope(
      function->shared().is_compiled_scope(isolate));
  if (!is_compiled_scope.is_compiled() &&
      !Compiler::Compile(isolate, function, Compiler::CLEAR_EXCEPTION,
                         &is_compiled_scope)) {
    return CrashUnlessFuzzing(isolate);
  }

  if (!FLAG_opt) return ReadOnlyRoots(isolate).undefined_value();

  if (function->shared().optimization_disabled() &&
      function->shared().disable_optimization_reason() ==
          BailoutReason::kNeverOptimize) {
    return CrashUnlessFuzzing(isolate);
  }

#if V8_ENABLE_WEBASSEMBLY
  if (function->shared().HasAsmWasmData()) return CrashUnlessFuzzing(isolate);
#endif  // V8_ENABLE_WEBASSEMBLY

  if (FLAG_testing_d8_test_runner) {
    PendingOptimizationTable::MarkedForOptimization(isolate, function);
  }

  CodeKind kind = CodeKindForTopTier();
  if ((tierup_kind == TierupKind::kTierupBytecode &&
       function->HasAvailableOptimizedCode()) ||
      function->HasAvailableCodeKind(kind)) {
    DCHECK(function->HasAttachedOptimizedCode() ||
           function->ChecksOptimizationMarker());
    if (FLAG_testing_d8_test_runner) {
      PendingOptimizationTable::FunctionWasOptimized(isolate, function);
    }
    return ReadOnlyRoots(isolate).undefined_value();
  }

  ConcurrencyMode concurrency_mode = ConcurrencyMode::kNotConcurrent;
  if (args.length() == 2) {
    CONVERT_ARG_HANDLE_CHECKED(Object, type, 1);
    if (!type->IsString()) return CrashUnlessFuzzing(isolate);
    if (Handle<String>::cast(type)->IsOneByteEqualTo(
            StaticCharVector("concurrent")) &&
        isolate->concurrent_recompilation_enabled()) {
      concurrency_mode = ConcurrencyMode::kConcurrent;
    }
  }
  if (FLAG_trace_opt) {
    PrintF("[manually marking ");
    function->ShortPrint();
    PrintF(" for %s optimization]\n",
           concurrency_mode == ConcurrencyMode::kConcurrent ? "concurrent"
                                                            : "non-concurrent");
  }

  // This function may not have been lazily compiled yet, even though its shared
  // function has.
  if (!function->is_compiled()) {
    DCHECK(function->shared().IsInterpreted());
    function->set_code(*BUILTIN_CODE(isolate, InterpreterEntryTrampoline));
  }

  JSFunction::EnsureFeedbackVector(function, &is_compiled_scope);
  function->MarkForOptimization(concurrency_mode);

  return ReadOnlyRoots(isolate).undefined_value();
}

bool EnsureFeedbackVector(Isolate* isolate, Handle<JSFunction> function) {
  // Check function allows lazy compilation.
  if (!function->shared().allows_lazy_compilation()) return false;

  if (function->has_feedback_vector()) return true;

  // If function isn't compiled, compile it now.
  IsCompiledScope is_compiled_scope(
      function->shared().is_compiled_scope(function->GetIsolate()));
  // If the JSFunction isn't compiled but it has a initialized feedback cell
  // then no need to compile. CompileLazy builtin would handle these cases by
  // installing the code from SFI. Calling compile here may cause another
  // optimization if FLAG_always_opt is set.
  bool needs_compilation =
      !function->is_compiled() && !function->has_closure_feedback_cell_array();
  if (needs_compilation &&
      !Compiler::Compile(isolate, function, Compiler::CLEAR_EXCEPTION,
                         &is_compiled_scope)) {
    return false;
  }

  // Ensure function has a feedback vector to hold type feedback for
  // optimization.
  JSFunction::EnsureFeedbackVector(function, &is_compiled_scope);
  return true;
}

}  // namespace

RUNTIME_FUNCTION(Runtime_CompileBaseline) {
  HandleScope scope(isolate);
  if (args.length() != 1) {
    return CrashUnlessFuzzing(isolate);
  }
  CONVERT_ARG_HANDLE_CHECKED(Object, function_object, 0);
  if (!function_object->IsJSFunction()) return CrashUnlessFuzzing(isolate);
  Handle<JSFunction> function = Handle<JSFunction>::cast(function_object);

  IsCompiledScope is_compiled_scope =
      function->shared(isolate).is_compiled_scope(isolate);

  if (!function->shared(isolate).IsUserJavaScript()) {
    return CrashUnlessFuzzing(isolate);
  }

  // First compile the bytecode, if we have to.
  if (!is_compiled_scope.is_compiled() &&
      !Compiler::Compile(isolate, function, Compiler::KEEP_EXCEPTION,
                         &is_compiled_scope)) {
    return CrashUnlessFuzzing(isolate);
  }

  if (!Compiler::CompileBaseline(isolate, function, Compiler::KEEP_EXCEPTION,
                                 &is_compiled_scope)) {
    return CrashUnlessFuzzing(isolate);
  }

  return *function;
}

RUNTIME_FUNCTION(Runtime_OptimizeFunctionOnNextCall) {
  HandleScope scope(isolate);
  return OptimizeFunctionOnNextCall(args, isolate, TierupKind::kTierupBytecode);
}

RUNTIME_FUNCTION(Runtime_TierupFunctionOnNextCall) {
  HandleScope scope(isolate);
  return OptimizeFunctionOnNextCall(args, isolate,
                                    TierupKind::kTierupBytecodeOrMidTier);
}

RUNTIME_FUNCTION(Runtime_EnsureFeedbackVectorForFunction) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  EnsureFeedbackVector(isolate, function);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_PrepareFunctionForOptimization) {
  HandleScope scope(isolate);
  if ((args.length() != 1 && args.length() != 2) || !args[0].IsJSFunction()) {
    return CrashUnlessFuzzing(isolate);
  }
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

  bool allow_heuristic_optimization = false;
  if (args.length() == 2) {
    CONVERT_ARG_HANDLE_CHECKED(Object, sync_object, 1);
    if (!sync_object->IsString()) return CrashUnlessFuzzing(isolate);
    Handle<String> sync = Handle<String>::cast(sync_object);
    if (sync->IsOneByteEqualTo(
            StaticCharVector("allow heuristic optimization"))) {
      allow_heuristic_optimization = true;
    }
  }

  if (!EnsureFeedbackVector(isolate, function)) {
    return CrashUnlessFuzzing(isolate);
  }

  // If optimization is disabled for the function, return without making it
  // pending optimize for test.
  if (function->shared().optimization_disabled() &&
      function->shared().disable_optimization_reason() ==
          BailoutReason::kNeverOptimize) {
    return CrashUnlessFuzzing(isolate);
  }

#if V8_ENABLE_WEBASSEMBLY
  if (function->shared().HasAsmWasmData()) return CrashUnlessFuzzing(isolate);
#endif  // V8_ENABLE_WEBASSEMBLY

  // Hold onto the bytecode array between marking and optimization to ensure
  // it's not flushed.
  if (FLAG_testing_d8_test_runner) {
    PendingOptimizationTable::PreparedForOptimization(
        isolate, function, allow_heuristic_optimization);
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_OptimizeOsr) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0 || args.length() == 1);

  Handle<JSFunction> function;

  // The optional parameter determines the frame being targeted.
  int stack_depth = 0;
  if (args.length() == 1) {
    if (!args[0].IsSmi()) return CrashUnlessFuzzing(isolate);
    stack_depth = args.smi_at(0);
  }

  // Find the JavaScript function on the top of the stack.
  JavaScriptFrameIterator it(isolate);
  while (!it.done() && stack_depth--) it.Advance();
  if (!it.done()) function = handle(it.frame()->function(), isolate);
  if (function.is_null()) return CrashUnlessFuzzing(isolate);

  if (!FLAG_opt) return ReadOnlyRoots(isolate).undefined_value();

  if (function->shared().optimization_disabled() &&
      function->shared().disable_optimization_reason() ==
          BailoutReason::kNeverOptimize) {
    return CrashUnlessFuzzing(isolate);
  }

  if (FLAG_testing_d8_test_runner) {
    PendingOptimizationTable::MarkedForOptimization(isolate, function);
  }

  if (function->HasAvailableOptimizedCode()) {
    DCHECK(function->HasAttachedOptimizedCode() ||
           function->ChecksOptimizationMarker());
    // If function is already optimized, remove the bytecode array from the
    // pending optimize for test table and return.
    if (FLAG_testing_d8_test_runner) {
      PendingOptimizationTable::FunctionWasOptimized(isolate, function);
    }
    return ReadOnlyRoots(isolate).undefined_value();
  }

  // Ensure that the function is marked for non-concurrent optimization, so that
  // subsequent runs don't also optimize.
  if (FLAG_trace_osr) {
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintF(scope.file(), "[OSR - OptimizeOsr marking ");
    function->ShortPrint(scope.file());
    PrintF(scope.file(), " for non-concurrent optimization]\n");
  }
  IsCompiledScope is_compiled_scope(
      function->shared().is_compiled_scope(isolate));
  JSFunction::EnsureFeedbackVector(function, &is_compiled_scope);
  function->MarkForOptimization(ConcurrencyMode::kNotConcurrent);

  // Make the profiler arm all back edges in unoptimized code.
  if (it.frame()->is_unoptimized()) {
    isolate->runtime_profiler()->AttemptOnStackReplacement(
        UnoptimizedFrame::cast(it.frame()),
        AbstractCode::kMaxLoopNestingMarker);
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_NeverOptimizeFunction) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, function_object, 0);
  if (!function_object->IsJSFunction()) return CrashUnlessFuzzing(isolate);
  Handle<JSFunction> function = Handle<JSFunction>::cast(function_object);
  SharedFunctionInfo sfi = function->shared();
  if (sfi.abstract_code(isolate).kind() != CodeKind::INTERPRETED_FUNCTION &&
      sfi.abstract_code(isolate).kind() != CodeKind::BUILTIN) {
    return CrashUnlessFuzzing(isolate);
  }
  sfi.DisableOptimization(BailoutReason::kNeverOptimize);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_GetOptimizationStatus) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1 || args.length() == 2);
  int status = 0;
  if (FLAG_lite_mode || FLAG_jitless) {
    // Both jitless and lite modes cannot optimize. Unit tests should handle
    // these the same way. In the future, the two flags may become synonyms.
    status |= static_cast<int>(OptimizationStatus::kLiteMode);
  }
  if (!isolate->use_optimizer()) {
    status |= static_cast<int>(OptimizationStatus::kNeverOptimize);
  }
  if (FLAG_always_opt || FLAG_prepare_always_opt) {
    status |= static_cast<int>(OptimizationStatus::kAlwaysOptimize);
  }
  if (FLAG_deopt_every_n_times) {
    status |= static_cast<int>(OptimizationStatus::kMaybeDeopted);
  }

  CONVERT_ARG_HANDLE_CHECKED(Object, function_object, 0);
  if (function_object->IsUndefined()) return Smi::FromInt(status);
  if (!function_object->IsJSFunction()) return CrashUnlessFuzzing(isolate);
  Handle<JSFunction> function = Handle<JSFunction>::cast(function_object);

  status |= static_cast<int>(OptimizationStatus::kIsFunction);

  bool sync_with_compiler_thread = true;
  if (args.length() == 2) {
    CONVERT_ARG_HANDLE_CHECKED(Object, sync_object, 1);
    if (!sync_object->IsString()) return CrashUnlessFuzzing(isolate);
    Handle<String> sync = Handle<String>::cast(sync_object);
    if (sync->IsOneByteEqualTo(StaticCharVector("no sync"))) {
      sync_with_compiler_thread = false;
    } else if (sync->IsOneByteEqualTo(StaticCharVector("sync")) ||
               sync->length() == 0) {
      DCHECK(sync_with_compiler_thread);
    } else {
      return CrashUnlessFuzzing(isolate);
    }
  }

  if (isolate->concurrent_recompilation_enabled() &&
      sync_with_compiler_thread) {
    while (function->IsInOptimizationQueue()) {
      isolate->optimizing_compile_dispatcher()->InstallOptimizedFunctions();
      base::OS::Sleep(base::TimeDelta::FromMilliseconds(50));
    }
  }

  if (function->IsMarkedForOptimization()) {
    status |= static_cast<int>(OptimizationStatus::kMarkedForOptimization);
  } else if (function->IsMarkedForConcurrentOptimization()) {
    status |=
        static_cast<int>(OptimizationStatus::kMarkedForConcurrentOptimization);
  } else if (function->IsInOptimizationQueue()) {
    status |= static_cast<int>(OptimizationStatus::kOptimizingConcurrently);
  }

  if (function->HasAttachedOptimizedCode()) {
    if (function->code().marked_for_deoptimization()) {
      status |= static_cast<int>(OptimizationStatus::kMarkedForDeoptimization);
    } else {
      status |= static_cast<int>(OptimizationStatus::kOptimized);
    }
    if (function->code().is_turbofanned()) {
      status |= static_cast<int>(OptimizationStatus::kTurboFanned);
    }
  }
  if (function->HasAttachedCodeKind(CodeKind::BASELINE)) {
    status |= static_cast<int>(OptimizationStatus::kBaseline);
  }
  if (function->ActiveTierIsIgnition()) {
    status |= static_cast<int>(OptimizationStatus::kInterpreted);
  }

  // Additionally, detect activations of this frame on the stack, and report the
  // status of the topmost frame.
  JavaScriptFrame* frame = nullptr;
  JavaScriptFrameIterator it(isolate);
  while (!it.done()) {
    if (it.frame()->function() == *function) {
      frame = it.frame();
      break;
    }
    it.Advance();
  }
  if (frame != nullptr) {
    status |= static_cast<int>(OptimizationStatus::kIsExecuting);
    if (frame->is_optimized()) {
      status |=
          static_cast<int>(OptimizationStatus::kTopmostFrameIsTurboFanned);
    }
  }

  return Smi::FromInt(status);
}

RUNTIME_FUNCTION(Runtime_UnblockConcurrentRecompilation) {
  DCHECK_EQ(0, args.length());
  CHECK(FLAG_block_concurrent_recompilation);
  CHECK(isolate->concurrent_recompilation_enabled());
  isolate->optimizing_compile_dispatcher()->Unblock();
  return ReadOnlyRoots(isolate).undefined_value();
}

static void ReturnNull(const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().SetNull();
}

RUNTIME_FUNCTION(Runtime_GetUndetectable) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  Local<v8::ObjectTemplate> desc = v8::ObjectTemplate::New(v8_isolate);
  desc->MarkAsUndetectable();
  desc->SetCallAsFunctionHandler(ReturnNull);
  Local<v8::Object> obj =
      desc->NewInstance(v8_isolate->GetCurrentContext()).ToLocalChecked();
  return *Utils::OpenHandle(*obj);
}

static void call_as_function(const v8::FunctionCallbackInfo<v8::Value>& args) {
  double v1 =
      args[0]->NumberValue(args.GetIsolate()->GetCurrentContext()).ToChecked();
  double v2 =
      args[1]->NumberValue(args.GetIsolate()->GetCurrentContext()).ToChecked();
  args.GetReturnValue().Set(v8::Number::New(args.GetIsolate(), v1 - v2));
}

// Returns a callable object. The object returns the difference of its two
// parameters when it is called.
RUNTIME_FUNCTION(Runtime_GetCallable) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(v8_isolate);
  Local<ObjectTemplate> instance_template = t->InstanceTemplate();
  instance_template->SetCallAsFunctionHandler(call_as_function);
  v8_isolate->GetCurrentContext();
  Local<v8::Object> instance =
      t->GetFunction(v8_isolate->GetCurrentContext())
          .ToLocalChecked()
          ->NewInstance(v8_isolate->GetCurrentContext())
          .ToLocalChecked();
  return *Utils::OpenHandle(*instance);
}

RUNTIME_FUNCTION(Runtime_ClearFunctionFeedback) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  function->ClearTypeFeedbackInfo();
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_NotifyContextDisposed) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  isolate->heap()->NotifyContextDisposed(true);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_SetAllocationTimeout) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2 || args.length() == 3);
#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
  CONVERT_INT32_ARG_FUZZ_SAFE(timeout, 1);
  isolate->heap()->set_allocation_timeout(timeout);
#endif
#ifdef DEBUG
  CONVERT_INT32_ARG_FUZZ_SAFE(interval, 0);
  FLAG_gc_interval = interval;
  if (args.length() == 3) {
    // Enable/disable inline allocation if requested.
    CONVERT_BOOLEAN_ARG_FUZZ_SAFE(inline_allocation, 2);
    if (inline_allocation) {
      isolate->heap()->EnableInlineAllocation();
    } else {
      isolate->heap()->DisableInlineAllocation();
    }
  }
#endif
  return ReadOnlyRoots(isolate).undefined_value();
}

namespace {

int FixedArrayLenFromSize(int size) {
  return std::min({(size - FixedArray::kHeaderSize) / kTaggedSize,
                   FixedArray::kMaxRegularLength});
}

void FillUpOneNewSpacePage(Isolate* isolate, Heap* heap) {
  PauseAllocationObserversScope pause_observers(heap);
  NewSpace* space = heap->new_space();
  // We cannot rely on `space->limit()` to point to the end of the current page
  // in the case where inline allocations are disabled, it actually points to
  // the current allocation pointer.
  DCHECK_IMPLIES(space->heap()->inline_allocation_disabled(),
                 space->limit() == space->top());
  int space_remaining =
      static_cast<int>(space->to_space().page_high() - space->top());
  while (space_remaining > 0) {
    int length = FixedArrayLenFromSize(space_remaining);
    if (length > 0) {
      Handle<FixedArray> padding =
          isolate->factory()->NewFixedArray(length, AllocationType::kYoung);
      DCHECK(heap->new_space()->Contains(*padding));
      space_remaining -= padding->Size();
    } else {
      // Not enough room to create another fixed array. Create a filler.
      heap->CreateFillerObjectAt(*heap->new_space()->allocation_top_address(),
                                 space_remaining, ClearRecordedSlots::kNo);
      break;
    }
  }
}

}  // namespace

RUNTIME_FUNCTION(Runtime_SimulateNewspaceFull) {
  HandleScope scope(isolate);
  Heap* heap = isolate->heap();
  NewSpace* space = heap->new_space();
  AlwaysAllocateScopeForTesting always_allocate(heap);
  do {
    FillUpOneNewSpacePage(isolate, heap);
  } while (space->AddFreshPage());

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_ScheduleGCInStackCheck) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  isolate->RequestInterrupt(
      [](v8::Isolate* isolate, void*) {
        isolate->RequestGarbageCollectionForTesting(
            v8::Isolate::kFullGarbageCollection);
      },
      nullptr);
  return ReadOnlyRoots(isolate).undefined_value();
}

static void DebugPrintImpl(MaybeObject maybe_object) {
  StdoutStream os;
  if (maybe_object->IsCleared()) {
    os << "[weak cleared]";
  } else {
    Object object = maybe_object.GetHeapObjectOrSmi();
    bool weak = maybe_object.IsWeak();

#ifdef OBJECT_PRINT
    os << "DebugPrint: ";
    if (weak) os << "[weak] ";
    object.Print(os);
    if (object.IsHeapObject()) {
      HeapObject::cast(object).map().Print(os);
    }
#else
    if (weak) os << "[weak] ";
    // ShortPrint is available in release mode. Print is not.
    os << Brief(object);
#endif
  }
  os << std::endl;
}

RUNTIME_FUNCTION(Runtime_DebugPrint) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());

  MaybeObject maybe_object(*args.address_of_arg_at(0));
  DebugPrintImpl(maybe_object);
  return args[0];
}

RUNTIME_FUNCTION(Runtime_DebugPrintPtr) {
  SealHandleScope shs(isolate);
  StdoutStream os;
  DCHECK_EQ(1, args.length());

  MaybeObject maybe_object(*args.address_of_arg_at(0));
  if (!maybe_object.IsCleared()) {
    Object object = maybe_object.GetHeapObjectOrSmi();
    size_t pointer;
    if (object.ToIntegerIndex(&pointer)) {
      MaybeObject from_pointer(static_cast<Address>(pointer));
      DebugPrintImpl(from_pointer);
    }
  }
  // We don't allow the converted pointer to leak out to JavaScript.
  return args[0];
}

RUNTIME_FUNCTION(Runtime_PrintWithNameForAssert) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(2, args.length());

  CONVERT_ARG_CHECKED(String, name, 0);

  PrintF(" * ");
  StringCharacterStream stream(name);
  while (stream.HasMore()) {
    uint16_t character = stream.GetNext();
    PrintF("%c", character);
  }
  PrintF(": ");
  args[1].ShortPrint();
  PrintF("\n");

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_DebugTrace) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  isolate->PrintStack(stdout);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_DebugTrackRetainingPath) {
  HandleScope scope(isolate);
  DCHECK_LE(1, args.length());
  DCHECK_GE(2, args.length());
  CHECK(FLAG_track_retaining_path);
  CONVERT_ARG_HANDLE_CHECKED(HeapObject, object, 0);
  RetainingPathOption option = RetainingPathOption::kDefault;
  if (args.length() == 2) {
    CONVERT_ARG_HANDLE_CHECKED(String, str, 1);
    const char track_ephemeron_path[] = "track-ephemeron-path";
    if (str->IsOneByteEqualTo(StaticCharVector(track_ephemeron_path))) {
      option = RetainingPathOption::kTrackEphemeronPath;
    } else {
      CHECK_EQ(str->length(), 0);
    }
  }
  isolate->heap()->AddRetainingPathTarget(object, option);
  return ReadOnlyRoots(isolate).undefined_value();
}

// This will not allocate (flatten the string), but it may run
// very slowly for very deeply nested ConsStrings.  For debugging use only.
RUNTIME_FUNCTION(Runtime_GlobalPrint) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_CHECKED(String, string, 0);
  StringCharacterStream stream(string);
  while (stream.HasMore()) {
    uint16_t character = stream.GetNext();
    PrintF("%c", character);
  }
  return string;
}

RUNTIME_FUNCTION(Runtime_SystemBreak) {
  // The code below doesn't create handles, but when breaking here in GDB
  // having a handle scope might be useful.
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  base::OS::DebugBreak();
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_SetForceSlowPath) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, arg, 0);
  if (arg.IsTrue(isolate)) {
    isolate->set_force_slow_path(true);
  } else {
    DCHECK(arg.IsFalse(isolate));
    isolate->set_force_slow_path(false);
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_Abort) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_SMI_ARG_CHECKED(message_id, 0);
  const char* message = GetAbortReason(static_cast<AbortReason>(message_id));
  base::OS::PrintError("abort: %s\n", message);
  isolate->PrintStack(stderr);
  base::OS::Abort();
  UNREACHABLE();
}

RUNTIME_FUNCTION(Runtime_AbortJS) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, message, 0);
  if (FLAG_disable_abortjs) {
    base::OS::PrintError("[disabled] abort: %s\n", message->ToCString().get());
    return Object();
  }
  base::OS::PrintError("abort: %s\n", message->ToCString().get());
  isolate->PrintStack(stderr);
  base::OS::Abort();
  UNREACHABLE();
}

RUNTIME_FUNCTION(Runtime_AbortCSAAssert) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, message, 0);
  base::OS::PrintError("abort: CSA_ASSERT failed: %s\n",
                       message->ToCString().get());
  isolate->PrintStack(stderr);
  base::OS::Abort();
  UNREACHABLE();
}

RUNTIME_FUNCTION(Runtime_DisassembleFunction) {
  HandleScope scope(isolate);
#ifdef DEBUG
  DCHECK_EQ(1, args.length());
  // Get the function and make sure it is compiled.
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, func, 0);
  IsCompiledScope is_compiled_scope;
  CHECK(func->is_compiled() ||
        Compiler::Compile(isolate, func, Compiler::KEEP_EXCEPTION,
                          &is_compiled_scope));
  StdoutStream os;
  func->code().Print(os);
  os << std::endl;
#endif  // DEBUG
  return ReadOnlyRoots(isolate).undefined_value();
}

namespace {

int StackSize(Isolate* isolate) {
  int n = 0;
  for (JavaScriptFrameIterator it(isolate); !it.done(); it.Advance()) n++;
  return n;
}

void PrintIndentation(int stack_size) {
  const int max_display = 80;
  if (stack_size <= max_display) {
    PrintF("%4d:%*s", stack_size, stack_size, "");
  } else {
    PrintF("%4d:%*s", stack_size, max_display, "...");
  }
}

}  // namespace

RUNTIME_FUNCTION(Runtime_TraceEnter) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  PrintIndentation(StackSize(isolate));
  JavaScriptFrame::PrintTop(isolate, stdout, true, false);
  PrintF(" {\n");
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_TraceExit) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, obj, 0);
  PrintIndentation(StackSize(isolate));
  PrintF("} -> ");
  obj.ShortPrint();
  PrintF("\n");
  return obj;  // return TOS
}

RUNTIME_FUNCTION(Runtime_HaveSameMap) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_CHECKED(JSObject, obj1, 0);
  CONVERT_ARG_CHECKED(JSObject, obj2, 1);
  return isolate->heap()->ToBoolean(obj1.map() == obj2.map());
}

RUNTIME_FUNCTION(Runtime_InLargeObjectSpace) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(HeapObject, obj, 0);
  return isolate->heap()->ToBoolean(
      isolate->heap()->new_lo_space()->Contains(obj) ||
      isolate->heap()->code_lo_space()->Contains(obj) ||
      isolate->heap()->lo_space()->Contains(obj));
}

RUNTIME_FUNCTION(Runtime_HasElementsInALargeObjectSpace) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(JSArray, array, 0);
  FixedArrayBase elements = array.elements();
  return isolate->heap()->ToBoolean(
      isolate->heap()->new_lo_space()->Contains(elements) ||
      isolate->heap()->lo_space()->Contains(elements));
}

RUNTIME_FUNCTION(Runtime_InYoungGeneration) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(ObjectInYoungGeneration(obj));
}

namespace {

v8::ModifyCodeGenerationFromStringsResult DisallowCodegenFromStringsCallback(
    v8::Local<v8::Context> context, v8::Local<v8::Value> source,
    bool is_code_kind) {
  return {false, {}};
}

}  // namespace

RUNTIME_FUNCTION(Runtime_DisallowCodegenFromStrings) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_BOOLEAN_ARG_CHECKED(flag, 0);
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  v8_isolate->SetModifyCodeGenerationFromStringsCallback(
      flag ? DisallowCodegenFromStringsCallback : nullptr);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_RegexpHasBytecode) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_CHECKED(JSRegExp, regexp, 0);
  CONVERT_BOOLEAN_ARG_CHECKED(is_latin1, 1);
  bool result;
  if (regexp.TypeTag() == JSRegExp::IRREGEXP) {
    result = regexp.Bytecode(is_latin1).IsByteArray();
  } else {
    result = false;
  }
  return isolate->heap()->ToBoolean(result);
}

RUNTIME_FUNCTION(Runtime_RegexpHasNativeCode) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_CHECKED(JSRegExp, regexp, 0);
  CONVERT_BOOLEAN_ARG_CHECKED(is_latin1, 1);
  bool result;
  if (regexp.TypeTag() == JSRegExp::IRREGEXP) {
    result = regexp.Code(is_latin1).IsCode();
  } else {
    result = false;
  }
  return isolate->heap()->ToBoolean(result);
}

RUNTIME_FUNCTION(Runtime_RegexpTypeTag) {
  HandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(JSRegExp, regexp, 0);
  const char* type_str;
  switch (regexp.TypeTag()) {
    case JSRegExp::NOT_COMPILED:
      type_str = "NOT_COMPILED";
      break;
    case JSRegExp::ATOM:
      type_str = "ATOM";
      break;
    case JSRegExp::IRREGEXP:
      type_str = "IRREGEXP";
      break;
    case JSRegExp::EXPERIMENTAL:
      type_str = "EXPERIMENTAL";
      break;
  }
  return *isolate->factory()->NewStringFromAsciiChecked(type_str);
}

RUNTIME_FUNCTION(Runtime_RegexpIsUnmodified) {
  HandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSRegExp, regexp, 0);
  return isolate->heap()->ToBoolean(
      RegExp::IsUnmodifiedRegExp(isolate, regexp));
}

#define ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(Name)      \
  RUNTIME_FUNCTION(Runtime_Has##Name) {                 \
    CONVERT_ARG_CHECKED(JSObject, obj, 0);              \
    return isolate->heap()->ToBoolean(obj.Has##Name()); \
  }

ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(SmiElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(ObjectElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(SmiOrObjectElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(DoubleElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(HoleyElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(DictionaryElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(PackedElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(SloppyArgumentsElements)
// Properties test sitting with elements tests - not fooling anyone.
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastProperties)

#undef ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION

#define FIXED_TYPED_ARRAYS_CHECK_RUNTIME_FUNCTION(Type, type, TYPE, ctype) \
  RUNTIME_FUNCTION(Runtime_HasFixed##Type##Elements) {                     \
    CONVERT_ARG_CHECKED(JSObject, obj, 0);                                 \
    return isolate->heap()->ToBoolean(obj.HasFixed##Type##Elements());     \
  }

TYPED_ARRAYS(FIXED_TYPED_ARRAYS_CHECK_RUNTIME_FUNCTION)

#undef FIXED_TYPED_ARRAYS_CHECK_RUNTIME_FUNCTION

RUNTIME_FUNCTION(Runtime_IsConcatSpreadableProtector) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(
      Protectors::IsIsConcatSpreadableLookupChainIntact(isolate));
}

RUNTIME_FUNCTION(Runtime_TypedArraySpeciesProtector) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(
      Protectors::IsTypedArraySpeciesLookupChainIntact(isolate));
}

RUNTIME_FUNCTION(Runtime_RegExpSpeciesProtector) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(
      Protectors::IsRegExpSpeciesLookupChainIntact(isolate));
}

RUNTIME_FUNCTION(Runtime_PromiseSpeciesProtector) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(
      Protectors::IsPromiseSpeciesLookupChainIntact(isolate));
}

RUNTIME_FUNCTION(Runtime_ArraySpeciesProtector) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(
      Protectors::IsArraySpeciesLookupChainIntact(isolate));
}

RUNTIME_FUNCTION(Runtime_MapIteratorProtector) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(
      Protectors::IsMapIteratorLookupChainIntact(isolate));
}

RUNTIME_FUNCTION(Runtime_SetIteratorProtector) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(
      Protectors::IsSetIteratorLookupChainIntact(isolate));
}

RUNTIME_FUNCTION(Runtime_StringIteratorProtector) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(
      Protectors::IsStringIteratorLookupChainIntact(isolate));
}

RUNTIME_FUNCTION(Runtime_ArrayIteratorProtector) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(
      Protectors::IsArrayIteratorLookupChainIntact(isolate));
}
// For use by tests and fuzzers. It
//
// 1. serializes a snapshot of the current isolate,
// 2. deserializes the snapshot,
// 3. and runs VerifyHeap on the resulting isolate.
//
// The current isolate should not be modified by this call and can keep running
// once it completes.
RUNTIME_FUNCTION(Runtime_SerializeDeserializeNow) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  Snapshot::SerializeDeserializeAndVerifyForTesting(isolate,
                                                    isolate->native_context());
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_HeapObjectVerify) {
  HandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
#ifdef VERIFY_HEAP
  object->ObjectVerify(isolate);
#else
  CHECK(object->IsObject());
  if (object->IsHeapObject()) {
    CHECK(HeapObject::cast(*object).map().IsMap());
  } else {
    CHECK(object->IsSmi());
  }
#endif
  return isolate->heap()->ToBoolean(true);
}

RUNTIME_FUNCTION(Runtime_ArrayBufferMaxByteLength) {
  HandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return *isolate->factory()->NewNumber(JSArrayBuffer::kMaxByteLength);
}

RUNTIME_FUNCTION(Runtime_TypedArrayMaxLength) {
  HandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return *isolate->factory()->NewNumber(JSTypedArray::kMaxLength);
}

RUNTIME_FUNCTION(Runtime_CompleteInobjectSlackTracking) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  object->map().CompleteInobjectSlackTracking(isolate);

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_TurbofanStaticAssert) {
  SealHandleScope shs(isolate);
  // Always lowered to StaticAssert node in Turbofan, so we never get here in
  // compiled code.
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_IsBeingInterpreted) {
  SealHandleScope shs(isolate);
  // Always lowered to false in Turbofan, so we never get here in compiled code.
  return ReadOnlyRoots(isolate).true_value();
}

RUNTIME_FUNCTION(Runtime_EnableCodeLoggingForTesting) {
  // The {NoopListener} currently does nothing on any callback, but reports
  // {true} on {is_listening_to_code_events()}. Feel free to add assertions to
  // any method to further test the code logging callbacks.
  class NoopListener final : public CodeEventListener {
    void CodeCreateEvent(LogEventsAndTags tag, Handle<AbstractCode> code,
                         const char* name) final {}
    void CodeCreateEvent(LogEventsAndTags tag, Handle<AbstractCode> code,
                         Handle<Name> name) final {}
    void CodeCreateEvent(LogEventsAndTags tag, Handle<AbstractCode> code,
                         Handle<SharedFunctionInfo> shared,
                         Handle<Name> script_name) final {}
    void CodeCreateEvent(LogEventsAndTags tag, Handle<AbstractCode> code,
                         Handle<SharedFunctionInfo> shared,
                         Handle<Name> script_name, int line, int column) final {
    }
#if V8_ENABLE_WEBASSEMBLY
    void CodeCreateEvent(LogEventsAndTags tag, const wasm::WasmCode* code,
                         wasm::WasmName name, const char* source_url,
                         int code_offset, int script_id) final {}
#endif  // V8_ENABLE_WEBASSEMBLY

    void CallbackEvent(Handle<Name> name, Address entry_point) final {}
    void GetterCallbackEvent(Handle<Name> name, Address entry_point) final {}
    void SetterCallbackEvent(Handle<Name> name, Address entry_point) final {}
    void RegExpCodeCreateEvent(Handle<AbstractCode> code,
                               Handle<String> source) final {}
    void CodeMoveEvent(AbstractCode from, AbstractCode to) final {}
    void SharedFunctionInfoMoveEvent(Address from, Address to) final {}
    void CodeMovingGCEvent() final {}
    void CodeDisableOptEvent(Handle<AbstractCode> code,
                             Handle<SharedFunctionInfo> shared) final {}
    void CodeDeoptEvent(Handle<Code> code, DeoptimizeKind kind, Address pc,
                        int fp_to_sp_delta, bool reuse_code) final {}
    void CodeDependencyChangeEvent(Handle<Code> code,
                                   Handle<SharedFunctionInfo> shared,
                                   const char* reason) final {}
    void WeakCodeClearEvent() final {}

    bool is_listening_to_code_events() final { return true; }
  };
  static base::LeakyObject<NoopListener> noop_listener;
#if V8_ENABLE_WEBASSEMBLY
  isolate->wasm_engine()->EnableCodeLogging(isolate);
#endif  // V8_ENABLE_WEBASSEMBLY
  isolate->code_event_dispatcher()->AddListener(noop_listener.get());
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_NewRegExpWithBacktrackLimit) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(String, pattern, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, flags_string, 1);
  CONVERT_UINT32_ARG_CHECKED(backtrack_limit, 2);

  bool success = false;
  JSRegExp::Flags flags =
      JSRegExp::FlagsFromString(isolate, flags_string, &success);
  CHECK(success);

  RETURN_RESULT_OR_FAILURE(
      isolate, JSRegExp::New(isolate, pattern, flags, backtrack_limit));
}

}  // namespace internal
}  // namespace v8

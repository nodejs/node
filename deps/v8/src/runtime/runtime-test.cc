// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include <memory>
#include <sstream>

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
#include "src/snapshot/snapshot.h"
#include "src/trap-handler/trap-handler.h"
#include "src/utils/ostreams.h"
#include "src/wasm/memory-tracing.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-serialization.h"

namespace v8 {
namespace internal {

namespace {
struct WasmCompileControls {
  uint32_t MaxWasmBufferSize = std::numeric_limits<uint32_t>::max();
  bool AllowAnySizeForAsync = true;
};
using WasmCompileControlsMap = std::map<v8::Isolate*, WasmCompileControls>;

// We need per-isolate controls, because we sometimes run tests in multiple
// isolates concurrently. Methods need to hold the accompanying mutex on access.
// To avoid upsetting the static initializer count, we lazy initialize this.
DEFINE_LAZY_LEAKY_OBJECT_GETTER(WasmCompileControlsMap,
                                GetPerIsolateWasmControls)
base::LazyMutex g_PerIsolateWasmControlsMutex = LAZY_MUTEX_INITIALIZER;

bool IsWasmCompileAllowed(v8::Isolate* isolate, v8::Local<v8::Value> value,
                          bool is_async) {
  base::MutexGuard guard(g_PerIsolateWasmControlsMutex.Pointer());
  DCHECK_GT(GetPerIsolateWasmControls()->count(isolate), 0);
  const WasmCompileControls& ctrls = GetPerIsolateWasmControls()->at(isolate);
  return (is_async && ctrls.AllowAnySizeForAsync) ||
         (value->IsArrayBuffer() &&
          v8::Local<v8::ArrayBuffer>::Cast(value)->ByteLength() <=
              ctrls.MaxWasmBufferSize) ||
         (value->IsArrayBufferView() &&
          v8::Local<v8::ArrayBufferView>::Cast(value)->ByteLength() <=
              ctrls.MaxWasmBufferSize);
}

// Use the compile controls for instantiation, too
bool IsWasmInstantiateAllowed(v8::Isolate* isolate,
                              v8::Local<v8::Value> module_or_bytes,
                              bool is_async) {
  base::MutexGuard guard(g_PerIsolateWasmControlsMutex.Pointer());
  DCHECK_GT(GetPerIsolateWasmControls()->count(isolate), 0);
  const WasmCompileControls& ctrls = GetPerIsolateWasmControls()->at(isolate);
  if (is_async && ctrls.AllowAnySizeForAsync) return true;
  if (!module_or_bytes->IsWasmModuleObject()) {
    return IsWasmCompileAllowed(isolate, module_or_bytes, is_async);
  }
  v8::Local<v8::WasmModuleObject> module =
      v8::Local<v8::WasmModuleObject>::Cast(module_or_bytes);
  return static_cast<uint32_t>(
             module->GetCompiledModule().GetWireBytesRef().size()) <=
         ctrls.MaxWasmBufferSize;
}

v8::Local<v8::Value> NewRangeException(v8::Isolate* isolate,
                                       const char* message) {
  return v8::Exception::RangeError(
      v8::String::NewFromOneByte(isolate,
                                 reinterpret_cast<const uint8_t*>(message))
          .ToLocalChecked());
}

void ThrowRangeException(v8::Isolate* isolate, const char* message) {
  isolate->ThrowException(NewRangeException(isolate, message));
}

bool WasmModuleOverride(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (IsWasmCompileAllowed(args.GetIsolate(), args[0], false)) return false;
  ThrowRangeException(args.GetIsolate(), "Sync compile not allowed");
  return true;
}

bool WasmInstanceOverride(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (IsWasmInstantiateAllowed(args.GetIsolate(), args[0], false)) return false;
  ThrowRangeException(args.GetIsolate(), "Sync instantiate not allowed");
  return true;
}

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

RUNTIME_FUNCTION(Runtime_DynamicMapChecksEnabled) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(FLAG_dynamic_map_checks);
}

RUNTIME_FUNCTION(Runtime_OptimizeFunctionOnNextCall) {
  HandleScope scope(isolate);

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
      !Compiler::Compile(function, Compiler::CLEAR_EXCEPTION,
                         &is_compiled_scope)) {
    return CrashUnlessFuzzing(isolate);
  }

  if (!FLAG_opt) return ReadOnlyRoots(isolate).undefined_value();

  if (function->shared().optimization_disabled() &&
      function->shared().disable_optimization_reason() ==
          BailoutReason::kNeverOptimize) {
    return CrashUnlessFuzzing(isolate);
  }

  if (function->shared().HasAsmWasmData()) return CrashUnlessFuzzing(isolate);

  if (FLAG_testing_d8_test_runner) {
    PendingOptimizationTable::MarkedForOptimization(isolate, function);
  }

  if (function->HasAvailableOptimizedCode()) {
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

namespace {

bool EnsureFeedbackVector(Handle<JSFunction> function) {
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
      !Compiler::Compile(function, Compiler::CLEAR_EXCEPTION,
                         &is_compiled_scope)) {
    return false;
  }

  // Ensure function has a feedback vector to hold type feedback for
  // optimization.
  JSFunction::EnsureFeedbackVector(function, &is_compiled_scope);
  return true;
}

}  // namespace

RUNTIME_FUNCTION(Runtime_EnsureFeedbackVectorForFunction) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  EnsureFeedbackVector(function);
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

  if (!EnsureFeedbackVector(function)) {
    return CrashUnlessFuzzing(isolate);
  }

  // If optimization is disabled for the function, return without making it
  // pending optimize for test.
  if (function->shared().optimization_disabled() &&
      function->shared().disable_optimization_reason() ==
          BailoutReason::kNeverOptimize) {
    return CrashUnlessFuzzing(isolate);
  }

  if (function->shared().HasAsmWasmData()) return CrashUnlessFuzzing(isolate);

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
  if (it.frame()->type() == StackFrame::INTERPRETED) {
    isolate->runtime_profiler()->AttemptOnStackReplacement(
        InterpretedFrame::cast(it.frame()),
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
  if (sfi.abstract_code().kind() != CodeKind::INTERPRETED_FUNCTION &&
      sfi.abstract_code().kind() != CodeKind::BUILTIN) {
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
  } else if (function->IsInOptimizationQueue()) {
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

RUNTIME_FUNCTION(Runtime_SetWasmCompileControls) {
  HandleScope scope(isolate);
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  CHECK_EQ(args.length(), 2);
  CONVERT_ARG_HANDLE_CHECKED(Smi, block_size, 0);
  CONVERT_BOOLEAN_ARG_CHECKED(allow_async, 1);
  base::MutexGuard guard(g_PerIsolateWasmControlsMutex.Pointer());
  WasmCompileControls& ctrl = (*GetPerIsolateWasmControls())[v8_isolate];
  ctrl.AllowAnySizeForAsync = allow_async;
  ctrl.MaxWasmBufferSize = static_cast<uint32_t>(block_size->value());
  v8_isolate->SetWasmModuleCallback(WasmModuleOverride);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_SetWasmInstantiateControls) {
  HandleScope scope(isolate);
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  CHECK_EQ(args.length(), 0);
  v8_isolate->SetWasmInstanceCallback(WasmInstanceOverride);
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
  return Min((size - FixedArray::kHeaderSize) / kTaggedSize,
             FixedArray::kMaxRegularLength);
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
        Compiler::Compile(func, Compiler::KEEP_EXCEPTION, &is_compiled_scope));
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

namespace {

int WasmStackSize(Isolate* isolate) {
  // TODO(wasm): Fix this for mixed JS/Wasm stacks with both --trace and
  // --trace-wasm.
  int n = 0;
  for (StackTraceFrameIterator it(isolate); !it.done(); it.Advance()) {
    if (it.is_wasm()) n++;
  }
  return n;
}

}  // namespace

RUNTIME_FUNCTION(Runtime_WasmTraceEnter) {
  HandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  PrintIndentation(WasmStackSize(isolate));

  // Find the caller wasm frame.
  wasm::WasmCodeRefScope wasm_code_ref_scope;
  StackTraceFrameIterator it(isolate);
  DCHECK(!it.done());
  DCHECK(it.is_wasm());
  WasmFrame* frame = WasmFrame::cast(it.frame());

  // Find the function name.
  int func_index = frame->function_index();
  const wasm::WasmModule* module = frame->wasm_instance().module();
  wasm::ModuleWireBytes wire_bytes =
      wasm::ModuleWireBytes(frame->native_module()->wire_bytes());
  wasm::WireBytesRef name_ref =
      module->lazily_generated_names.LookupFunctionName(
          wire_bytes, func_index, VectorOf(module->export_table));
  wasm::WasmName name = wire_bytes.GetNameOrNull(name_ref);

  wasm::WasmCode* code = frame->wasm_code();
  PrintF(code->is_liftoff() ? "~" : "*");

  if (name.empty()) {
    PrintF("wasm-function[%d] {\n", func_index);
  } else {
    PrintF("wasm-function[%d] \"%.*s\" {\n", func_index, name.length(),
           name.begin());
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmTraceExit) {
  HandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Smi, value_addr_smi, 0);

  PrintIndentation(WasmStackSize(isolate));
  PrintF("}");

  // Find the caller wasm frame.
  wasm::WasmCodeRefScope wasm_code_ref_scope;
  StackTraceFrameIterator it(isolate);
  DCHECK(!it.done());
  DCHECK(it.is_wasm());
  WasmFrame* frame = WasmFrame::cast(it.frame());
  int func_index = frame->function_index();
  const wasm::FunctionSig* sig =
      frame->wasm_instance().module()->functions[func_index].sig;

  size_t num_returns = sig->return_count();
  if (num_returns == 1) {
    wasm::ValueType return_type = sig->GetReturn(0);
    switch (return_type.kind()) {
      case wasm::ValueType::kI32: {
        int32_t value = ReadUnalignedValue<int32_t>(value_addr_smi.ptr());
        PrintF(" -> %d\n", value);
        break;
      }
      case wasm::ValueType::kI64: {
        int64_t value = ReadUnalignedValue<int64_t>(value_addr_smi.ptr());
        PrintF(" -> %" PRId64 "\n", value);
        break;
      }
      case wasm::ValueType::kF32: {
        float_t value = ReadUnalignedValue<float_t>(value_addr_smi.ptr());
        PrintF(" -> %f\n", value);
        break;
      }
      case wasm::ValueType::kF64: {
        double_t value = ReadUnalignedValue<double_t>(value_addr_smi.ptr());
        PrintF(" -> %f\n", value);
        break;
      }
      default:
        PrintF(" -> Unsupported type\n");
        break;
    }
  } else {
    // TODO(wasm) Handle multiple return values.
    PrintF("\n");
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_HaveSameMap) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_CHECKED(JSObject, obj1, 0);
  CONVERT_ARG_CHECKED(JSObject, obj2, 1);
  return isolate->heap()->ToBoolean(obj1.map() == obj2.map());
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

RUNTIME_FUNCTION(Runtime_IsAsmWasmCode) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(JSFunction, function, 0);
  if (!function.shared().HasAsmWasmData()) {
    return ReadOnlyRoots(isolate).false_value();
  }
  if (function.shared().HasBuiltinId() &&
      function.shared().builtin_id() == Builtins::kInstantiateAsmJs) {
    // Hasn't been compiled yet.
    return ReadOnlyRoots(isolate).false_value();
  }
  return ReadOnlyRoots(isolate).true_value();
}

namespace {

v8::ModifyCodeGenerationFromStringsResult DisallowCodegenFromStringsCallback(
    v8::Local<v8::Context> context, v8::Local<v8::Value> source) {
  return {false, {}};
}

bool DisallowWasmCodegenFromStringsCallback(v8::Local<v8::Context> context,
                                            v8::Local<v8::String> source) {
  return false;
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

RUNTIME_FUNCTION(Runtime_DisallowWasmCodegen) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_BOOLEAN_ARG_CHECKED(flag, 0);
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  v8_isolate->SetAllowWasmCodeGenerationCallback(
      flag ? DisallowWasmCodegenFromStringsCallback : nullptr);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_IsWasmCode) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(JSFunction, function, 0);
  bool is_js_to_wasm =
      function.code().kind() == CodeKind::JS_TO_WASM_FUNCTION ||
      (function.code().is_builtin() &&
       function.code().builtin_index() == Builtins::kGenericJSToWasmWrapper);
  return isolate->heap()->ToBoolean(is_js_to_wasm);
}

RUNTIME_FUNCTION(Runtime_IsWasmTrapHandlerEnabled) {
  DisallowHeapAllocation no_gc;
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(trap_handler::IsTrapHandlerEnabled());
}

RUNTIME_FUNCTION(Runtime_IsThreadInWasm) {
  DisallowHeapAllocation no_gc;
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(trap_handler::IsThreadInWasm());
}

RUNTIME_FUNCTION(Runtime_GetWasmRecoveredTrapCount) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  size_t trap_count = trap_handler::GetRecoveredTrapCount();
  return *isolate->factory()->NewNumberFromSize(trap_count);
}

RUNTIME_FUNCTION(Runtime_GetWasmExceptionId) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmExceptionPackage, exception, 0);
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 1);
  Handle<Object> tag =
      WasmExceptionPackage::GetExceptionTag(isolate, exception);
  CHECK(tag->IsWasmExceptionTag());
  Handle<FixedArray> exceptions_table(instance->exceptions_table(), isolate);
  for (int index = 0; index < exceptions_table->length(); ++index) {
    if (exceptions_table->get(index) == *tag) return Smi::FromInt(index);
  }
  UNREACHABLE();
}

RUNTIME_FUNCTION(Runtime_GetWasmExceptionValues) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmExceptionPackage, exception, 0);
  Handle<Object> values_obj =
      WasmExceptionPackage::GetExceptionValues(isolate, exception);
  CHECK(values_obj->IsFixedArray());  // Only called with correct input.
  Handle<FixedArray> values = Handle<FixedArray>::cast(values_obj);
  return *isolate->factory()->NewJSArrayWithElements(values);
}

namespace {
bool EnableWasmThreads(v8::Local<v8::Context> context) { return true; }
bool DisableWasmThreads(v8::Local<v8::Context> context) { return false; }
}  // namespace

// This runtime function enables WebAssembly threads through an embedder
// callback and thereby bypasses the value in FLAG_experimental_wasm_threads.
RUNTIME_FUNCTION(Runtime_SetWasmThreadsEnabled) {
  DCHECK_EQ(1, args.length());
  CONVERT_BOOLEAN_ARG_CHECKED(flag, 0);
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  v8_isolate->SetWasmThreadsEnabledCallback(flag ? EnableWasmThreads
                                                 : DisableWasmThreads);
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

// Wait until the given module is fully tiered up, then serialize it into an
// array buffer.
RUNTIME_FUNCTION(Runtime_SerializeWasmModule) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmModuleObject, module_obj, 0);

  wasm::NativeModule* native_module = module_obj->native_module();
  native_module->compilation_state()->WaitForTopTierFinished();
  DCHECK(!native_module->compilation_state()->failed());

  wasm::WasmSerializer wasm_serializer(native_module);
  size_t byte_length = wasm_serializer.GetSerializedNativeModuleSize();

  Handle<JSArrayBuffer> array_buffer =
      isolate->factory()
          ->NewJSArrayBufferAndBackingStore(byte_length,
                                            InitializedFlag::kUninitialized)
          .ToHandleChecked();

  CHECK(wasm_serializer.SerializeNativeModule(
      {static_cast<uint8_t*>(array_buffer->backing_store()), byte_length}));
  return *array_buffer;
}

// Take an array buffer and attempt to reconstruct a compiled wasm module.
// Return undefined if unsuccessful.
RUNTIME_FUNCTION(Runtime_DeserializeWasmModule) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSArrayBuffer, buffer, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, wire_bytes, 1);
  CHECK(!buffer->was_detached());
  CHECK(!wire_bytes->WasDetached());

  Handle<JSArrayBuffer> wire_bytes_buffer = wire_bytes->GetBuffer();
  Vector<const uint8_t> wire_bytes_vec{
      reinterpret_cast<const uint8_t*>(wire_bytes_buffer->backing_store()) +
          wire_bytes->byte_offset(),
      wire_bytes->byte_length()};
  Vector<uint8_t> buffer_vec{
      reinterpret_cast<uint8_t*>(buffer->backing_store()),
      buffer->byte_length()};

  // Note that {wasm::DeserializeNativeModule} will allocate. We assume the
  // JSArrayBuffer backing store doesn't get relocated.
  MaybeHandle<WasmModuleObject> maybe_module_object =
      wasm::DeserializeNativeModule(isolate, buffer_vec, wire_bytes_vec, {});
  Handle<WasmModuleObject> module_object;
  if (!maybe_module_object.ToHandle(&module_object)) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
  return *module_object;
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

RUNTIME_FUNCTION(Runtime_WasmGetNumberOfInstances) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmModuleObject, module_obj, 0);
  int instance_count = 0;
  WeakArrayList weak_instance_list =
      module_obj->script().wasm_weak_instance_list();
  for (int i = 0; i < weak_instance_list.length(); ++i) {
    if (weak_instance_list.Get(i)->IsWeak()) instance_count++;
  }
  return Smi::FromInt(instance_count);
}

RUNTIME_FUNCTION(Runtime_WasmNumCodeSpaces) {
  DCHECK_EQ(1, args.length());
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, argument, 0);
  Handle<WasmModuleObject> module;
  if (argument->IsWasmInstanceObject()) {
    module = handle(Handle<WasmInstanceObject>::cast(argument)->module_object(),
                    isolate);
  } else if (argument->IsWasmModuleObject()) {
    module = Handle<WasmModuleObject>::cast(argument);
  }
  size_t num_spaces =
      module->native_module()->GetNumberOfCodeSpacesForTesting();
  return *isolate->factory()->NewNumberFromSize(num_spaces);
}

RUNTIME_FUNCTION(Runtime_WasmTraceMemory) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Smi, info_addr, 0);

  wasm::MemoryTracingInfo* info =
      reinterpret_cast<wasm::MemoryTracingInfo*>(info_addr.ptr());

  // Find the caller wasm frame.
  wasm::WasmCodeRefScope wasm_code_ref_scope;
  StackTraceFrameIterator it(isolate);
  DCHECK(!it.done());
  DCHECK(it.is_wasm());
  WasmFrame* frame = WasmFrame::cast(it.frame());

  uint8_t* mem_start = reinterpret_cast<uint8_t*>(
      frame->wasm_instance().memory_object().array_buffer().backing_store());
  int func_index = frame->function_index();
  int pos = frame->position();
  // TODO(titzer): eliminate dependency on WasmModule definition here.
  int func_start =
      frame->wasm_instance().module()->functions[func_index].code.offset();
  wasm::ExecutionTier tier = frame->wasm_code()->is_liftoff()
                                 ? wasm::ExecutionTier::kLiftoff
                                 : wasm::ExecutionTier::kTurbofan;
  wasm::TraceMemoryOperation(tier, info, func_index, pos - func_start,
                             mem_start);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmTierUpFunction) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_SMI_ARG_CHECKED(function_index, 1);
  auto* native_module = instance->module_object().native_module();
  isolate->wasm_engine()->CompileFunction(
      isolate, native_module, function_index, wasm::ExecutionTier::kTurbofan);
  CHECK(!native_module->compilation_state()->failed());
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmTierDownModule) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  auto* native_module = instance->module_object().native_module();
  native_module->SetTieringState(wasm::kTieredDown);
  native_module->RecompileForTiering();
  CHECK(!native_module->compilation_state()->failed());
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmTierUpModule) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  auto* native_module = instance->module_object().native_module();
  native_module->SetTieringState(wasm::kTieredUp);
  native_module->RecompileForTiering();
  CHECK(!native_module->compilation_state()->failed());
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_IsLiftoffFunction) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CHECK(WasmExportedFunction::IsWasmExportedFunction(*function));
  Handle<WasmExportedFunction> exp_fun =
      Handle<WasmExportedFunction>::cast(function);
  wasm::NativeModule* native_module =
      exp_fun->instance().module_object().native_module();
  uint32_t func_index = exp_fun->function_index();
  wasm::WasmCodeRefScope code_ref_scope;
  wasm::WasmCode* code = native_module->GetCode(func_index);
  return isolate->heap()->ToBoolean(code && code->is_liftoff());
}

RUNTIME_FUNCTION(Runtime_CompleteInobjectSlackTracking) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  object->map().CompleteInobjectSlackTracking(isolate);

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_FreezeWasmLazyCompilation) {
  DCHECK_EQ(1, args.length());
  DisallowHeapAllocation no_gc;
  CONVERT_ARG_CHECKED(WasmInstanceObject, instance, 0);

  instance.module_object().native_module()->set_lazy_compile_frozen(true);
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
    void CodeCreateEvent(LogEventsAndTags tag, const wasm::WasmCode* code,
                         wasm::WasmName name) final {}

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

    bool is_listening_to_code_events() final { return true; }
  };
  static base::LeakyObject<NoopListener> noop_listener;
  isolate->wasm_engine()->EnableCodeLogging(isolate);
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

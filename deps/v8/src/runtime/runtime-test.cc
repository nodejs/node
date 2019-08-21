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
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/arguments-inl.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/runtime-profiler.h"
#include "src/heap/heap-inl.h"  // For ToBoolean. TODO(jkummerow): Drop.
#include "src/heap/heap-write-barrier-inl.h"
#include "src/ic/stub-cache.h"
#include "src/logging/counters.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/smi.h"
#include "src/snapshot/natives.h"
#include "src/trap-handler/trap-handler.h"
#include "src/utils/ostreams.h"
#include "src/wasm/memory-tracing.h"
#include "src/wasm/module-compiler.h"
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
  if (!module_or_bytes->IsWebAssemblyCompiledModule()) {
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
                                 reinterpret_cast<const uint8_t*>(message),
                                 v8::NewStringType::kNormal)
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

  // This function is used by fuzzers to get coverage in compiler.
  // Ignore calls on non-function objects to avoid runtime errors.
  CONVERT_ARG_HANDLE_CHECKED(Object, function_object, 0);
  if (!function_object->IsJSFunction()) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
  Handle<JSFunction> function = Handle<JSFunction>::cast(function_object);

  // If the function is not optimized, just return.
  if (!function->IsOptimized()) return ReadOnlyRoots(isolate).undefined_value();

  Deoptimizer::DeoptimizeFunction(*function);

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_DeoptimizeNow) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());

  Handle<JSFunction> function;

  // Find the JavaScript function on the top of the stack.
  JavaScriptFrameIterator it(isolate);
  if (!it.done()) function = handle(it.frame()->function(), isolate);
  if (function.is_null()) return ReadOnlyRoots(isolate).undefined_value();

  // If the function is not optimized, just return.
  if (!function->IsOptimized()) return ReadOnlyRoots(isolate).undefined_value();

  Deoptimizer::DeoptimizeFunction(*function);

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

RUNTIME_FUNCTION(Runtime_OptimizeFunctionOnNextCall) {
  HandleScope scope(isolate);

  // This function is used by fuzzers, ignore calls with bogus arguments count.
  if (args.length() != 1 && args.length() != 2) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  // This function is used by fuzzers to get coverage for optimizations
  // in compiler. Ignore calls on non-function objects to avoid runtime errors.
  CONVERT_ARG_HANDLE_CHECKED(Object, function_object, 0);
  if (!function_object->IsJSFunction()) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
  Handle<JSFunction> function = Handle<JSFunction>::cast(function_object);

  // The following conditions were lifted (in part) from the DCHECK inside
  // JSFunction::MarkForOptimization().

  if (!function->shared().allows_lazy_compilation()) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  // If function isn't compiled, compile it now.
  IsCompiledScope is_compiled_scope(function->shared().is_compiled_scope());
  if (!is_compiled_scope.is_compiled() &&
      !Compiler::Compile(function, Compiler::CLEAR_EXCEPTION,
                         &is_compiled_scope)) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  if (!FLAG_opt || (function->shared().optimization_disabled() &&
                    function->shared().disable_optimization_reason() ==
                        BailoutReason::kNeverOptimize)) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  if (function->shared().HasAsmWasmData()) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  if (FLAG_testing_d8_test_runner) {
    PendingOptimizationTable::MarkedForOptimization(isolate, function);
  }

  if (function->HasOptimizedCode()) {
    DCHECK(function->IsOptimized() || function->ChecksOptimizationMarker());
    if (FLAG_testing_d8_test_runner) {
      PendingOptimizationTable::FunctionWasOptimized(isolate, function);
    }
    return ReadOnlyRoots(isolate).undefined_value();
  }

  ConcurrencyMode concurrency_mode = ConcurrencyMode::kNotConcurrent;
  if (args.length() == 2) {
    // Ignore invalid inputs produced by fuzzers.
    CONVERT_ARG_HANDLE_CHECKED(Object, type, 1);
    if (!type->IsString()) {
      return ReadOnlyRoots(isolate).undefined_value();
    }
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

  JSFunction::EnsureFeedbackVector(function);
  function->MarkForOptimization(concurrency_mode);

  return ReadOnlyRoots(isolate).undefined_value();
}

namespace {

bool EnsureFeedbackVector(Handle<JSFunction> function) {
  // Check function allows lazy compilation.
  if (!function->shared().allows_lazy_compilation()) {
    return false;
  }

  // If function isn't compiled, compile it now.
  IsCompiledScope is_compiled_scope(function->shared().is_compiled_scope());
  if (!is_compiled_scope.is_compiled() &&
      !Compiler::Compile(function, Compiler::CLEAR_EXCEPTION,
                         &is_compiled_scope)) {
    return false;
  }

  // Ensure function has a feedback vector to hold type feedback for
  // optimization.
  JSFunction::EnsureFeedbackVector(function);
  return true;
}

}  // namespace

RUNTIME_FUNCTION(Runtime_EnsureFeedbackVectorForFunction) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  if (!args[0].IsJSFunction()) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  EnsureFeedbackVector(function);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_PrepareFunctionForOptimization) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  if (!args[0].IsJSFunction()) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

  if (!EnsureFeedbackVector(function)) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  // If optimization is disabled for the function, return without making it
  // pending optimize for test.
  if (function->shared().optimization_disabled() &&
      function->shared().disable_optimization_reason() ==
          BailoutReason::kNeverOptimize) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  // We don't optimize Asm/Wasm functions.
  if (function->shared().HasAsmWasmData()) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  // Hold onto the bytecode array between marking and optimization to ensure
  // it's not flushed.
  if (FLAG_testing_d8_test_runner) {
    PendingOptimizationTable::PreparedForOptimization(isolate, function);
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_OptimizeOsr) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0 || args.length() == 1);

  Handle<JSFunction> function;

  // The optional parameter determines the frame being targeted.
  int stack_depth = args.length() == 1 ? args.smi_at(0) : 0;

  // Find the JavaScript function on the top of the stack.
  JavaScriptFrameIterator it(isolate);
  while (!it.done() && stack_depth--) it.Advance();
  if (!it.done()) function = handle(it.frame()->function(), isolate);
  if (function.is_null()) return ReadOnlyRoots(isolate).undefined_value();

  if (!FLAG_opt || (function->shared().optimization_disabled() &&
                    function->shared().disable_optimization_reason() ==
                        BailoutReason::kNeverOptimize)) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  if (FLAG_testing_d8_test_runner) {
    PendingOptimizationTable::MarkedForOptimization(isolate, function);
  }

  if (function->HasOptimizedCode()) {
    DCHECK(function->IsOptimized() || function->ChecksOptimizationMarker());
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
    PrintF("[OSR - OptimizeOsr marking ");
    function->ShortPrint();
    PrintF(" for non-concurrent optimization]\n");
  }
  JSFunction::EnsureFeedbackVector(function);
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
  // This function is used by fuzzers to get coverage for optimizations
  // in compiler. Ignore calls on non-function objects to avoid runtime errors.
  CONVERT_ARG_HANDLE_CHECKED(Object, function_object, 0);
  if (!function_object->IsJSFunction()) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
  Handle<JSFunction> function = Handle<JSFunction>::cast(function_object);
  function->shared().DisableOptimization(BailoutReason::kNeverOptimize);
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

  // This function is used by fuzzers to get coverage for optimizations
  // in compiler. Ignore calls on non-function objects to avoid runtime errors.
  CONVERT_ARG_HANDLE_CHECKED(Object, function_object, 0);
  if (!function_object->IsJSFunction()) {
    return Smi::FromInt(status);
  }
  Handle<JSFunction> function = Handle<JSFunction>::cast(function_object);
  status |= static_cast<int>(OptimizationStatus::kIsFunction);

  bool sync_with_compiler_thread = true;
  if (args.length() == 2) {
    CONVERT_ARG_HANDLE_CHECKED(Object, sync_object, 1);
    if (!sync_object->IsString())
      return ReadOnlyRoots(isolate).undefined_value();
    Handle<String> sync = Handle<String>::cast(sync_object);
    if (sync->IsOneByteEqualTo(StaticCharVector("no sync"))) {
      sync_with_compiler_thread = false;
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

  if (function->IsOptimized()) {
    status |= static_cast<int>(OptimizationStatus::kOptimized);
    if (function->code().is_turbofanned()) {
      status |= static_cast<int>(OptimizationStatus::kTurboFanned);
    }
  }
  if (function->IsInterpreted()) {
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
  if (FLAG_block_concurrent_recompilation &&
      isolate->concurrent_recompilation_enabled()) {
    isolate->optimizing_compile_dispatcher()->Unblock();
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

static void ReturnThis(const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(args.This());
}

RUNTIME_FUNCTION(Runtime_GetUndetectable) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);

  Local<v8::ObjectTemplate> desc = v8::ObjectTemplate::New(v8_isolate);
  desc->MarkAsUndetectable();
  desc->SetCallAsFunctionHandler(ReturnThis);
  Local<v8::Object> obj;
  if (!desc->NewInstance(v8_isolate->GetCurrentContext()).ToLocal(&obj)) {
    return Object();
  }
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
  if (!args[0].IsJSFunction()) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
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
  CONVERT_INT32_ARG_CHECKED(timeout, 1);
  isolate->heap()->set_allocation_timeout(timeout);
#endif
#ifdef DEBUG
  CONVERT_INT32_ARG_CHECKED(interval, 0);
  FLAG_gc_interval = interval;
  if (args.length() == 3) {
    // Enable/disable inline allocation if requested.
    CONVERT_BOOLEAN_ARG_CHECKED(inline_allocation, 2);
    if (inline_allocation) {
      isolate->heap()->EnableInlineAllocation();
    } else {
      isolate->heap()->DisableInlineAllocation();
    }
  }
#endif
  return ReadOnlyRoots(isolate).undefined_value();
}


RUNTIME_FUNCTION(Runtime_DebugPrint) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());

  MaybeObject maybe_object(*args.address_of_arg_at(0));

  StdoutStream os;
  if (maybe_object->IsCleared()) {
    os << "[weak cleared]";
  } else {
    Object object = maybe_object.GetHeapObjectOrSmi();
    bool weak = maybe_object.IsWeak();

#ifdef DEBUG
    if (object.IsString() && !isolate->context().is_null()) {
      DCHECK(!weak);
      // If we have a string, assume it's a code "marker"
      // and print some interesting cpu debugging info.
      object.Print(os);
      JavaScriptFrameIterator it(isolate);
      JavaScriptFrame* frame = it.frame();
      os << "fp = " << reinterpret_cast<void*>(frame->fp())
         << ", sp = " << reinterpret_cast<void*>(frame->sp())
         << ", caller_sp = " << reinterpret_cast<void*>(frame->caller_sp())
         << ": ";
    } else {
      os << "DebugPrint: ";
      if (weak) {
        os << "[weak] ";
      }
      object.Print(os);
    }
    if (object.IsHeapObject()) {
      HeapObject::cast(object).map().Print(os);
    }
#else
    if (weak) {
      os << "[weak] ";
    }
    // ShortPrint is available in release mode. Print is not.
    os << Brief(object);
#endif
  }
  os << std::endl;

  return args[0];  // return TOS
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
  if (!FLAG_track_retaining_path) {
    PrintF("DebugTrackRetainingPath requires --track-retaining-path flag.\n");
  } else {
    CONVERT_ARG_HANDLE_CHECKED(HeapObject, object, 0);
    RetainingPathOption option = RetainingPathOption::kDefault;
    if (args.length() == 2) {
      CONVERT_ARG_HANDLE_CHECKED(String, str, 1);
      const char track_ephemeron_path[] = "track-ephemeron-path";
      if (str->IsOneByteEqualTo(StaticCharVector(track_ephemeron_path))) {
        option = RetainingPathOption::kTrackEphemeronPath;
      } else if (str->length() != 0) {
        PrintF("Unexpected second argument of DebugTrackRetainingPath.\n");
        PrintF("Expected an empty string or '%s', got '%s'.\n",
               track_ephemeron_path, str->ToCString().get());
      }
    }
    isolate->heap()->AddRetainingPathTarget(object, option);
  }
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
  if (!func->is_compiled() &&
      !Compiler::Compile(func, Compiler::KEEP_EXCEPTION, &is_compiled_scope)) {
    return ReadOnlyRoots(isolate).exception();
  }
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

void PrintIndentation(Isolate* isolate) {
  const int nmax = 80;
  int n = StackSize(isolate);
  if (n <= nmax) {
    PrintF("%4d:%*s", n, n, "");
  } else {
    PrintF("%4d:%*s", n, nmax, "...");
  }
}

}  // namespace

RUNTIME_FUNCTION(Runtime_TraceEnter) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  PrintIndentation(isolate);
  JavaScriptFrame::PrintTop(isolate, stdout, true, false);
  PrintF(" {\n");
  return ReadOnlyRoots(isolate).undefined_value();
}


RUNTIME_FUNCTION(Runtime_TraceExit) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, obj, 0);
  PrintIndentation(isolate);
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
    // Doesn't have wasm data.
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
bool DisallowCodegenFromStringsCallback(v8::Local<v8::Context> context,
                                        v8::Local<v8::String> source) {
  return false;
}
}

RUNTIME_FUNCTION(Runtime_DisallowCodegenFromStrings) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_BOOLEAN_ARG_CHECKED(flag, 0);
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  v8_isolate->SetAllowCodeGenerationFromStringsCallback(
      flag ? DisallowCodegenFromStringsCallback : nullptr);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_DisallowWasmCodegen) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_BOOLEAN_ARG_CHECKED(flag, 0);
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  v8_isolate->SetAllowWasmCodeGenerationCallback(
      flag ? DisallowCodegenFromStringsCallback : nullptr);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_IsWasmCode) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(JSFunction, function, 0);
  bool is_js_to_wasm = function.code().kind() == Code::JS_TO_WASM_FUNCTION;
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
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, exception, 0);
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 1);
  Handle<Object> tag =
      WasmExceptionPackage::GetExceptionTag(isolate, exception);
  if (tag->IsWasmExceptionTag()) {
    Handle<FixedArray> exceptions_table(instance->exceptions_table(), isolate);
    for (int index = 0; index < exceptions_table->length(); ++index) {
      if (exceptions_table->get(index) == *tag) return Smi::FromInt(index);
    }
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_GetWasmExceptionValues) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, exception, 0);
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
  return isolate->heap()->ToBoolean(isolate->IsArraySpeciesLookupChainIntact());
}

RUNTIME_FUNCTION(Runtime_MapIteratorProtector) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(isolate->IsMapIteratorLookupChainIntact());
}

RUNTIME_FUNCTION(Runtime_SetIteratorProtector) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(isolate->IsSetIteratorLookupChainIntact());
}

RUNTIME_FUNCTION(Runtime_StringIteratorProtector) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(
      isolate->IsStringIteratorLookupChainIntact());
}

// Take a compiled wasm module and serialize it into an array buffer, which is
// then returned.
RUNTIME_FUNCTION(Runtime_SerializeWasmModule) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmModuleObject, module_obj, 0);

  wasm::NativeModule* native_module = module_obj->native_module();
  wasm::WasmSerializer wasm_serializer(native_module);
  size_t compiled_size = wasm_serializer.GetSerializedNativeModuleSize();
  void* array_data = isolate->array_buffer_allocator()->Allocate(compiled_size);
  Handle<JSArrayBuffer> array_buffer =
      isolate->factory()->NewJSArrayBuffer(SharedFlag::kNotShared);
  JSArrayBuffer::Setup(array_buffer, isolate, false, array_data, compiled_size);
  if (!array_data ||
      !wasm_serializer.SerializeNativeModule(
          {reinterpret_cast<uint8_t*>(array_data), compiled_size})) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
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
      wasm::DeserializeNativeModule(isolate, buffer_vec, wire_bytes_vec);
  Handle<WasmModuleObject> module_object;
  if (!maybe_module_object.ToHandle(&module_object)) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
  return *module_object;
}

// Create a new Module object using the same NativeModule.
RUNTIME_FUNCTION(Runtime_CloneWasmModule) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmModuleObject, module_object, 0);

  Handle<WasmModuleObject> new_module_object =
      wasm::WasmEngine::GetWasmEngine()->ImportNativeModule(
          isolate, module_object->shared_native_module());

  return *new_module_object;
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

RUNTIME_FUNCTION(Runtime_WasmGetNumberOfInstances) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmModuleObject, module_obj, 0);
  int instance_count = 0;
  WeakArrayList weak_instance_list = module_obj->weak_instance_list();
  for (int i = 0; i < weak_instance_list.length(); ++i) {
    if (weak_instance_list.Get(i)->IsWeak()) instance_count++;
  }
  return Smi::FromInt(instance_count);
}

RUNTIME_FUNCTION(Runtime_WasmNumInterpretedCalls) {
  DCHECK_EQ(1, args.length());
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  if (!instance->has_debug_info()) return Object();
  uint64_t num = instance->debug_info().NumInterpretedCalls();
  return *isolate->factory()->NewNumberFromSize(static_cast<size_t>(num));
}

RUNTIME_FUNCTION(Runtime_RedirectToWasmInterpreter) {
  DCHECK_EQ(2, args.length());
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_SMI_ARG_CHECKED(function_index, 1);
  Handle<WasmDebugInfo> debug_info =
      WasmInstanceObject::GetOrCreateDebugInfo(instance);
  WasmDebugInfo::RedirectToInterpreter(debug_info,
                                       Vector<int>(&function_index, 1));
  return ReadOnlyRoots(isolate).undefined_value();
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
  WasmCompiledFrame* frame = WasmCompiledFrame::cast(it.frame());

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
  // Always lowered to StaticAssert node in Turbofan, so we should never get
  // here in compiled code.
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_EnableCodeLoggingForTesting) {
  // The {NoopListener} currently does nothing on any callback, but reports
  // {true} on {is_listening_to_code_events()}. Feel free to add assertions to
  // any method to further test the code logging callbacks.
  class NoopListener final : public CodeEventListener {
    void CodeCreateEvent(LogEventsAndTags tag, AbstractCode code,
                         const char* comment) final {}
    void CodeCreateEvent(LogEventsAndTags tag, AbstractCode code,
                         Name name) final {}
    void CodeCreateEvent(LogEventsAndTags tag, AbstractCode code,
                         SharedFunctionInfo shared, Name source) final {}
    void CodeCreateEvent(LogEventsAndTags tag, AbstractCode code,
                         SharedFunctionInfo shared, Name source, int line,
                         int column) final {}
    void CodeCreateEvent(LogEventsAndTags tag, const wasm::WasmCode* code,
                         wasm::WasmName name) final {}
    void CallbackEvent(Name name, Address entry_point) final {}
    void GetterCallbackEvent(Name name, Address entry_point) final {}
    void SetterCallbackEvent(Name name, Address entry_point) final {}
    void RegExpCodeCreateEvent(AbstractCode code, String source) final {}
    void CodeMoveEvent(AbstractCode from, AbstractCode to) final {}
    void SharedFunctionInfoMoveEvent(Address from, Address to) final {}
    void CodeMovingGCEvent() final {}
    void CodeDisableOptEvent(AbstractCode code,
                             SharedFunctionInfo shared) final {}
    void CodeDeoptEvent(Code code, DeoptimizeKind kind, Address pc,
                        int fp_to_sp_delta) final {}

    bool is_listening_to_code_events() final { return true; }
  };
  static base::LeakyObject<NoopListener> noop_listener;
  isolate->wasm_engine()->EnableCodeLogging(isolate);
  isolate->code_event_dispatcher()->AddListener(noop_listener.get());
  return ReadOnlyRoots(isolate).undefined_value();
}

}  // namespace internal
}  // namespace v8

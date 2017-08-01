// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include <memory>

#include "src/arguments.h"
#include "src/assembler-inl.h"
#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"
#include "src/compiler.h"
#include "src/deoptimizer.h"
#include "src/frames-inl.h"
#include "src/full-codegen/full-codegen.h"
#include "src/isolate-inl.h"
#include "src/runtime-profiler.h"
#include "src/snapshot/code-serializer.h"
#include "src/snapshot/natives.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"

namespace {
struct WasmCompileControls {
  uint32_t MaxWasmBufferSize = std::numeric_limits<uint32_t>::max();
  bool AllowAnySizeForAsync = true;
};

// We need per-isolate controls, because we sometimes run tests in multiple
// isolates
// concurrently.
// To avoid upsetting the static initializer count, we lazy initialize this.
v8::base::LazyInstance<std::map<v8::Isolate*, WasmCompileControls>>::type
    g_PerIsolateWasmControls = LAZY_INSTANCE_INITIALIZER;

bool IsWasmCompileAllowed(v8::Isolate* isolate, v8::Local<v8::Value> value,
                          bool is_async) {
  DCHECK_GT(g_PerIsolateWasmControls.Get().count(isolate), 0);
  const WasmCompileControls& ctrls = g_PerIsolateWasmControls.Get().at(isolate);
  return (is_async && ctrls.AllowAnySizeForAsync) ||
         (v8::Local<v8::ArrayBuffer>::Cast(value)->ByteLength() <=
          ctrls.MaxWasmBufferSize);
}

// Use the compile controls for instantiation, too
bool IsWasmInstantiateAllowed(v8::Isolate* isolate,
                              v8::Local<v8::Value> module_or_bytes,
                              bool is_async) {
  DCHECK_GT(g_PerIsolateWasmControls.Get().count(isolate), 0);
  const WasmCompileControls& ctrls = g_PerIsolateWasmControls.Get().at(isolate);
  if (is_async && ctrls.AllowAnySizeForAsync) return true;
  if (!module_or_bytes->IsWebAssemblyCompiledModule()) {
    return IsWasmCompileAllowed(isolate, module_or_bytes, is_async);
  }
  v8::Local<v8::WasmCompiledModule> module =
      v8::Local<v8::WasmCompiledModule>::Cast(module_or_bytes);
  return static_cast<uint32_t>(module->GetWasmWireBytes()->Length()) <=
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

void RejectPromiseWithRangeError(
    const v8::FunctionCallbackInfo<v8::Value>& args, const char* message) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver;
  if (!v8::Promise::Resolver::New(context).ToLocal(&resolver)) return;
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(resolver->GetPromise());

  auto maybe = resolver->Reject(context, NewRangeException(isolate, message));
  CHECK(!maybe.IsNothing());
  return;
}

bool WasmModuleOverride(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (IsWasmCompileAllowed(args.GetIsolate(), args[0], false)) return false;
  ThrowRangeException(args.GetIsolate(), "Sync compile not allowed");
  return true;
}

bool WasmCompileOverride(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (IsWasmCompileAllowed(args.GetIsolate(), args[0], true)) return false;
  RejectPromiseWithRangeError(args, "Async compile not allowed");
  return true;
}

bool WasmInstanceOverride(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (IsWasmInstantiateAllowed(args.GetIsolate(), args[0], false)) return false;
  ThrowRangeException(args.GetIsolate(), "Sync instantiate not allowed");
  return true;
}

bool WasmInstantiateOverride(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (IsWasmInstantiateAllowed(args.GetIsolate(), args[0], true)) return false;
  RejectPromiseWithRangeError(args, "Async instantiate not allowed");
  return true;
}

bool GetWasmFromArray(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1);
  v8::Local<v8::Context> context = args.GetIsolate()->GetCurrentContext();
  v8::Local<v8::Value> module =
      v8::Local<v8::Object>::Cast(args[0])->Get(context, 0).ToLocalChecked();

  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(context).ToLocalChecked();
  args.GetReturnValue().Set(resolver->GetPromise());
  USE(resolver->Resolve(context, module));
  return true;
}

bool NoExtension(const v8::FunctionCallbackInfo<v8::Value>&) { return false; }

}  // namespace

namespace v8 {
namespace internal {

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

RUNTIME_FUNCTION(Runtime_DeoptimizeFunction) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());

  // This function is used by fuzzers to get coverage in compiler.
  // Ignore calls on non-function objects to avoid runtime errors.
  CONVERT_ARG_HANDLE_CHECKED(Object, function_object, 0);
  if (!function_object->IsJSFunction()) {
    return isolate->heap()->undefined_value();
  }
  Handle<JSFunction> function = Handle<JSFunction>::cast(function_object);
  function->shared()->set_marked_for_tier_up(false);

  // If the function is not optimized, just return.
  if (!function->IsOptimized()) return isolate->heap()->undefined_value();

  // TODO(turbofan): Deoptimization is not supported yet.
  if (function->code()->is_turbofanned() &&
      function->shared()->asm_function()) {
    return isolate->heap()->undefined_value();
  }

  Deoptimizer::DeoptimizeFunction(*function);

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DeoptimizeNow) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());

  Handle<JSFunction> function;

  // Find the JavaScript function on the top of the stack.
  JavaScriptFrameIterator it(isolate);
  if (!it.done()) function = Handle<JSFunction>(it.frame()->function());
  if (function.is_null()) return isolate->heap()->undefined_value();

  // If the function is not optimized, just return.
  if (!function->IsOptimized()) return isolate->heap()->undefined_value();

  // TODO(turbofan): Deoptimization is not supported yet.
  if (function->code()->is_turbofanned() &&
      function->shared()->asm_function()) {
    return isolate->heap()->undefined_value();
  }

  Deoptimizer::DeoptimizeFunction(*function);

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_RunningInSimulator) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
#if defined(USE_SIMULATOR)
  return isolate->heap()->true_value();
#else
  return isolate->heap()->false_value();
#endif
}


RUNTIME_FUNCTION(Runtime_IsConcurrentRecompilationSupported) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(
      isolate->concurrent_recompilation_enabled());
}

RUNTIME_FUNCTION(Runtime_TypeProfile) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());

  if (!FLAG_type_profile) {
    return isolate->heap()->undefined_value();
  }

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  if (function->has_feedback_vector()) {
    FeedbackVector* vector = function->feedback_vector();
    if (vector->metadata()->HasTypeProfileSlot()) {
      FeedbackSlot slot = vector->GetTypeProfileSlot();
      CollectTypeProfileNexus nexus(vector, slot);
      return nexus.GetTypeProfile();
    }
  }
  return *isolate->factory()->NewJSObject(isolate->object_function());
}

RUNTIME_FUNCTION(Runtime_OptimizeFunctionOnNextCall) {
  HandleScope scope(isolate);

  // This function is used by fuzzers, ignore calls with bogus arguments count.
  if (args.length() != 1 && args.length() != 2) {
    return isolate->heap()->undefined_value();
  }

  // This function is used by fuzzers to get coverage for optimizations
  // in compiler. Ignore calls on non-function objects to avoid runtime errors.
  CONVERT_ARG_HANDLE_CHECKED(Object, function_object, 0);
  if (!function_object->IsJSFunction()) {
    return isolate->heap()->undefined_value();
  }
  Handle<JSFunction> function = Handle<JSFunction>::cast(function_object);

  // The following condition was lifted from the DCHECK inside
  // JSFunction::MarkForOptimization().
  if (!(function->shared()->allows_lazy_compilation() ||
        (function->code()->kind() == Code::FUNCTION &&
         !function->shared()->optimization_disabled()))) {
    return isolate->heap()->undefined_value();
  }

  // If function isn't compiled, compile it now.
  if (!function->shared()->is_compiled() &&
      !Compiler::Compile(function, Compiler::CLEAR_EXCEPTION)) {
    return isolate->heap()->undefined_value();
  }

  // If the function is already optimized, just return.
  if (function->IsOptimized()) return isolate->heap()->undefined_value();

  function->MarkForOptimization();
  if (FLAG_trace_opt) {
    PrintF("[manually marking ");
    function->ShortPrint();
    PrintF(" for optimization]\n");
  }

  if (args.length() == 2) {
    CONVERT_ARG_HANDLE_CHECKED(String, type, 1);
    if (type->IsOneByteEqualTo(STATIC_CHAR_VECTOR("concurrent")) &&
        isolate->concurrent_recompilation_enabled()) {
      function->AttemptConcurrentOptimization();
    }
  }

  return isolate->heap()->undefined_value();
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
  if (!it.done()) function = Handle<JSFunction>(it.frame()->function());
  if (function.is_null()) return isolate->heap()->undefined_value();

  // If the function is already optimized, just return.
  if (function->IsOptimized()) return isolate->heap()->undefined_value();

  // Make the profiler arm all back edges in unoptimized code.
  if (it.frame()->type() == StackFrame::JAVA_SCRIPT ||
      it.frame()->type() == StackFrame::INTERPRETED) {
    isolate->runtime_profiler()->AttemptOnStackReplacement(
        it.frame(), AbstractCode::kMaxLoopNestingMarker);
  }

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_NeverOptimizeFunction) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(JSFunction, function, 0);
  function->shared()->set_disable_optimization_reason(
      kOptimizationDisabledForTest);
  function->shared()->set_optimization_disabled(true);
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_GetOptimizationStatus) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1 || args.length() == 2);
  int status = 0;
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
    if (!sync_object->IsString()) return isolate->heap()->undefined_value();
    Handle<String> sync = Handle<String>::cast(sync_object);
    if (sync->IsOneByteEqualTo(STATIC_CHAR_VECTOR("no sync"))) {
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
  if (function->IsOptimized()) {
    status |= static_cast<int>(OptimizationStatus::kOptimized);
    if (function->code()->is_turbofanned()) {
      status |= static_cast<int>(OptimizationStatus::kTurboFanned);
    }
  }
  if (function->IsInterpreted()) {
    status |= static_cast<int>(OptimizationStatus::kInterpreted);
  }
  return Smi::FromInt(status);
}


RUNTIME_FUNCTION(Runtime_UnblockConcurrentRecompilation) {
  DCHECK_EQ(0, args.length());
  if (FLAG_block_concurrent_recompilation &&
      isolate->concurrent_recompilation_enabled()) {
    isolate->optimizing_compile_dispatcher()->Unblock();
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_GetOptimizationCount) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  return Smi::FromInt(function->shared()->opt_count());
}

RUNTIME_FUNCTION(Runtime_GetDeoptCount) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  return Smi::FromInt(function->shared()->deopt_count());
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
    return nullptr;
  }
  return *Utils::OpenHandle(*obj);
}

static void call_as_function(const v8::FunctionCallbackInfo<v8::Value>& args) {
  double v1 = args[0]
                  ->NumberValue(v8::Isolate::GetCurrent()->GetCurrentContext())
                  .ToChecked();
  double v2 = args[1]
                  ->NumberValue(v8::Isolate::GetCurrent()->GetCurrentContext())
                  .ToChecked();
  args.GetReturnValue().Set(
      v8::Number::New(v8::Isolate::GetCurrent(), v1 - v2));
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
  Code* unoptimized = function->shared()->code();
  if (unoptimized->kind() == Code::FUNCTION) {
    unoptimized->ClearInlineCaches();
  }
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_SetWasmCompileFromPromiseOverload) {
  isolate->set_wasm_compile_callback(GetWasmFromArray);
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_ResetWasmOverloads) {
  isolate->set_wasm_compile_callback(NoExtension);
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_CheckWasmWrapperElision) {
  // This only supports the case where the function being exported
  // calls an intermediate function, and the intermediate function
  // calls exactly one imported function
  HandleScope scope(isolate);
  CHECK(args.length() == 2);
  // It takes two parameters, the first one is the JSFunction,
  // The second one is the type
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  // If type is 0, it means that it is supposed to be a direct call into a WASM
  // function
  // If type is 1, it means that it is supposed to have wrappers
  CONVERT_ARG_HANDLE_CHECKED(Smi, type, 1);
  Handle<Code> export_code = handle(function->code());
  CHECK(export_code->kind() == Code::JS_TO_WASM_FUNCTION);
  int const mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET);
  // check the type of the $export_fct
  Handle<Code> export_fct;
  int count = 0;
  for (RelocIterator it(*export_code, mask); !it.done(); it.next()) {
    RelocInfo* rinfo = it.rinfo();
    Address target_address = rinfo->target_address();
    Code* target = Code::GetCodeFromTargetAddress(target_address);
    if (target->kind() == Code::WASM_FUNCTION) {
      ++count;
      export_fct = handle(target);
    }
  }
  CHECK(count == 1);
  // check the type of the intermediate_fct
  Handle<Code> intermediate_fct;
  count = 0;
  for (RelocIterator it(*export_fct, mask); !it.done(); it.next()) {
    RelocInfo* rinfo = it.rinfo();
    Address target_address = rinfo->target_address();
    Code* target = Code::GetCodeFromTargetAddress(target_address);
    if (target->kind() == Code::WASM_FUNCTION) {
      ++count;
      intermediate_fct = handle(target);
    }
  }
  CHECK(count == 1);
  // check the type of the imported exported function, it should be also a WASM
  // function in our case
  Handle<Code> imported_fct;
  CHECK(type->value() == 0 || type->value() == 1);

  Code::Kind target_kind =
      type->value() == 0 ? Code::WASM_FUNCTION : Code::WASM_TO_JS_FUNCTION;
  count = 0;
  for (RelocIterator it(*intermediate_fct, mask); !it.done(); it.next()) {
    RelocInfo* rinfo = it.rinfo();
    Address target_address = rinfo->target_address();
    Code* target = Code::GetCodeFromTargetAddress(target_address);
    if (target->kind() == target_kind) {
      ++count;
      imported_fct = handle(target);
    }
  }
  CHECK_LE(count, 1);
  return isolate->heap()->ToBoolean(count == 1);
}

RUNTIME_FUNCTION(Runtime_SetWasmCompileControls) {
  HandleScope scope(isolate);
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  CHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(Smi, block_size, 0);
  CONVERT_BOOLEAN_ARG_CHECKED(allow_async, 1);
  WasmCompileControls& ctrl = (*g_PerIsolateWasmControls.Pointer())[v8_isolate];
  ctrl.AllowAnySizeForAsync = allow_async;
  ctrl.MaxWasmBufferSize = static_cast<uint32_t>(block_size->value());
  v8_isolate->SetWasmModuleCallback(WasmModuleOverride);
  v8_isolate->SetWasmCompileCallback(WasmCompileOverride);
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_SetWasmInstantiateControls) {
  HandleScope scope(isolate);
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  CHECK(args.length() == 0);
  v8_isolate->SetWasmInstanceCallback(WasmInstanceOverride);
  v8_isolate->SetWasmInstantiateCallback(WasmInstantiateOverride);
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_NotifyContextDisposed) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  isolate->heap()->NotifyContextDisposed(true);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_SetAllocationTimeout) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2 || args.length() == 3);
#ifdef DEBUG
  CONVERT_INT32_ARG_CHECKED(interval, 0);
  CONVERT_INT32_ARG_CHECKED(timeout, 1);
  isolate->heap()->set_allocation_timeout(timeout);
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
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DebugPrint) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());

  OFStream os(stdout);
#ifdef DEBUG
  if (args[0]->IsString() && isolate->context() != nullptr) {
    // If we have a string, assume it's a code "marker"
    // and print some interesting cpu debugging info.
    args[0]->Print(os);
    JavaScriptFrameIterator it(isolate);
    JavaScriptFrame* frame = it.frame();
    os << "fp = " << static_cast<void*>(frame->fp())
       << ", sp = " << static_cast<void*>(frame->sp())
       << ", caller_sp = " << static_cast<void*>(frame->caller_sp()) << ": ";
  } else {
    os << "DebugPrint: ";
    args[0]->Print(os);
  }
  if (args[0]->IsHeapObject()) {
    HeapObject::cast(args[0])->map()->Print(os);
  }
#else
  // ShortPrint is available in release mode. Print is not.
  os << Brief(args[0]);
#endif
  os << std::endl;

  return args[0];  // return TOS
}


RUNTIME_FUNCTION(Runtime_DebugTrace) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  isolate->PrintStack(stdout);
  return isolate->heap()->undefined_value();
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
  return isolate->heap()->undefined_value();
}


// Sets a v8 flag.
RUNTIME_FUNCTION(Runtime_SetFlags) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(String, arg, 0);
  std::unique_ptr<char[]> flags =
      arg->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  FlagList::SetFlagsFromString(flags.get(), StrLength(flags.get()));
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_Abort) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_SMI_ARG_CHECKED(message_id, 0);
  const char* message =
      GetBailoutReason(static_cast<BailoutReason>(message_id));
  base::OS::PrintError("abort: %s\n", message);
  isolate->PrintStack(stderr);
  base::OS::Abort();
  UNREACHABLE();
  return NULL;
}


RUNTIME_FUNCTION(Runtime_AbortJS) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, message, 0);
  base::OS::PrintError("abort: %s\n", message->ToCString().get());
  isolate->PrintStack(stderr);
  base::OS::Abort();
  UNREACHABLE();
  return NULL;
}


RUNTIME_FUNCTION(Runtime_NativeScriptsCount) {
  DCHECK_EQ(0, args.length());
  return Smi::FromInt(Natives::GetBuiltinsCount());
}


RUNTIME_FUNCTION(Runtime_DisassembleFunction) {
  HandleScope scope(isolate);
#ifdef DEBUG
  DCHECK_EQ(1, args.length());
  // Get the function and make sure it is compiled.
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, func, 0);
  if (!Compiler::Compile(func, Compiler::KEEP_EXCEPTION)) {
    return isolate->heap()->exception();
  }
  OFStream os(stdout);
  func->code()->Print(os);
  os << std::endl;
#endif  // DEBUG
  return isolate->heap()->undefined_value();
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
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_TraceExit) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, obj, 0);
  PrintIndentation(isolate);
  PrintF("} -> ");
  obj->ShortPrint();
  PrintF("\n");
  return obj;  // return TOS
}

RUNTIME_FUNCTION(Runtime_TraceTailCall) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  PrintIndentation(isolate);
  PrintF("} -> tail call ->\n");
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_GetExceptionDetails) {
  HandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, exception_obj, 0);

  Factory* factory = isolate->factory();
  Handle<JSMessageObject> message_obj =
      isolate->CreateMessage(exception_obj, nullptr);

  Handle<JSObject> message = factory->NewJSObject(isolate->object_function());

  Handle<String> key;
  Handle<Object> value;

  key = factory->NewStringFromAsciiChecked("start_pos");
  value = handle(Smi::FromInt(message_obj->start_position()), isolate);
  JSObject::SetProperty(message, key, value, STRICT).Assert();

  key = factory->NewStringFromAsciiChecked("end_pos");
  value = handle(Smi::FromInt(message_obj->end_position()), isolate);
  JSObject::SetProperty(message, key, value, STRICT).Assert();

  return *message;
}

RUNTIME_FUNCTION(Runtime_HaveSameMap) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_CHECKED(JSObject, obj1, 0);
  CONVERT_ARG_CHECKED(JSObject, obj2, 1);
  return isolate->heap()->ToBoolean(obj1->map() == obj2->map());
}


RUNTIME_FUNCTION(Runtime_InNewSpace) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(isolate->heap()->InNewSpace(obj));
}

RUNTIME_FUNCTION(Runtime_IsAsmWasmCode) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(JSFunction, function, 0);
  if (!function->shared()->HasAsmWasmData()) {
    // Doesn't have wasm data.
    return isolate->heap()->false_value();
  }
  if (function->shared()->code() !=
      isolate->builtins()->builtin(Builtins::kInstantiateAsmJs)) {
    // Hasn't been compiled yet.
    return isolate->heap()->false_value();
  }
  return isolate->heap()->true_value();
}

namespace {
bool DisallowCodegenFromStringsCallback(v8::Local<v8::Context> context) {
  return false;
}
}

RUNTIME_FUNCTION(Runtime_DisallowCodegenFromStrings) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  v8_isolate->SetAllowCodeGenerationFromStringsCallback(
      DisallowCodegenFromStringsCallback);
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_IsWasmCode) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(JSFunction, function, 0);
  bool is_js_to_wasm = function->code()->kind() == Code::JS_TO_WASM_FUNCTION;
  return isolate->heap()->ToBoolean(is_js_to_wasm);
}

#define ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(Name)       \
  RUNTIME_FUNCTION(Runtime_Has##Name) {                  \
    CONVERT_ARG_CHECKED(JSObject, obj, 0);               \
    return isolate->heap()->ToBoolean(obj->Has##Name()); \
  }

ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastSmiElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastObjectElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastSmiOrObjectElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastDoubleElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastHoleyElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(DictionaryElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(SloppyArgumentsElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FixedTypedArrayElements)
// Properties test sitting with elements tests - not fooling anyone.
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastProperties)

#undef ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION


#define FIXED_TYPED_ARRAYS_CHECK_RUNTIME_FUNCTION(Type, type, TYPE, ctype, s) \
  RUNTIME_FUNCTION(Runtime_HasFixed##Type##Elements) {                        \
    CONVERT_ARG_CHECKED(JSObject, obj, 0);                                    \
    return isolate->heap()->ToBoolean(obj->HasFixed##Type##Elements());       \
  }

TYPED_ARRAYS(FIXED_TYPED_ARRAYS_CHECK_RUNTIME_FUNCTION)

#undef FIXED_TYPED_ARRAYS_CHECK_RUNTIME_FUNCTION


RUNTIME_FUNCTION(Runtime_SpeciesProtector) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(isolate->IsArraySpeciesLookupChainIntact());
}

#define CONVERT_ARG_HANDLE_CHECKED_2(Type, name, index) \
  CHECK(Type::Is##Type(args[index]));                   \
  Handle<Type> name = args.at<Type>(index);

// Take a compiled wasm module, serialize it and copy the buffer into an array
// buffer, which is then returned.
RUNTIME_FUNCTION(Runtime_SerializeWasmModule) {
  HandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED_2(WasmModuleObject, module_obj, 0);

  Handle<WasmCompiledModule> orig(module_obj->compiled_module());
  std::unique_ptr<ScriptData> data =
      WasmCompiledModuleSerializer::SerializeWasmModule(isolate, orig);
  void* buff = isolate->array_buffer_allocator()->Allocate(data->length());
  Handle<JSArrayBuffer> ret = isolate->factory()->NewJSArrayBuffer();
  JSArrayBuffer::Setup(ret, isolate, false, buff, data->length());
  memcpy(buff, data->data(), data->length());
  return *ret;
}

// Take an array buffer and attempt to reconstruct a compiled wasm module.
// Return undefined if unsuccessful.
RUNTIME_FUNCTION(Runtime_DeserializeWasmModule) {
  HandleScope shs(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSArrayBuffer, buffer, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArrayBuffer, wire_bytes, 1);

  Address mem_start = static_cast<Address>(buffer->backing_store());
  int mem_size = static_cast<int>(buffer->byte_length()->Number());

  // DeserializeWasmModule will allocate. We assume JSArrayBuffer doesn't
  // get relocated.
  ScriptData sc(mem_start, mem_size);
  bool already_external = wire_bytes->is_external();
  if (!already_external) {
    wire_bytes->set_is_external(true);
    isolate->heap()->UnregisterArrayBuffer(*wire_bytes);
  }
  MaybeHandle<FixedArray> maybe_compiled_module =
      WasmCompiledModuleSerializer::DeserializeWasmModule(
          isolate, &sc,
          Vector<const uint8_t>(
              reinterpret_cast<uint8_t*>(wire_bytes->backing_store()),
              static_cast<int>(wire_bytes->byte_length()->Number())));
  if (!already_external) {
    wire_bytes->set_is_external(false);
    isolate->heap()->RegisterNewArrayBuffer(*wire_bytes);
  }
  Handle<FixedArray> compiled_module;
  if (!maybe_compiled_module.ToHandle(&compiled_module)) {
    return isolate->heap()->undefined_value();
  }
  return *WasmModuleObject::New(
      isolate, Handle<WasmCompiledModule>::cast(compiled_module));
}

RUNTIME_FUNCTION(Runtime_ValidateWasmInstancesChain) {
  HandleScope shs(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED_2(WasmModuleObject, module_obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Smi, instance_count, 1);
  wasm::testing::ValidateInstancesChain(isolate, module_obj,
                                        instance_count->value());
  return isolate->heap()->ToBoolean(true);
}

RUNTIME_FUNCTION(Runtime_ValidateWasmModuleState) {
  HandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED_2(WasmModuleObject, module_obj, 0);
  wasm::testing::ValidateModuleState(isolate, module_obj);
  return isolate->heap()->ToBoolean(true);
}

RUNTIME_FUNCTION(Runtime_ValidateWasmOrphanedInstance) {
  HandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED_2(WasmInstanceObject, instance, 0);
  wasm::testing::ValidateOrphanedInstance(isolate, instance);
  return isolate->heap()->ToBoolean(true);
}

RUNTIME_FUNCTION(Runtime_HeapObjectVerify) {
  HandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
#ifdef VERIFY_HEAP
  object->ObjectVerify();
#else
  CHECK(object->IsObject());
  if (object->IsHeapObject()) {
    CHECK(HeapObject::cast(*object)->map()->IsMap());
  } else {
    CHECK(object->IsSmi());
  }
#endif
  return isolate->heap()->ToBoolean(true);
}

RUNTIME_FUNCTION(Runtime_WasmNumInterpretedCalls) {
  DCHECK_EQ(1, args.length());
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, instance_obj, 0);
  CHECK(WasmInstanceObject::IsWasmInstanceObject(*instance_obj));
  Handle<WasmInstanceObject> instance =
      Handle<WasmInstanceObject>::cast(instance_obj);
  if (!instance->has_debug_info()) return 0;
  uint64_t num = instance->debug_info()->NumInterpretedCalls();
  return *isolate->factory()->NewNumberFromSize(static_cast<size_t>(num));
}

RUNTIME_FUNCTION(Runtime_RedirectToWasmInterpreter) {
  DCHECK_EQ(2, args.length());
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, instance_obj, 0);
  CONVERT_SMI_ARG_CHECKED(function_index, 1);
  CHECK(WasmInstanceObject::IsWasmInstanceObject(*instance_obj));
  Handle<WasmInstanceObject> instance =
      Handle<WasmInstanceObject>::cast(instance_obj);
  Handle<WasmDebugInfo> debug_info =
      WasmInstanceObject::GetOrCreateDebugInfo(instance);
  WasmDebugInfo::RedirectToInterpreter(debug_info,
                                       Vector<int>(&function_index, 1));
  return isolate->heap()->undefined_value();
}

}  // namespace internal
}  // namespace v8

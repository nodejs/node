// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-wasm.h"
#include "src/base/memory.h"
#include "src/base/platform/mutex.h"
#include "src/execution/arguments-inl.h"
#include "src/execution/frames-inl.h"
#include "src/heap/heap-inl.h"
#include "src/objects/smi.h"
#include "src/trap-handler/trap-handler.h"
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
         (value->IsArrayBuffer() && value.As<v8::ArrayBuffer>()->ByteLength() <=
                                        ctrls.MaxWasmBufferSize) ||
         (value->IsArrayBufferView() &&
          value.As<v8::ArrayBufferView>()->ByteLength() <=
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

}  // namespace

// Returns a callable object. The object returns the difference of its two
// parameters when it is called.
RUNTIME_FUNCTION(Runtime_SetWasmCompileControls) {
  HandleScope scope(isolate);
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  CHECK_EQ(args.length(), 2);
  int block_size = args.smi_value_at(0);
  bool allow_async = Oddball::cast(args[1]).ToBool(isolate);
  base::MutexGuard guard(g_PerIsolateWasmControlsMutex.Pointer());
  WasmCompileControls& ctrl = (*GetPerIsolateWasmControls())[v8_isolate];
  ctrl.AllowAnySizeForAsync = allow_async;
  ctrl.MaxWasmBufferSize = static_cast<uint32_t>(block_size);
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

namespace {

void PrintIndentation(int stack_size) {
  const int max_display = 80;
  if (stack_size <= max_display) {
    PrintF("%4d:%*s", stack_size, stack_size, "");
  } else {
    PrintF("%4d:%*s", stack_size, max_display, "...");
  }
}

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
      module->lazily_generated_names.LookupFunctionName(wire_bytes, func_index);
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
  auto value_addr_smi = Smi::cast(args[0]);

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
      case wasm::kI32: {
        int32_t value = base::ReadUnalignedValue<int32_t>(value_addr_smi.ptr());
        PrintF(" -> %d\n", value);
        break;
      }
      case wasm::kI64: {
        int64_t value = base::ReadUnalignedValue<int64_t>(value_addr_smi.ptr());
        PrintF(" -> %" PRId64 "\n", value);
        break;
      }
      case wasm::kF32: {
        float value = base::ReadUnalignedValue<float>(value_addr_smi.ptr());
        PrintF(" -> %f\n", value);
        break;
      }
      case wasm::kF64: {
        double value = base::ReadUnalignedValue<double>(value_addr_smi.ptr());
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

RUNTIME_FUNCTION(Runtime_IsAsmWasmCode) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  auto function = JSFunction::cast(args[0]);
  if (!function.shared().HasAsmWasmData()) {
    return ReadOnlyRoots(isolate).false_value();
  }
  if (function.shared().HasBuiltinId() &&
      function.shared().builtin_id() == Builtin::kInstantiateAsmJs) {
    // Hasn't been compiled yet.
    return ReadOnlyRoots(isolate).false_value();
  }
  return ReadOnlyRoots(isolate).true_value();
}

namespace {

bool DisallowWasmCodegenFromStringsCallback(v8::Local<v8::Context> context,
                                            v8::Local<v8::String> source) {
  return false;
}

}  // namespace

RUNTIME_FUNCTION(Runtime_DisallowWasmCodegen) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  bool flag = Oddball::cast(args[0]).ToBool(isolate);
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  v8_isolate->SetAllowWasmCodeGenerationCallback(
      flag ? DisallowWasmCodegenFromStringsCallback : nullptr);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_IsWasmCode) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  auto function = JSFunction::cast(args[0]);
  CodeT code = function.code();
  bool is_js_to_wasm = code.kind() == CodeKind::JS_TO_WASM_FUNCTION ||
                       (code.builtin_id() == Builtin::kGenericJSToWasmWrapper);
  return isolate->heap()->ToBoolean(is_js_to_wasm);
}

RUNTIME_FUNCTION(Runtime_IsWasmTrapHandlerEnabled) {
  DisallowGarbageCollection no_gc;
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(trap_handler::IsTrapHandlerEnabled());
}

RUNTIME_FUNCTION(Runtime_IsThreadInWasm) {
  DisallowGarbageCollection no_gc;
  DCHECK_EQ(0, args.length());
  return isolate->heap()->ToBoolean(trap_handler::IsThreadInWasm());
}

RUNTIME_FUNCTION(Runtime_GetWasmRecoveredTrapCount) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  size_t trap_count = trap_handler::GetRecoveredTrapCount();
  return *isolate->factory()->NewNumberFromSize(trap_count);
}

RUNTIME_FUNCTION(Runtime_GetWasmExceptionTagId) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<WasmExceptionPackage> exception = args.at<WasmExceptionPackage>(0);
  Handle<WasmInstanceObject> instance = args.at<WasmInstanceObject>(1);
  Handle<Object> tag =
      WasmExceptionPackage::GetExceptionTag(isolate, exception);
  CHECK(tag->IsWasmExceptionTag());
  Handle<FixedArray> tags_table(instance->tags_table(), isolate);
  for (int index = 0; index < tags_table->length(); ++index) {
    if (tags_table->get(index) == *tag) return Smi::FromInt(index);
  }
  UNREACHABLE();
}

RUNTIME_FUNCTION(Runtime_GetWasmExceptionValues) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<WasmExceptionPackage> exception = args.at<WasmExceptionPackage>(0);
  Handle<Object> values_obj =
      WasmExceptionPackage::GetExceptionValues(isolate, exception);
  CHECK(values_obj->IsFixedArray());  // Only called with correct input.
  Handle<FixedArray> values = Handle<FixedArray>::cast(values_obj);
  return *isolate->factory()->NewJSArrayWithElements(values);
}

RUNTIME_FUNCTION(Runtime_SerializeWasmModule) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<WasmModuleObject> module_obj = args.at<WasmModuleObject>(0);

  wasm::NativeModule* native_module = module_obj->native_module();
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
  Handle<JSArrayBuffer> buffer = args.at<JSArrayBuffer>(0);
  Handle<JSTypedArray> wire_bytes = args.at<JSTypedArray>(1);
  CHECK(!buffer->was_detached());
  CHECK(!wire_bytes->WasDetached());

  Handle<JSArrayBuffer> wire_bytes_buffer = wire_bytes->GetBuffer();
  base::Vector<const uint8_t> wire_bytes_vec{
      reinterpret_cast<const uint8_t*>(wire_bytes_buffer->backing_store()) +
          wire_bytes->byte_offset(),
      wire_bytes->byte_length()};
  base::Vector<uint8_t> buffer_vec{
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

RUNTIME_FUNCTION(Runtime_WasmGetNumberOfInstances) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  Handle<WasmModuleObject> module_obj = args.at<WasmModuleObject>(0);
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
  Handle<JSObject> argument = args.at<JSObject>(0);
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
  auto info_addr = Smi::cast(args[0]);

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
  wasm::ExecutionTier tier = frame->wasm_code()->is_liftoff()
                                 ? wasm::ExecutionTier::kLiftoff
                                 : wasm::ExecutionTier::kTurbofan;
  wasm::TraceMemoryOperation(tier, info, func_index, pos, mem_start);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmTierUpFunction) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<WasmInstanceObject> instance = args.at<WasmInstanceObject>(0);
  int function_index = args.smi_value_at(1);
  wasm::TierUpNowForTesting(isolate, *instance, function_index);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmTierDown) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  wasm::GetWasmEngine()->TierDownAllModulesPerIsolate(isolate);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmTierUp) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  wasm::GetWasmEngine()->TierUpAllModulesPerIsolate(isolate);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_IsLiftoffFunction) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<JSFunction> function = args.at<JSFunction>(0);
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

RUNTIME_FUNCTION(Runtime_IsTurboFanFunction) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<JSFunction> function = args.at<JSFunction>(0);
  CHECK(WasmExportedFunction::IsWasmExportedFunction(*function));
  Handle<WasmExportedFunction> exp_fun =
      Handle<WasmExportedFunction>::cast(function);
  wasm::NativeModule* native_module =
      exp_fun->instance().module_object().native_module();
  uint32_t func_index = exp_fun->function_index();
  wasm::WasmCodeRefScope code_ref_scope;
  wasm::WasmCode* code = native_module->GetCode(func_index);
  return isolate->heap()->ToBoolean(code && code->is_turbofan());
}

RUNTIME_FUNCTION(Runtime_FreezeWasmLazyCompilation) {
  DCHECK_EQ(1, args.length());
  DisallowGarbageCollection no_gc;
  auto instance = WasmInstanceObject::cast(args[0]);

  instance.module_object().native_module()->set_lazy_compile_frozen(true);
  return ReadOnlyRoots(isolate).undefined_value();
}

}  // namespace internal
}  // namespace v8

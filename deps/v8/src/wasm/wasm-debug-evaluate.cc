// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-debug-evaluate.h"

#include <algorithm>

#include "src/api/api-inl.h"
#include "src/codegen/machine-type.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-arguments.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace {

static Handle<String> V8String(Isolate* isolate, const char* str) {
  return isolate->factory()->NewStringFromAsciiChecked(str);
}

static bool CheckSignature(ValueType return_type,
                           std::initializer_list<ValueType> argument_types,
                           const FunctionSig* sig, ErrorThrower* thrower) {
  if (sig->return_count() != 1 && return_type != kWasmBottom) {
    thrower->CompileError("Invalid return type. Got none, expected %s",
                          return_type.type_name());
    return false;
  }

  if (sig->return_count() == 1) {
    if (sig->GetReturn(0) != return_type) {
      thrower->CompileError("Invalid return type. Got %s, expected %s",
                            sig->GetReturn(0).type_name(),
                            return_type.type_name());
      return false;
    }
  }

  if (sig->parameter_count() != argument_types.size()) {
    thrower->CompileError("Invalid number of arguments. Expected %zu, got %zu",
                          sig->parameter_count(), argument_types.size());
    return false;
  }
  size_t p = 0;
  for (ValueType argument_type : argument_types) {
    if (sig->GetParam(p) != argument_type) {
      thrower->CompileError(
          "Invalid argument type for argument %zu. Got %s, expected %s", p,
          sig->GetParam(p).type_name(), argument_type.type_name());
      return false;
    }
    ++p;
  }
  return true;
}

static bool CheckRangeOutOfBounds(uint32_t offset, uint32_t size,
                                  size_t allocation_size,
                                  wasm::ErrorThrower* thrower) {
  if (size > std::numeric_limits<uint32_t>::max() - offset) {
    thrower->RuntimeError("Overflowing memory range\n");
    return true;
  }
  if (offset + size > allocation_size) {
    thrower->RuntimeError("Illegal access to out-of-bounds memory");
    return true;
  }
  return false;
}

class DebugEvaluatorProxy {
 public:
  explicit DebugEvaluatorProxy(Isolate* isolate) : isolate_(isolate) {}

  static void GetMemoryTrampoline(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    DebugEvaluatorProxy& proxy = GetProxy(args);

    uint32_t offset = proxy.GetArgAsUInt32(args, 0);
    uint32_t size = proxy.GetArgAsUInt32(args, 1);
    uint32_t result = proxy.GetArgAsUInt32(args, 2);

    proxy.GetMemory(offset, size, result);
  }

  void GetMemory(uint32_t offset, uint32_t size, uint32_t result) {
    wasm::ScheduledErrorThrower thrower(isolate_, "debug evaluate proxy");
    // Check all overflows.
    if (CheckRangeOutOfBounds(result, size, debuggee_->memory_size(),
                              &thrower) ||
        CheckRangeOutOfBounds(offset, size, evaluator_->memory_size(),
                              &thrower)) {
      return;
    }

    std::memcpy(&evaluator_->memory_start()[result],
                &debuggee_->memory_start()[offset], size);
  }

  template <typename CallableT>
  Handle<JSReceiver> WrapAsV8Function(CallableT callback) {
    v8::Isolate* api_isolate = reinterpret_cast<v8::Isolate*>(isolate_);
    v8::Local<v8::Context> context = api_isolate->GetCurrentContext();
    std::string data;
    v8::Local<v8::Function> func =
        v8::Function::New(context, callback,
                          v8::External::New(api_isolate, this))
            .ToLocalChecked();

    return Utils::OpenHandle(*func);
  }

  Handle<JSObject> CreateImports() {
    Handle<JSObject> imports_obj =
        isolate_->factory()->NewJSObject(isolate_->object_function());
    Handle<JSObject> import_module_obj =
        isolate_->factory()->NewJSObject(isolate_->object_function());
    Object::SetProperty(isolate_, imports_obj,
                        isolate_->factory()->empty_string(), import_module_obj)
        .Assert();

    Object::SetProperty(
        isolate_, import_module_obj, V8String(isolate_, "__getMemory"),
        WrapAsV8Function(DebugEvaluatorProxy::GetMemoryTrampoline))
        .Assert();
    return imports_obj;
  }

  void SetInstances(Handle<WasmInstanceObject> evaluator,
                    Handle<WasmInstanceObject> debuggee) {
    evaluator_ = evaluator;
    debuggee_ = debuggee;
  }

 private:
  uint32_t GetArgAsUInt32(const v8::FunctionCallbackInfo<v8::Value>& args,
                          int index) {
    // No type/range checks needed on his because this is only called for {args}
    // where we have performed a signature check via {VerifyEvaluatorInterface}
    double number = Utils::OpenHandle(*args[index])->Number();
    return static_cast<uint32_t>(number);
  }

  static DebugEvaluatorProxy& GetProxy(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    return *reinterpret_cast<DebugEvaluatorProxy*>(
        args.Data().As<v8::External>()->Value());
  }

  Isolate* isolate_;
  Handle<WasmInstanceObject> evaluator_;
  Handle<WasmInstanceObject> debuggee_;
};

static bool VerifyEvaluatorInterface(const WasmModule* raw_module,
                                     const ModuleWireBytes& bytes,
                                     ErrorThrower* thrower) {
  for (const WasmFunction& F : raw_module->functions) {
    WireBytesRef name_ref = raw_module->function_names.Lookup(
        bytes, F.func_index, VectorOf(raw_module->export_table));
    std::string name(bytes.start() + name_ref.offset(),
                     bytes.start() + name_ref.end_offset());
    if (F.exported && name == "wasm_format") {
      if (!CheckSignature(kWasmI32, {}, F.sig, thrower)) return false;
    } else if (F.imported) {
      if (name == "__getMemory") {
        if (!CheckSignature(kWasmBottom, {kWasmI32, kWasmI32, kWasmI32}, F.sig,
                            thrower)) {
          return false;
        }
      }
    }
  }
  return true;
}

}  // namespace

Maybe<std::string> DebugEvaluateImpl(
    Vector<const byte> snippet, Handle<WasmInstanceObject> debuggee_instance,
    WasmInterpreter::FramePtr frame) {
  Isolate* isolate = debuggee_instance->GetIsolate();
  HandleScope handle_scope(isolate);
  WasmEngine* engine = isolate->wasm_engine();
  wasm::ErrorThrower thrower(isolate, "wasm debug evaluate");

  // Create module object.
  wasm::ModuleWireBytes bytes(snippet);
  wasm::WasmFeatures features = wasm::WasmFeatures::FromIsolate(isolate);
  Handle<WasmModuleObject> evaluator_module;
  if (!engine->SyncCompile(isolate, features, &thrower, bytes)
           .ToHandle(&evaluator_module)) {
    return Nothing<std::string>();
  }

  // Verify interface.
  const WasmModule* raw_module = evaluator_module->module();
  if (!VerifyEvaluatorInterface(raw_module, bytes, &thrower)) {
    return Nothing<std::string>();
  }

  // Set up imports.
  DebugEvaluatorProxy proxy(isolate);
  Handle<JSObject> imports = proxy.CreateImports();

  // Instantiate Module.
  Handle<WasmInstanceObject> evaluator_instance;
  if (!engine->SyncInstantiate(isolate, &thrower, evaluator_module, imports, {})
           .ToHandle(&evaluator_instance)) {
    return Nothing<std::string>();
  }

  proxy.SetInstances(evaluator_instance, debuggee_instance);

  Handle<JSObject> exports_obj(evaluator_instance->exports_object(), isolate);
  Handle<Object> entry_point_obj;
  bool get_property_success =
      Object::GetProperty(isolate, exports_obj,
                          V8String(isolate, "wasm_format"))
          .ToHandle(&entry_point_obj);
  if (!get_property_success ||
      !WasmExportedFunction::IsWasmExportedFunction(*entry_point_obj)) {
    thrower.LinkError("Missing export: \"wasm_format\"");
    return Nothing<std::string>();
  }
  Handle<WasmExportedFunction> entry_point =
      Handle<WasmExportedFunction>::cast(entry_point_obj);

  Handle<WasmDebugInfo> debug_info =
      WasmInstanceObject::GetOrCreateDebugInfo(evaluator_instance);
  Handle<Code> wasm_entry =
      WasmDebugInfo::GetCWasmEntry(debug_info, entry_point->sig());
  CWasmArgumentsPacker packer(4 /* uint32_t return value, no parameters. */);
  Execution::CallWasm(isolate, wasm_entry, entry_point->GetWasmCallTarget(),
                      evaluator_instance, packer.argv());
  if (isolate->has_pending_exception()) return Nothing<std::string>();

  uint32_t offset = packer.Pop<uint32_t>();
  if (CheckRangeOutOfBounds(offset, 0, evaluator_instance->memory_size(),
                            &thrower)) {
    return Nothing<std::string>();
  }

  // Copy the zero-terminated string result but don't overflow.
  std::string result;
  byte* heap = evaluator_instance->memory_start() + offset;
  for (; offset < evaluator_instance->memory_size(); ++offset, ++heap) {
    if (*heap == 0) return Just(result);
    result.push_back(*heap);
  }

  thrower.RuntimeError("The evaluation returned an invalid result");
  return Nothing<std::string>();
}

MaybeHandle<String> DebugEvaluate(Vector<const byte> snippet,
                                  Handle<WasmInstanceObject> debuggee_instance,
                                  WasmInterpreter::FramePtr frame) {
  Maybe<std::string> result =
      DebugEvaluateImpl(snippet, debuggee_instance, std::move(frame));
  if (result.IsNothing()) return {};
  std::string result_str = result.ToChecked();
  return V8String(debuggee_instance->GetIsolate(), result_str.c_str());
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

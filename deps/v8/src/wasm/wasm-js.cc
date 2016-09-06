// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api-natives.h"
#include "src/api.h"
#include "src/asmjs/asm-js.h"
#include "src/asmjs/asm-typer.h"
#include "src/asmjs/asm-wasm-builder.h"
#include "src/assert-scope.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/compiler.h"
#include "src/execution.h"
#include "src/factory.h"
#include "src/handles.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/parsing/parse-info.h"

#include "src/wasm/encoder.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-result.h"

typedef uint8_t byte;

using v8::internal::wasm::ErrorThrower;

namespace v8 {

namespace {
struct RawBuffer {
  const byte* start;
  const byte* end;
  size_t size() { return static_cast<size_t>(end - start); }
};

RawBuffer GetRawBufferSource(
    v8::Local<v8::Value> source, ErrorThrower* thrower) {
  const byte* start = nullptr;
  const byte* end = nullptr;

  if (source->IsArrayBuffer()) {
    // A raw array buffer was passed.
    Local<ArrayBuffer> buffer = Local<ArrayBuffer>::Cast(source);
    ArrayBuffer::Contents contents = buffer->GetContents();

    start = reinterpret_cast<const byte*>(contents.Data());
    end = start + contents.ByteLength();

    if (start == nullptr || end == start) {
      thrower->Error("ArrayBuffer argument is empty");
    }
  } else if (source->IsTypedArray()) {
    // A TypedArray was passed.
    Local<TypedArray> array = Local<TypedArray>::Cast(source);
    Local<ArrayBuffer> buffer = array->Buffer();

    ArrayBuffer::Contents contents = buffer->GetContents();

    start =
        reinterpret_cast<const byte*>(contents.Data()) + array->ByteOffset();
    end = start + array->ByteLength();

    if (start == nullptr || end == start) {
      thrower->Error("ArrayBuffer argument is empty");
    }
  } else {
    thrower->Error("Argument 0 must be an ArrayBuffer or Uint8Array");
  }

  return {start, end};
}

void VerifyModule(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(args.GetIsolate());
  ErrorThrower thrower(isolate, "Wasm.verifyModule()");

  if (args.Length() < 1) {
    thrower.Error("Argument 0 must be a buffer source");
    return;
  }
  RawBuffer buffer = GetRawBufferSource(args[0], &thrower);
  if (thrower.error()) return;

  i::Zone zone(isolate->allocator());
  internal::wasm::ModuleResult result =
      internal::wasm::DecodeWasmModule(isolate, &zone, buffer.start, buffer.end,
                                       true, internal::wasm::kWasmOrigin);

  if (result.failed()) {
    thrower.Failed("", result);
  }

  if (result.val) delete result.val;
}

void VerifyFunction(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(args.GetIsolate());
  ErrorThrower thrower(isolate, "Wasm.verifyFunction()");

  if (args.Length() < 1) {
    thrower.Error("Argument 0 must be a buffer source");
    return;
  }
  RawBuffer buffer = GetRawBufferSource(args[0], &thrower);
  if (thrower.error()) return;

  internal::wasm::FunctionResult result;
  {
    // Verification of a single function shouldn't allocate.
    i::DisallowHeapAllocation no_allocation;
    i::Zone zone(isolate->allocator());
    result = internal::wasm::DecodeWasmFunction(isolate, &zone, nullptr,
                                                buffer.start, buffer.end);
  }

  if (result.failed()) {
    thrower.Failed("", result);
  }

  if (result.val) delete result.val;
}

i::MaybeHandle<i::JSObject> InstantiateModule(
    const v8::FunctionCallbackInfo<v8::Value>& args, const byte* start,
    const byte* end, ErrorThrower* thrower,
    internal::wasm::ModuleOrigin origin = i::wasm::kWasmOrigin) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(args.GetIsolate());

  // Decode but avoid a redundant pass over function bodies for verification.
  // Verification will happen during compilation.
  i::Zone zone(isolate->allocator());
  internal::wasm::ModuleResult result = internal::wasm::DecodeWasmModule(
      isolate, &zone, start, end, false, origin);

  i::MaybeHandle<i::JSObject> object;
  if (result.failed()) {
    thrower->Failed("", result);
  } else {
    // Success. Instantiate the module and return the object.
    i::Handle<i::JSObject> ffi = i::Handle<i::JSObject>::null();
    if (args.Length() > 1 && args[1]->IsObject()) {
      Local<Object> obj = Local<Object>::Cast(args[1]);
      ffi = i::Handle<i::JSObject>::cast(v8::Utils::OpenHandle(*obj));
    }

    i::Handle<i::JSArrayBuffer> memory = i::Handle<i::JSArrayBuffer>::null();
    if (args.Length() > 2 && args[2]->IsArrayBuffer()) {
      Local<Object> obj = Local<Object>::Cast(args[2]);
      i::Handle<i::Object> mem_obj = v8::Utils::OpenHandle(*obj);
      memory = i::Handle<i::JSArrayBuffer>(i::JSArrayBuffer::cast(*mem_obj));
    }

    i::MaybeHandle<i::FixedArray> compiled_module =
        result.val->CompileFunctions(isolate, thrower);
    if (!thrower->error()) {
      DCHECK(!compiled_module.is_null());
      object = i::wasm::WasmModule::Instantiate(
          isolate, compiled_module.ToHandleChecked(), ffi, memory);
      if (!object.is_null()) {
        args.GetReturnValue().Set(v8::Utils::ToLocal(object.ToHandleChecked()));
      }
    }
  }

  if (result.val) delete result.val;
  return object;
}

void InstantiateModule(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(args.GetIsolate());
  ErrorThrower thrower(isolate, "Wasm.instantiateModule()");

  if (args.Length() < 1) {
    thrower.Error("Argument 0 must be a buffer source");
    return;
  }
  RawBuffer buffer = GetRawBufferSource(args[0], &thrower);
  if (buffer.start == nullptr) return;

  InstantiateModule(args, buffer.start, buffer.end, &thrower);
}

static i::MaybeHandle<i::JSObject> CreateModuleObject(
    v8::Isolate* isolate, const v8::Local<v8::Value> source,
    ErrorThrower* thrower) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::MaybeHandle<i::JSObject> nothing;

  RawBuffer buffer = GetRawBufferSource(source, thrower);
  if (buffer.start == nullptr) return i::MaybeHandle<i::JSObject>();

  DCHECK(source->IsArrayBuffer() || source->IsTypedArray());
  i::Zone zone(i_isolate->allocator());
  i::wasm::ModuleResult result = i::wasm::DecodeWasmModule(
      i_isolate, &zone, buffer.start, buffer.end, false, i::wasm::kWasmOrigin);
  std::unique_ptr<const i::wasm::WasmModule> decoded_module(result.val);
  if (result.failed()) {
    thrower->Failed("", result);
    return nothing;
  }
  i::MaybeHandle<i::FixedArray> compiled_module =
      decoded_module->CompileFunctions(i_isolate, thrower);
  if (compiled_module.is_null()) return nothing;

  return i::wasm::CreateCompiledModuleObject(i_isolate,
                                             compiled_module.ToHandleChecked());
}

void WebAssemblyCompile(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  ErrorThrower thrower(reinterpret_cast<i::Isolate*>(isolate),
                       "WebAssembly.compile()");

  if (args.Length() < 1) {
    thrower.Error("Argument 0 must be a buffer source");
    return;
  }
  i::MaybeHandle<i::JSObject> module_obj =
      CreateModuleObject(isolate, args[0], &thrower);

  Local<Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver;
  if (!v8::Promise::Resolver::New(context).ToLocal(&resolver)) return;
  if (thrower.error()) {
    resolver->Reject(context, Utils::ToLocal(thrower.Reify()));
  } else {
    resolver->Resolve(context, Utils::ToLocal(module_obj.ToHandleChecked()));
  }
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(resolver->GetPromise());
}

void WebAssemblyModule(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  ErrorThrower thrower(reinterpret_cast<i::Isolate*>(isolate),
                       "WebAssembly.Module()");

  if (args.Length() < 1) {
    thrower.Error("Argument 0 must be a buffer source");
    return;
  }
  i::MaybeHandle<i::JSObject> module_obj =
      CreateModuleObject(isolate, args[0], &thrower);
  if (module_obj.is_null()) return;

  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(module_obj.ToHandleChecked()));
}

void WebAssemblyInstance(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  ErrorThrower thrower(i_isolate, "WebAssembly.Instance()");

  if (args.Length() < 1) {
    thrower.Error(
        "Argument 0 must be provided, and must be a WebAssembly.Module object");
    return;
  }

  Local<Context> context = isolate->GetCurrentContext();
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);
  i::Handle<i::Symbol> module_sym(i_context->wasm_module_sym());
  i::MaybeHandle<i::Object> source =
      i::Object::GetProperty(Utils::OpenHandle(*args[0]), module_sym);
  if (source.is_null() || source.ToHandleChecked()->IsUndefined(i_isolate)) {
    thrower.Error("Argument 0 must be a WebAssembly.Module");
    return;
  }

  Local<Object> obj = Local<Object>::Cast(args[0]);

  i::Handle<i::JSObject> module_obj =
      i::Handle<i::JSObject>::cast(v8::Utils::OpenHandle(*obj));
  if (module_obj->GetInternalFieldCount() < 1 ||
      !module_obj->GetInternalField(0)->IsFixedArray()) {
    thrower.Error("Argument 0 is an invalid WebAssembly.Module");
    return;
  }

  i::Handle<i::FixedArray> compiled_code = i::Handle<i::FixedArray>(
      i::FixedArray::cast(module_obj->GetInternalField(0)));

  i::Handle<i::JSReceiver> ffi = i::Handle<i::JSObject>::null();
  if (args.Length() > 1 && args[1]->IsObject()) {
    Local<Object> obj = Local<Object>::Cast(args[1]);
    ffi = i::Handle<i::JSReceiver>::cast(v8::Utils::OpenHandle(*obj));
  }

  i::Handle<i::JSArrayBuffer> memory = i::Handle<i::JSArrayBuffer>::null();
  if (args.Length() > 2 && args[2]->IsArrayBuffer()) {
    Local<Object> obj = Local<Object>::Cast(args[2]);
    i::Handle<i::Object> mem_obj = v8::Utils::OpenHandle(*obj);
    memory = i::Handle<i::JSArrayBuffer>(i::JSArrayBuffer::cast(*mem_obj));
  }
  i::MaybeHandle<i::JSObject> instance =
      i::wasm::WasmModule::Instantiate(i_isolate, compiled_code, ffi, memory);
  if (instance.is_null()) {
    thrower.Error("Could not instantiate module");
    return;
  }
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(instance.ToHandleChecked()));
}
}  // namespace

// TODO(titzer): we use the API to create the function template because the
// internal guts are too ugly to replicate here.
static i::Handle<i::FunctionTemplateInfo> NewTemplate(i::Isolate* i_isolate,
                                                      FunctionCallback func) {
  Isolate* isolate = reinterpret_cast<Isolate*>(i_isolate);
  Local<FunctionTemplate> local = FunctionTemplate::New(isolate, func);
  return v8::Utils::OpenHandle(*local);
}

namespace internal {
static Handle<String> v8_str(Isolate* isolate, const char* str) {
  return isolate->factory()->NewStringFromAsciiChecked(str);
}

static Handle<JSFunction> InstallFunc(Isolate* isolate, Handle<JSObject> object,
                                      const char* str, FunctionCallback func) {
  Handle<String> name = v8_str(isolate, str);
  Handle<FunctionTemplateInfo> temp = NewTemplate(isolate, func);
  Handle<JSFunction> function =
      ApiNatives::InstantiateFunction(temp).ToHandleChecked();
  PropertyAttributes attributes =
      static_cast<PropertyAttributes>(DONT_DELETE | READ_ONLY);
  JSObject::AddProperty(object, name, function, attributes);
  return function;
}

void WasmJs::Install(Isolate* isolate, Handle<JSGlobalObject> global) {
  if (!FLAG_expose_wasm && !FLAG_validate_asm) {
    return;
  }

  Factory* factory = isolate->factory();

  // Setup wasm function map.
  Handle<Context> context(global->native_context(), isolate);
  InstallWasmFunctionMap(isolate, context);

  if (!FLAG_expose_wasm) {
    return;
  }

  // Bind the experimental WASM object.
  // TODO(rossberg, titzer): remove once it's no longer needed.
  {
    Handle<String> name = v8_str(isolate, "Wasm");
    Handle<JSFunction> cons = factory->NewFunction(name);
    JSFunction::SetInstancePrototype(
        cons, Handle<Object>(context->initial_object_prototype(), isolate));
    cons->shared()->set_instance_class_name(*name);
    Handle<JSObject> wasm_object = factory->NewJSObject(cons, TENURED);
    PropertyAttributes attributes = static_cast<PropertyAttributes>(DONT_ENUM);
    JSObject::AddProperty(global, name, wasm_object, attributes);

    // Install functions on the WASM object.
    InstallFunc(isolate, wasm_object, "verifyModule", VerifyModule);
    InstallFunc(isolate, wasm_object, "verifyFunction", VerifyFunction);
    InstallFunc(isolate, wasm_object, "instantiateModule", InstantiateModule);

    {
      // Add the Wasm.experimentalVersion property.
      Handle<String> name = v8_str(isolate, "experimentalVersion");
      PropertyAttributes attributes =
          static_cast<PropertyAttributes>(DONT_DELETE | READ_ONLY);
      Handle<Smi> value =
          Handle<Smi>(Smi::FromInt(wasm::kWasmVersion), isolate);
      JSObject::AddProperty(wasm_object, name, value, attributes);
    }
  }

  // Create private symbols.
  Handle<Symbol> module_sym = isolate->factory()->NewPrivateSymbol();
  Handle<Symbol> instance_sym = isolate->factory()->NewPrivateSymbol();
  context->set_wasm_module_sym(*module_sym);
  context->set_wasm_instance_sym(*instance_sym);

  // Bind the WebAssembly object.
  Handle<String> name = v8_str(isolate, "WebAssembly");
  Handle<JSFunction> cons = factory->NewFunction(name);
  JSFunction::SetInstancePrototype(
      cons, Handle<Object>(context->initial_object_prototype(), isolate));
  cons->shared()->set_instance_class_name(*name);
  Handle<JSObject> wasm_object = factory->NewJSObject(cons, TENURED);
  PropertyAttributes attributes = static_cast<PropertyAttributes>(DONT_ENUM);
  JSObject::AddProperty(global, name, wasm_object, attributes);

  // Install static methods on WebAssembly object.
  InstallFunc(isolate, wasm_object, "compile", WebAssemblyCompile);
  Handle<JSFunction> module_constructor =
      InstallFunc(isolate, wasm_object, "Module", WebAssemblyModule);
  Handle<JSFunction> instance_constructor =
      InstallFunc(isolate, wasm_object, "Instance", WebAssemblyInstance);
  i::Handle<i::Map> map = isolate->factory()->NewMap(
      i::JS_OBJECT_TYPE, i::JSObject::kHeaderSize + i::kPointerSize);
  module_constructor->set_prototype_or_initial_map(*map);
  map->SetConstructor(*module_constructor);

  context->set_wasm_module_constructor(*module_constructor);
  context->set_wasm_instance_constructor(*instance_constructor);
}

void WasmJs::InstallWasmFunctionMap(Isolate* isolate, Handle<Context> context) {
  if (!context->get(Context::WASM_FUNCTION_MAP_INDEX)->IsMap()) {
    // TODO(titzer): Move this to bootstrapper.cc??
    // TODO(titzer): Also make one for strict mode functions?
    Handle<Map> prev_map = Handle<Map>(context->sloppy_function_map(), isolate);

    InstanceType instance_type = prev_map->instance_type();
    int internal_fields = JSObject::GetInternalFieldCount(*prev_map);
    CHECK_EQ(0, internal_fields);
    int pre_allocated =
        prev_map->GetInObjectProperties() - prev_map->unused_property_fields();
    int instance_size = 0;
    int in_object_properties = 0;
    int wasm_internal_fields = internal_fields + 1  // module instance object
                               + 1                  // function arity
                               + 1;                 // function signature
    JSFunction::CalculateInstanceSizeHelper(instance_type, wasm_internal_fields,
                                            0, &instance_size,
                                            &in_object_properties);

    int unused_property_fields = in_object_properties - pre_allocated;
    Handle<Map> map = Map::CopyInitialMap(
        prev_map, instance_size, in_object_properties, unused_property_fields);

    context->set_wasm_function_map(*map);
  }
}

}  // namespace internal
}  // namespace v8

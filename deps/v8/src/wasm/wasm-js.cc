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
#include "src/execution.h"
#include "src/factory.h"
#include "src/handles.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/parsing/parse-info.h"

#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-result.h"

typedef uint8_t byte;

using v8::internal::wasm::ErrorThrower;

namespace v8 {

namespace {
i::Handle<i::String> v8_str(i::Isolate* isolate, const char* str) {
  return isolate->factory()->NewStringFromAsciiChecked(str);
}
Local<String> v8_str(Isolate* isolate, const char* str) {
  return Utils::ToLocal(v8_str(reinterpret_cast<i::Isolate*>(isolate), str));
}

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
    thrower.TypeError("Argument 0 must be a buffer source");
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
    thrower.TypeError("Argument 0 must be a buffer source");
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
  i::MaybeHandle<i::JSObject> module_object =
      i::wasm::CreateModuleObjectFromBytes(isolate, start, end, thrower,
                                           origin);
  i::MaybeHandle<i::JSObject> object;
  if (!module_object.is_null()) {
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

    object = i::wasm::WasmModule::Instantiate(
        isolate, thrower, module_object.ToHandleChecked(), ffi, memory);
    if (!object.is_null()) {
      args.GetReturnValue().Set(v8::Utils::ToLocal(object.ToHandleChecked()));
    }
  }
  return object;
}

void InstantiateModule(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(args.GetIsolate());
  ErrorThrower thrower(isolate, "Wasm.instantiateModule()");

  if (args.Length() < 1) {
    thrower.TypeError("Argument 0 must be a buffer source");
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
  return i::wasm::CreateModuleObjectFromBytes(
      i_isolate, buffer.start, buffer.end, thrower,
      i::wasm::ModuleOrigin::kWasmOrigin);
}

static bool ValidateModule(v8::Isolate* isolate,
                           const v8::Local<v8::Value> source,
                           ErrorThrower* thrower) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::MaybeHandle<i::JSObject> nothing;

  RawBuffer buffer = GetRawBufferSource(source, thrower);
  if (buffer.start == nullptr) return false;

  DCHECK(source->IsArrayBuffer() || source->IsTypedArray());
  return i::wasm::ValidateModuleBytes(i_isolate, buffer.start, buffer.end,
                                      thrower,
                                      i::wasm::ModuleOrigin::kWasmOrigin);
}

bool BrandCheck(Isolate* isolate, i::Handle<i::Object> value,
                i::Handle<i::Symbol> sym, const char* msg) {
  if (value->IsJSObject()) {
    i::Handle<i::JSObject> object = i::Handle<i::JSObject>::cast(value);
    Maybe<bool> has_brand = i::JSObject::HasOwnProperty(object, sym);
    if (has_brand.IsNothing()) return false;
    if (has_brand.ToChecked()) return true;
  }
  v8::Local<v8::Value> e = v8::Exception::TypeError(v8_str(isolate, msg));
  isolate->ThrowException(e);
  return false;
}

void WebAssemblyCompile(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  ErrorThrower thrower(reinterpret_cast<i::Isolate*>(isolate),
                       "WebAssembly.compile()");

  if (args.Length() < 1) {
    thrower.TypeError("Argument 0 must be a buffer source");
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

void WebAssemblyValidate(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  ErrorThrower thrower(reinterpret_cast<i::Isolate*>(isolate),
                       "WebAssembly.validate()");

  if (args.Length() < 1) {
    thrower.TypeError("Argument 0 must be a buffer source");
    return;
  }

  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  if (ValidateModule(isolate, args[0], &thrower)) {
    return_value.Set(v8::True(isolate));
  } else {
    return_value.Set(v8::False(isolate));
  }
}

void WebAssemblyModule(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  ErrorThrower thrower(reinterpret_cast<i::Isolate*>(isolate),
                       "WebAssembly.Module()");

  if (args.Length() < 1) {
    thrower.TypeError("Argument 0 must be a buffer source");
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
    thrower.TypeError("Argument 0 must be a WebAssembly.Module");
    return;
  }

  Local<Context> context = isolate->GetCurrentContext();
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);
  if (!BrandCheck(isolate, Utils::OpenHandle(*args[0]),
                  i::Handle<i::Symbol>(i_context->wasm_module_sym()),
                  "Argument 0 must be a WebAssembly.Module")) {
    return;
  }

  Local<Object> obj = Local<Object>::Cast(args[0]);

  i::Handle<i::JSObject> module_obj =
      i::Handle<i::JSObject>::cast(v8::Utils::OpenHandle(*obj));
  if (module_obj->GetInternalFieldCount() < 1 ||
      !module_obj->GetInternalField(0)->IsFixedArray()) {
    thrower.TypeError("Argument 0 is an invalid WebAssembly.Module");
    return;
  }

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
  i::MaybeHandle<i::JSObject> instance = i::wasm::WasmModule::Instantiate(
      i_isolate, &thrower, module_obj, ffi, memory);
  if (instance.is_null()) {
    if (!thrower.error()) thrower.Error("Could not instantiate module");
    return;
  }
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(instance.ToHandleChecked()));
}

bool GetIntegerProperty(v8::Isolate* isolate, ErrorThrower* thrower,
                        Local<Context> context, Local<v8::Object> object,
                        Local<String> property, int* result, int lower_bound,
                        int upper_bound) {
  v8::MaybeLocal<v8::Value> maybe = object->Get(context, property);
  v8::Local<v8::Value> value;
  if (maybe.ToLocal(&value)) {
    int64_t number;
    if (!value->IntegerValue(context).To(&number)) return false;
    if (number < static_cast<int64_t>(lower_bound)) {
      thrower->RangeError("Property value %" PRId64
                          " is below the lower bound %d",
                          number, lower_bound);
      return false;
    }
    if (number > static_cast<int64_t>(upper_bound)) {
      thrower->RangeError("Property value %" PRId64
                          " is above the upper bound %d",
                          number, upper_bound);
      return false;
    }
    *result = static_cast<int>(number);
    return true;
  }
  return false;
}

void WebAssemblyTable(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  ErrorThrower thrower(reinterpret_cast<i::Isolate*>(isolate),
                       "WebAssembly.Module()");
  if (args.Length() < 1 || !args[0]->IsObject()) {
    thrower.TypeError("Argument 0 must be a table descriptor");
    return;
  }
  Local<Context> context = isolate->GetCurrentContext();
  Local<v8::Object> descriptor = args[0]->ToObject(context).ToLocalChecked();
  // The descriptor's 'element'.
  {
    v8::MaybeLocal<v8::Value> maybe =
        descriptor->Get(context, v8_str(isolate, "element"));
    v8::Local<v8::Value> value;
    if (!maybe.ToLocal(&value)) return;
    v8::Local<v8::String> string;
    if (!value->ToString(context).ToLocal(&string)) return;
    bool equal;
    if (!string->Equals(context, v8_str(isolate, "anyfunc")).To(&equal)) return;
    if (!equal) {
      thrower.TypeError("Descriptor property 'element' must be 'anyfunc'");
      return;
    }
  }
  const int max_table_size = 1 << 26;
  // The descriptor's 'initial'.
  int initial;
  if (!GetIntegerProperty(isolate, &thrower, context, descriptor,
                          v8_str(isolate, "initial"), &initial, 0,
                          max_table_size)) {
    return;
  }
  // The descriptor's 'maximum'.
  int maximum = 0;
  Local<String> maximum_key = v8_str(isolate, "maximum");
  Maybe<bool> has_maximum = descriptor->Has(context, maximum_key);

  if (has_maximum.IsNothing()) {
    // There has been an exception, just return.
    return;
  }
  if (has_maximum.FromJust()) {
    if (!GetIntegerProperty(isolate, &thrower, context, descriptor, maximum_key,
                            &maximum, initial, max_table_size)) {
      return;
    }
  }

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::Handle<i::JSFunction> table_ctor(
      i_isolate->native_context()->wasm_table_constructor());
  i::Handle<i::JSObject> table_obj =
      i_isolate->factory()->NewJSObject(table_ctor);
  i::Handle<i::FixedArray> fixed_array =
      i_isolate->factory()->NewFixedArray(initial);
  i::Object* null = i_isolate->heap()->null_value();
  for (int i = 0; i < initial; ++i) fixed_array->set(i, null);
  table_obj->SetInternalField(0, *fixed_array);
  table_obj->SetInternalField(
      1, has_maximum.FromJust()
             ? static_cast<i::Object*>(i::Smi::FromInt(maximum))
             : static_cast<i::Object*>(i_isolate->heap()->undefined_value()));
  i::Handle<i::Symbol> table_sym(i_isolate->native_context()->wasm_table_sym());
  i::Object::SetProperty(table_obj, table_sym, table_obj, i::STRICT).Check();
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(table_obj));
}

void WebAssemblyMemory(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  ErrorThrower thrower(reinterpret_cast<i::Isolate*>(isolate),
                       "WebAssembly.Module()");
  if (args.Length() < 1 || !args[0]->IsObject()) {
    thrower.TypeError("Argument 0 must be a table descriptor");
    return;
  }
  Local<Context> context = isolate->GetCurrentContext();
  Local<v8::Object> descriptor = args[0]->ToObject(context).ToLocalChecked();
  // The descriptor's 'initial'.
  int initial;
  if (!GetIntegerProperty(isolate, &thrower, context, descriptor,
                          v8_str(isolate, "initial"), &initial, 0, 65536)) {
    return;
  }
  // The descriptor's 'maximum'.
  int maximum = 0;
  Local<String> maximum_key = v8_str(isolate, "maximum");
  Maybe<bool> has_maximum = descriptor->Has(context, maximum_key);

  if (has_maximum.IsNothing()) {
    // There has been an exception, just return.
    return;
  }
  if (has_maximum.FromJust()) {
    if (!GetIntegerProperty(isolate, &thrower, context, descriptor, maximum_key,
                            &maximum, initial, 65536)) {
      return;
    }
  }
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::Handle<i::JSArrayBuffer> buffer =
      i_isolate->factory()->NewJSArrayBuffer(i::SharedFlag::kNotShared);
  size_t size = static_cast<size_t>(i::wasm::WasmModule::kPageSize) *
                static_cast<size_t>(initial);
  i::JSArrayBuffer::SetupAllocatingData(buffer, i_isolate, size);

  i::Handle<i::JSObject> memory_obj = i::WasmJs::CreateWasmMemoryObject(
      i_isolate, buffer, has_maximum.FromJust(), maximum);
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(memory_obj));
}
void WebAssemblyTableGetLength(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // TODO(rossberg)
}
void WebAssemblyTableGrow(const v8::FunctionCallbackInfo<v8::Value>& args) {
  // TODO(rossberg)
}
void WebAssemblyTableGet(const v8::FunctionCallbackInfo<v8::Value>& args) {
  // TODO(rossberg)
}
void WebAssemblyTableSet(const v8::FunctionCallbackInfo<v8::Value>& args) {
  // TODO(rossberg)
}
void WebAssemblyMemoryGrow(const v8::FunctionCallbackInfo<v8::Value>& args) {
  // TODO(rossberg)
}
void WebAssemblyMemoryGetBuffer(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);
  if (!BrandCheck(isolate, Utils::OpenHandle(*args.This()),
                  i::Handle<i::Symbol>(i_context->wasm_memory_sym()),
                  "Receiver is not a WebAssembly.Memory")) {
    return;
  }
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::Handle<i::JSObject> receiver =
      i::Handle<i::JSObject>::cast(Utils::OpenHandle(*args.This()));
  i::Handle<i::Object> buffer(receiver->GetInternalField(0), i_isolate);
  DCHECK(buffer->IsJSArrayBuffer());
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(buffer));
}
}  // namespace

i::Handle<i::JSObject> i::WasmJs::CreateWasmMemoryObject(
    i::Isolate* i_isolate, i::Handle<i::JSArrayBuffer> buffer, bool has_maximum,
    int maximum) {
  i::Handle<i::JSFunction> memory_ctor(
      i_isolate->native_context()->wasm_memory_constructor());
  i::Handle<i::JSObject> memory_obj =
      i_isolate->factory()->NewJSObject(memory_ctor);
  memory_obj->SetInternalField(0, *buffer);
  memory_obj->SetInternalField(
      1, has_maximum
             ? static_cast<i::Object*>(i::Smi::FromInt(maximum))
             : static_cast<i::Object*>(i_isolate->heap()->undefined_value()));
  i::Handle<i::Symbol> memory_sym(
      i_isolate->native_context()->wasm_memory_sym());
  i::Object::SetProperty(memory_obj, memory_sym, memory_obj, i::STRICT).Check();
  return memory_obj;
}

// TODO(titzer): we use the API to create the function template because the
// internal guts are too ugly to replicate here.
static i::Handle<i::FunctionTemplateInfo> NewTemplate(i::Isolate* i_isolate,
                                                      FunctionCallback func) {
  Isolate* isolate = reinterpret_cast<Isolate*>(i_isolate);
  Local<FunctionTemplate> local = FunctionTemplate::New(isolate, func);
  return v8::Utils::OpenHandle(*local);
}

namespace internal {

Handle<JSFunction> InstallFunc(Isolate* isolate, Handle<JSObject> object,
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

Handle<JSFunction> InstallGetter(Isolate* isolate, Handle<JSObject> object,
                                 const char* str, FunctionCallback func) {
  Handle<String> name = v8_str(isolate, str);
  Handle<FunctionTemplateInfo> temp = NewTemplate(isolate, func);
  Handle<JSFunction> function =
      ApiNatives::InstantiateFunction(temp).ToHandleChecked();
  v8::PropertyAttribute attributes =
      static_cast<v8::PropertyAttribute>(v8::DontDelete | v8::ReadOnly);
  Utils::ToLocal(object)->SetAccessorProperty(Utils::ToLocal(name),
                                              Utils::ToLocal(function),
                                              Local<Function>(), attributes);
  return function;
}

void WasmJs::InstallWasmModuleSymbolIfNeeded(Isolate* isolate,
                                             Handle<JSGlobalObject> global,
                                             Handle<Context> context) {
  if (!context->get(Context::WASM_MODULE_SYM_INDEX)->IsSymbol() ||
      !context->get(Context::WASM_INSTANCE_SYM_INDEX)->IsSymbol()) {
    InstallWasmMapsIfNeeded(isolate, isolate->native_context());
    InstallWasmConstructors(isolate, isolate->global_object(),
                            isolate->native_context());
  }
}

void WasmJs::InstallWasmConstructors(Isolate* isolate,
                                     Handle<JSGlobalObject> global,
                                     Handle<Context> context) {
  Factory* factory = isolate->factory();
  // Create private symbols.
  Handle<Symbol> module_sym = factory->NewPrivateSymbol();
  context->set_wasm_module_sym(*module_sym);

  Handle<Symbol> instance_sym = factory->NewPrivateSymbol();
  context->set_wasm_instance_sym(*instance_sym);

  Handle<Symbol> table_sym = factory->NewPrivateSymbol();
  context->set_wasm_table_sym(*table_sym);

  Handle<Symbol> memory_sym = factory->NewPrivateSymbol();
  context->set_wasm_memory_sym(*memory_sym);

  // Bind the WebAssembly object.
  Handle<String> name = v8_str(isolate, "WebAssembly");
  Handle<JSFunction> cons = factory->NewFunction(name);
  JSFunction::SetInstancePrototype(
      cons, Handle<Object>(context->initial_object_prototype(), isolate));
  cons->shared()->set_instance_class_name(*name);
  Handle<JSObject> wasm_object = factory->NewJSObject(cons, TENURED);
  PropertyAttributes attributes = static_cast<PropertyAttributes>(DONT_ENUM);
  JSObject::AddProperty(global, name, wasm_object, attributes);

  // Setup compile
  InstallFunc(isolate, wasm_object, "compile", WebAssemblyCompile);

  // Setup compile
  InstallFunc(isolate, wasm_object, "validate", WebAssemblyValidate);

  // Setup Module
  Handle<JSFunction> module_constructor =
      InstallFunc(isolate, wasm_object, "Module", WebAssemblyModule);
  context->set_wasm_module_constructor(*module_constructor);
  Handle<JSObject> module_proto =
      factory->NewJSObject(module_constructor, TENURED);
  i::Handle<i::Map> map = isolate->factory()->NewMap(
      i::JS_OBJECT_TYPE, i::JSObject::kHeaderSize + i::kPointerSize);
  JSFunction::SetInitialMap(module_constructor, map, module_proto);
  JSObject::AddProperty(module_proto, isolate->factory()->constructor_string(),
                        module_constructor, DONT_ENUM);

  // Setup Instance
  Handle<JSFunction> instance_constructor =
      InstallFunc(isolate, wasm_object, "Instance", WebAssemblyInstance);
  context->set_wasm_instance_constructor(*instance_constructor);

  // Setup Table
  Handle<JSFunction> table_constructor =
      InstallFunc(isolate, wasm_object, "Table", WebAssemblyTable);
  context->set_wasm_table_constructor(*table_constructor);
  Handle<JSObject> table_proto =
      factory->NewJSObject(table_constructor, TENURED);
  map = isolate->factory()->NewMap(
      i::JS_OBJECT_TYPE, i::JSObject::kHeaderSize + 2 * i::kPointerSize);
  JSFunction::SetInitialMap(table_constructor, map, table_proto);
  JSObject::AddProperty(table_proto, isolate->factory()->constructor_string(),
                        table_constructor, DONT_ENUM);
  InstallGetter(isolate, table_proto, "length", WebAssemblyTableGetLength);
  InstallFunc(isolate, table_proto, "grow", WebAssemblyTableGrow);
  InstallFunc(isolate, table_proto, "get", WebAssemblyTableGet);
  InstallFunc(isolate, table_proto, "set", WebAssemblyTableSet);

  // Setup Memory
  Handle<JSFunction> memory_constructor =
      InstallFunc(isolate, wasm_object, "Memory", WebAssemblyMemory);
  context->set_wasm_memory_constructor(*memory_constructor);
  Handle<JSObject> memory_proto =
      factory->NewJSObject(memory_constructor, TENURED);
  map = isolate->factory()->NewMap(
      i::JS_OBJECT_TYPE, i::JSObject::kHeaderSize + 2 * i::kPointerSize);
  JSFunction::SetInitialMap(memory_constructor, map, memory_proto);
  JSObject::AddProperty(memory_proto, isolate->factory()->constructor_string(),
                        memory_constructor, DONT_ENUM);
  InstallFunc(isolate, memory_proto, "grow", WebAssemblyMemoryGrow);
  InstallGetter(isolate, memory_proto, "buffer", WebAssemblyMemoryGetBuffer);
}

void WasmJs::Install(Isolate* isolate, Handle<JSGlobalObject> global) {
  if (!FLAG_expose_wasm && !FLAG_validate_asm) {
    return;
  }

  Factory* factory = isolate->factory();

  // Setup wasm function map.
  Handle<Context> context(global->native_context(), isolate);
  InstallWasmMapsIfNeeded(isolate, context);

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
  InstallWasmConstructors(isolate, global, context);
}

void WasmJs::InstallWasmMapsIfNeeded(Isolate* isolate,
                                     Handle<Context> context) {
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

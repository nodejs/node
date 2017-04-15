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
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-result.h"

typedef uint8_t byte;

using v8::internal::wasm::ErrorThrower;

namespace v8 {

namespace {

#define RANGE_ERROR_MSG                                                        \
  "Wasm compilation exceeds internal limits in this context for the provided " \
  "arguments"

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

bool IsCompilationAllowed(i::Isolate* isolate, ErrorThrower* thrower,
                          v8::Local<v8::Value> source, bool is_async) {
  // Allow caller to do one final check on thrower state, rather than
  // one at each step. No information is lost - failure reason is captured
  // in the thrower state.
  if (thrower->error()) return false;

  AllowWasmCompileCallback callback = isolate->allow_wasm_compile_callback();
  if (callback != nullptr &&
      !callback(reinterpret_cast<v8::Isolate*>(isolate), source, is_async)) {
    thrower->RangeError(RANGE_ERROR_MSG);
    return false;
  }
  return true;
}

bool IsInstantiationAllowed(i::Isolate* isolate, ErrorThrower* thrower,
                            v8::Local<v8::Value> module_or_bytes,
                            i::MaybeHandle<i::JSReceiver> ffi, bool is_async) {
  // Allow caller to do one final check on thrower state, rather than
  // one at each step. No information is lost - failure reason is captured
  // in the thrower state.
  if (thrower->error()) return false;
  v8::MaybeLocal<v8::Value> v8_ffi;
  if (!ffi.is_null()) {
    v8_ffi = v8::Local<v8::Value>::Cast(Utils::ToLocal(ffi.ToHandleChecked()));
  }
  AllowWasmInstantiateCallback callback =
      isolate->allow_wasm_instantiate_callback();
  if (callback != nullptr &&
      !callback(reinterpret_cast<v8::Isolate*>(isolate), module_or_bytes,
                v8_ffi, is_async)) {
    thrower->RangeError(RANGE_ERROR_MSG);
    return false;
  }
  return true;
}

i::wasm::ModuleWireBytes GetFirstArgumentAsBytes(
    const v8::FunctionCallbackInfo<v8::Value>& args, ErrorThrower* thrower) {
  if (args.Length() < 1) {
    thrower->TypeError("Argument 0 must be a buffer source");
    return i::wasm::ModuleWireBytes(nullptr, nullptr);
  }

  const byte* start = nullptr;
  size_t length = 0;
  v8::Local<v8::Value> source = args[0];
  if (source->IsArrayBuffer()) {
    // A raw array buffer was passed.
    Local<ArrayBuffer> buffer = Local<ArrayBuffer>::Cast(source);
    ArrayBuffer::Contents contents = buffer->GetContents();

    start = reinterpret_cast<const byte*>(contents.Data());
    length = contents.ByteLength();
  } else if (source->IsTypedArray()) {
    // A TypedArray was passed.
    Local<TypedArray> array = Local<TypedArray>::Cast(source);
    Local<ArrayBuffer> buffer = array->Buffer();

    ArrayBuffer::Contents contents = buffer->GetContents();

    start =
        reinterpret_cast<const byte*>(contents.Data()) + array->ByteOffset();
    length = array->ByteLength();
  } else {
    thrower->TypeError("Argument 0 must be a buffer source");
  }
  DCHECK_IMPLIES(length, start != nullptr);
  if (length == 0) {
    thrower->CompileError("BufferSource argument is empty");
  }
  if (length > i::wasm::kV8MaxWasmModuleSize) {
    thrower->RangeError("buffer source exceeds maximum size of %zu (is %zu)",
                        i::wasm::kV8MaxWasmModuleSize, length);
  }
  if (thrower->error()) return i::wasm::ModuleWireBytes(nullptr, nullptr);
  // TODO(titzer): use the handle as well?
  return i::wasm::ModuleWireBytes(start, start + length);
}

i::MaybeHandle<i::JSReceiver> GetSecondArgumentAsImports(
    const v8::FunctionCallbackInfo<v8::Value>& args, ErrorThrower* thrower) {
  if (args.Length() < 2) return {};
  if (args[1]->IsUndefined()) return {};

  if (!args[1]->IsObject()) {
    thrower->TypeError("Argument 1 must be an object");
    return {};
  }
  Local<Object> obj = Local<Object>::Cast(args[1]);
  return i::Handle<i::JSReceiver>::cast(v8::Utils::OpenHandle(*obj));
}

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

  } else if (source->IsTypedArray()) {
    // A TypedArray was passed.
    Local<TypedArray> array = Local<TypedArray>::Cast(source);
    Local<ArrayBuffer> buffer = array->Buffer();

    ArrayBuffer::Contents contents = buffer->GetContents();

    start =
        reinterpret_cast<const byte*>(contents.Data()) + array->ByteOffset();
    end = start + array->ByteLength();

  } else {
    thrower->TypeError("Argument 0 must be a buffer source");
  }
  if (start == nullptr || end == start) {
    thrower->CompileError("BufferSource argument is empty");
  }
  return {start, end};
}

static i::MaybeHandle<i::WasmModuleObject> CreateModuleObject(
    v8::Isolate* isolate, const v8::Local<v8::Value> source,
    ErrorThrower* thrower) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::MaybeHandle<i::JSObject> nothing;

  RawBuffer buffer = GetRawBufferSource(source, thrower);
  if (buffer.start == nullptr) return i::MaybeHandle<i::WasmModuleObject>();

  DCHECK(source->IsArrayBuffer() || source->IsTypedArray());
  return i::wasm::CreateModuleObjectFromBytes(
      i_isolate, buffer.start, buffer.end, thrower, i::wasm::kWasmOrigin,
      i::Handle<i::Script>::null(), i::Vector<const byte>::empty());
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

// TODO(wasm): move brand check to the respective types, and don't throw
// in it, rather, use a provided ErrorThrower, or let caller handle it.
static bool BrandCheck(Isolate* isolate, i::Handle<i::Object> value,
                       i::Handle<i::Symbol> sym) {
  if (!value->IsJSObject()) return false;
  i::Handle<i::JSObject> object = i::Handle<i::JSObject>::cast(value);
  Maybe<bool> has_brand = i::JSObject::HasOwnProperty(object, sym);
  if (has_brand.IsNothing()) return false;
  return has_brand.ToChecked();
}

static bool BrandCheck(Isolate* isolate, i::Handle<i::Object> value,
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
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ErrorThrower thrower(reinterpret_cast<i::Isolate*>(isolate),
                       "WebAssembly.compile()");

  Local<Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Promise::Resolver> resolver;
  if (!v8::Promise::Resolver::New(context).ToLocal(&resolver)) return;
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(resolver->GetPromise());

  if (args.Length() < 1) {
    thrower.TypeError("Argument 0 must be a buffer source");
    resolver->Reject(context, Utils::ToLocal(thrower.Reify()));
    return;
  }
  auto bytes = GetFirstArgumentAsBytes(args, &thrower);
  USE(bytes);
  if (!IsCompilationAllowed(i_isolate, &thrower, args[0], true)) {
    resolver->Reject(context, Utils::ToLocal(thrower.Reify()));
    return;
  }
  i::MaybeHandle<i::JSObject> module_obj =
      CreateModuleObject(isolate, args[0], &thrower);

  if (thrower.error()) {
    resolver->Reject(context, Utils::ToLocal(thrower.Reify()));
  } else {
    resolver->Resolve(context, Utils::ToLocal(module_obj.ToHandleChecked()));
  }
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
    if (thrower.wasm_error()) thrower.Reify();  // Clear error.
    return_value.Set(v8::False(isolate));
  }
}

void WebAssemblyModule(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ErrorThrower thrower(reinterpret_cast<i::Isolate*>(isolate),
                       "WebAssembly.Module()");

  if (args.Length() < 1) {
    thrower.TypeError("Argument 0 must be a buffer source");
    return;
  }
  auto bytes = GetFirstArgumentAsBytes(args, &thrower);
  USE(bytes);
  if (!IsCompilationAllowed(i_isolate, &thrower, args[0], false)) return;

  i::MaybeHandle<i::JSObject> module_obj =
      CreateModuleObject(isolate, args[0], &thrower);
  if (module_obj.is_null()) return;

  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(module_obj.ToHandleChecked()));
}

MaybeLocal<Value> InstantiateModuleImpl(
    i::Isolate* i_isolate, i::Handle<i::WasmModuleObject> i_module_obj,
    const v8::FunctionCallbackInfo<v8::Value>& args, ErrorThrower* thrower) {
  // It so happens that in both the WebAssembly.instantiate, as well as
  // WebAssembly.Instance ctor, the positions of the ffi object and memory
  // are the same. If that changes later, we refactor the consts into
  // parameters.
  static const int kFfiOffset = 1;

  MaybeLocal<Value> nothing;
  i::Handle<i::JSReceiver> ffi = i::Handle<i::JSObject>::null();
  // This is a first - level validation of the argument. If present, we only
  // check its type. {Instantiate} will further check that if the module
  // has imports, the argument must be present, as well as piecemeal
  // import satisfaction.
  if (args.Length() > kFfiOffset && !args[kFfiOffset]->IsUndefined()) {
    if (!args[kFfiOffset]->IsObject()) {
      thrower->TypeError("Argument %d must be an object", kFfiOffset);
      return nothing;
    }
    Local<Object> obj = Local<Object>::Cast(args[kFfiOffset]);
    ffi = i::Handle<i::JSReceiver>::cast(v8::Utils::OpenHandle(*obj));
  }

  i::MaybeHandle<i::JSObject> instance =
      i::wasm::WasmModule::Instantiate(i_isolate, thrower, i_module_obj, ffi);
  if (instance.is_null()) {
    if (!thrower->error())
      thrower->RuntimeError("Could not instantiate module");
    return nothing;
  }
  DCHECK(!i_isolate->has_pending_exception());
  return Utils::ToLocal(instance.ToHandleChecked());
}

namespace {
i::MaybeHandle<i::WasmModuleObject> GetFirstArgumentAsModule(
    const v8::FunctionCallbackInfo<v8::Value>& args, ErrorThrower& thrower) {
  v8::Isolate* isolate = args.GetIsolate();
  i::MaybeHandle<i::WasmModuleObject> nothing;
  if (args.Length() < 1) {
    thrower.TypeError("Argument 0 must be a WebAssembly.Module");
    return nothing;
  }

  Local<Context> context = isolate->GetCurrentContext();
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);
  if (!BrandCheck(isolate, Utils::OpenHandle(*args[0]),
                  i::Handle<i::Symbol>(i_context->wasm_module_sym()),
                  "Argument 0 must be a WebAssembly.Module")) {
    return nothing;
  }

  Local<Object> module_obj = Local<Object>::Cast(args[0]);
  return i::Handle<i::WasmModuleObject>::cast(
      v8::Utils::OpenHandle(*module_obj));
}
}  // namespace

void WebAssemblyModuleImports(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ErrorThrower thrower(i_isolate, "WebAssembly.Module.imports()");

  auto maybe_module = GetFirstArgumentAsModule(args, thrower);

  if (!maybe_module.is_null()) {
    auto imports =
        i::wasm::GetImports(i_isolate, maybe_module.ToHandleChecked());
    args.GetReturnValue().Set(Utils::ToLocal(imports));
  }
}

void WebAssemblyModuleExports(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  ErrorThrower thrower(i_isolate, "WebAssembly.Module.exports()");

  auto maybe_module = GetFirstArgumentAsModule(args, thrower);

  if (!maybe_module.is_null()) {
    auto exports =
        i::wasm::GetExports(i_isolate, maybe_module.ToHandleChecked());
    args.GetReturnValue().Set(Utils::ToLocal(exports));
  }
}

void WebAssemblyModuleCustomSections(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  ErrorThrower thrower(i_isolate, "WebAssembly.Module.customSections()");

  auto maybe_module = GetFirstArgumentAsModule(args, thrower);

  if (args.Length() < 2) {
    thrower.TypeError("Argument 1 must be a string");
    return;
  }

  i::Handle<i::Object> name = Utils::OpenHandle(*args[1]);
  if (!name->IsString()) {
    thrower.TypeError("Argument 1 must be a string");
    return;
  }

  if (!maybe_module.is_null()) {
    auto custom_sections =
        i::wasm::GetCustomSections(i_isolate, maybe_module.ToHandleChecked(),
                                   i::Handle<i::String>::cast(name), &thrower);
    if (!thrower.error()) {
      args.GetReturnValue().Set(Utils::ToLocal(custom_sections));
    }
  }
}

void WebAssemblyInstance(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  ErrorThrower thrower(i_isolate, "WebAssembly.Instance()");

  auto maybe_module = GetFirstArgumentAsModule(args, thrower);
  if (thrower.error()) return;
  auto maybe_imports = GetSecondArgumentAsImports(args, &thrower);
  if (!IsInstantiationAllowed(i_isolate, &thrower, args[0], maybe_imports,
                              false)) {
    return;
  }
  DCHECK(!thrower.error());

  if (!maybe_module.is_null()) {
    MaybeLocal<Value> instance = InstantiateModuleImpl(
        i_isolate, maybe_module.ToHandleChecked(), args, &thrower);
    if (instance.IsEmpty()) {
      DCHECK(thrower.error());
      return;
    }
    args.GetReturnValue().Set(instance.ToLocalChecked());
  }
}

void WebAssemblyInstantiate(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  HandleScope scope(isolate);
  ErrorThrower thrower(i_isolate, "WebAssembly.instantiate()");

  Local<Context> context = isolate->GetCurrentContext();
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);

  v8::Local<v8::Promise::Resolver> resolver;
  if (!v8::Promise::Resolver::New(context).ToLocal(&resolver)) return;
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(resolver->GetPromise());

  if (args.Length() < 1) {
    thrower.TypeError(
        "Argument 0 must be provided and must be either a buffer source or a "
        "WebAssembly.Module object");
    resolver->Reject(context, Utils::ToLocal(thrower.Reify()));
    return;
  }

  i::Handle<i::Object> first_arg = Utils::OpenHandle(*args[0]);
  if (!first_arg->IsJSObject()) {
    thrower.TypeError(
        "Argument 0 must be a buffer source or a WebAssembly.Module object");
    resolver->Reject(context, Utils::ToLocal(thrower.Reify()));
    return;
  }

  bool want_pair = !BrandCheck(
      isolate, first_arg, i::Handle<i::Symbol>(i_context->wasm_module_sym()));
  auto maybe_imports = GetSecondArgumentAsImports(args, &thrower);
  if (thrower.error()) {
    resolver->Reject(context, Utils::ToLocal(thrower.Reify()));
    return;
  }
  if (!IsInstantiationAllowed(i_isolate, &thrower, args[0], maybe_imports,
                              true)) {
    resolver->Reject(context, Utils::ToLocal(thrower.Reify()));
    return;
  }
  i::Handle<i::WasmModuleObject> module_obj;
  if (want_pair) {
    i::MaybeHandle<i::WasmModuleObject> maybe_module_obj =
        CreateModuleObject(isolate, args[0], &thrower);
    if (!maybe_module_obj.ToHandle(&module_obj)) {
      DCHECK(thrower.error());
      resolver->Reject(context, Utils::ToLocal(thrower.Reify()));
      return;
    }
  } else {
    module_obj = i::Handle<i::WasmModuleObject>::cast(first_arg);
  }
  DCHECK(!module_obj.is_null());
  MaybeLocal<Value> instance =
      InstantiateModuleImpl(i_isolate, module_obj, args, &thrower);
  if (instance.IsEmpty()) {
    DCHECK(thrower.error());
    resolver->Reject(context, Utils::ToLocal(thrower.Reify()));
  } else {
    DCHECK(!thrower.error());
    Local<Value> retval;
    if (want_pair) {
      i::Handle<i::JSFunction> object_function = i::Handle<i::JSFunction>(
          i_isolate->native_context()->object_function(), i_isolate);

      i::Handle<i::JSObject> i_retval =
          i_isolate->factory()->NewJSObject(object_function, i::TENURED);
      i::Handle<i::String> module_property_name =
          i_isolate->factory()->InternalizeUtf8String("module");
      i::Handle<i::String> instance_property_name =
          i_isolate->factory()->InternalizeUtf8String("instance");
      i::JSObject::AddProperty(i_retval, module_property_name, module_obj,
                               i::NONE);
      i::JSObject::AddProperty(i_retval, instance_property_name,
                               Utils::OpenHandle(*instance.ToLocalChecked()),
                               i::NONE);
      retval = Utils::ToLocal(i_retval);
    } else {
      retval = instance.ToLocalChecked();
    }
    DCHECK(!retval.IsEmpty());
    resolver->Resolve(context, retval);
  }
}

bool GetIntegerProperty(v8::Isolate* isolate, ErrorThrower* thrower,
                        Local<Context> context, Local<v8::Object> object,
                        Local<String> property, int* result,
                        int64_t lower_bound, uint64_t upper_bound) {
  v8::MaybeLocal<v8::Value> maybe = object->Get(context, property);
  v8::Local<v8::Value> value;
  if (maybe.ToLocal(&value)) {
    int64_t number;
    if (!value->IntegerValue(context).To(&number)) return false;
    if (number < lower_bound) {
      thrower->RangeError("Property value %" PRId64
                          " is below the lower bound %" PRIx64,
                          number, lower_bound);
      return false;
    }
    if (number > static_cast<int64_t>(upper_bound)) {
      thrower->RangeError("Property value %" PRId64
                          " is above the upper bound %" PRIu64,
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
  // The descriptor's 'initial'.
  int initial = 0;
  if (!GetIntegerProperty(isolate, &thrower, context, descriptor,
                          v8_str(isolate, "initial"), &initial, 0,
                          i::wasm::kV8MaxWasmTableSize)) {
    return;
  }
  // The descriptor's 'maximum'.
  int maximum = -1;
  Local<String> maximum_key = v8_str(isolate, "maximum");
  Maybe<bool> has_maximum = descriptor->Has(context, maximum_key);

  if (!has_maximum.IsNothing() && has_maximum.FromJust()) {
    if (!GetIntegerProperty(isolate, &thrower, context, descriptor, maximum_key,
                            &maximum, initial,
                            i::wasm::kSpecMaxWasmTableSize)) {
      return;
    }
  }

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::Handle<i::FixedArray> fixed_array;
  i::Handle<i::JSObject> table_obj =
      i::WasmTableObject::New(i_isolate, initial, maximum, &fixed_array);
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(table_obj));
}

void WebAssemblyMemory(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  ErrorThrower thrower(reinterpret_cast<i::Isolate*>(isolate),
                       "WebAssembly.Memory()");
  if (args.Length() < 1 || !args[0]->IsObject()) {
    thrower.TypeError("Argument 0 must be a memory descriptor");
    return;
  }
  Local<Context> context = isolate->GetCurrentContext();
  Local<v8::Object> descriptor = args[0]->ToObject(context).ToLocalChecked();
  // The descriptor's 'initial'.
  int initial = 0;
  if (!GetIntegerProperty(isolate, &thrower, context, descriptor,
                          v8_str(isolate, "initial"), &initial, 0,
                          i::wasm::kV8MaxWasmMemoryPages)) {
    return;
  }
  // The descriptor's 'maximum'.
  int maximum = -1;
  Local<String> maximum_key = v8_str(isolate, "maximum");
  Maybe<bool> has_maximum = descriptor->Has(context, maximum_key);

  if (!has_maximum.IsNothing() && has_maximum.FromJust()) {
    if (!GetIntegerProperty(isolate, &thrower, context, descriptor, maximum_key,
                            &maximum, initial,
                            i::wasm::kSpecMaxWasmMemoryPages)) {
      return;
    }
  }
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  size_t size = static_cast<size_t>(i::wasm::WasmModule::kPageSize) *
                static_cast<size_t>(initial);
  i::Handle<i::JSArrayBuffer> buffer =
      i::wasm::NewArrayBuffer(i_isolate, size, i::FLAG_wasm_guard_pages);
  if (buffer.is_null()) {
    thrower.RangeError("could not allocate memory");
    return;
  }
  i::Handle<i::JSObject> memory_obj =
      i::WasmMemoryObject::New(i_isolate, buffer, maximum);
  args.GetReturnValue().Set(Utils::ToLocal(memory_obj));
}

void WebAssemblyTableGetLength(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);
  if (!BrandCheck(isolate, Utils::OpenHandle(*args.This()),
                  i::Handle<i::Symbol>(i_context->wasm_table_sym()),
                  "Receiver is not a WebAssembly.Table")) {
    return;
  }
  auto receiver =
      i::Handle<i::WasmTableObject>::cast(Utils::OpenHandle(*args.This()));
  args.GetReturnValue().Set(
      v8::Number::New(isolate, receiver->current_length()));
}

void WebAssemblyTableGrow(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);
  if (!BrandCheck(isolate, Utils::OpenHandle(*args.This()),
                  i::Handle<i::Symbol>(i_context->wasm_table_sym()),
                  "Receiver is not a WebAssembly.Table")) {
    return;
  }

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  auto receiver =
      i::Handle<i::WasmTableObject>::cast(Utils::OpenHandle(*args.This()));
  i::Handle<i::FixedArray> old_array(receiver->functions(), i_isolate);
  int old_size = old_array->length();
  int64_t new_size64 = 0;
  if (args.Length() > 0 && !args[0]->IntegerValue(context).To(&new_size64)) {
    return;
  }
  new_size64 += old_size;

  int64_t max_size64 = receiver->maximum_length();
  if (max_size64 < 0 ||
      max_size64 > static_cast<int64_t>(i::wasm::kV8MaxWasmTableSize)) {
    max_size64 = i::wasm::kV8MaxWasmTableSize;
  }

  if (new_size64 < old_size || new_size64 > max_size64) {
    v8::Local<v8::Value> e = v8::Exception::RangeError(
        v8_str(isolate, new_size64 < old_size ? "trying to shrink table"
                                              : "maximum table size exceeded"));
    isolate->ThrowException(e);
    return;
  }

  int new_size = static_cast<int>(new_size64);
  i::WasmTableObject::Grow(i_isolate, receiver,
                           static_cast<uint32_t>(new_size - old_size));

  if (new_size != old_size) {
    i::Handle<i::FixedArray> new_array =
        i_isolate->factory()->NewFixedArray(new_size);
    for (int i = 0; i < old_size; ++i) new_array->set(i, old_array->get(i));
    i::Object* null = i_isolate->heap()->null_value();
    for (int i = old_size; i < new_size; ++i) new_array->set(i, null);
    receiver->set_functions(*new_array);
  }

  // TODO(gdeepti): use weak links for instances
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(old_size);
}

void WebAssemblyTableGet(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);
  if (!BrandCheck(isolate, Utils::OpenHandle(*args.This()),
                  i::Handle<i::Symbol>(i_context->wasm_table_sym()),
                  "Receiver is not a WebAssembly.Table")) {
    return;
  }

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  auto receiver =
      i::Handle<i::WasmTableObject>::cast(Utils::OpenHandle(*args.This()));
  i::Handle<i::FixedArray> array(receiver->functions(), i_isolate);
  int i = 0;
  if (args.Length() > 0 && !args[0]->Int32Value(context).To(&i)) return;
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  if (i < 0 || i >= array->length()) {
    v8::Local<v8::Value> e =
        v8::Exception::RangeError(v8_str(isolate, "index out of bounds"));
    isolate->ThrowException(e);
    return;
  }

  i::Handle<i::Object> value(array->get(i), i_isolate);
  return_value.Set(Utils::ToLocal(value));
}

void WebAssemblyTableSet(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);
  if (!BrandCheck(isolate, Utils::OpenHandle(*args.This()),
                  i::Handle<i::Symbol>(i_context->wasm_table_sym()),
                  "Receiver is not a WebAssembly.Table")) {
    return;
  }
  if (args.Length() < 2) {
    v8::Local<v8::Value> e = v8::Exception::TypeError(
        v8_str(isolate, "Argument 1 must be null or a function"));
    isolate->ThrowException(e);
    return;
  }
  i::Handle<i::Object> value = Utils::OpenHandle(*args[1]);
  if (!value->IsNull(i_isolate) &&
      (!value->IsJSFunction() ||
       i::Handle<i::JSFunction>::cast(value)->code()->kind() !=
           i::Code::JS_TO_WASM_FUNCTION)) {
    v8::Local<v8::Value> e = v8::Exception::TypeError(
        v8_str(isolate, "Argument 1 must be null or a WebAssembly function"));
    isolate->ThrowException(e);
    return;
  }

  auto receiver =
      i::Handle<i::WasmTableObject>::cast(Utils::OpenHandle(*args.This()));
  i::Handle<i::FixedArray> array(receiver->functions(), i_isolate);
  int i;
  if (!args[0]->Int32Value(context).To(&i)) return;
  if (i < 0 || i >= array->length()) {
    v8::Local<v8::Value> e =
        v8::Exception::RangeError(v8_str(isolate, "index out of bounds"));
    isolate->ThrowException(e);
    return;
  }

  i::Handle<i::FixedArray> dispatch_tables(receiver->dispatch_tables(),
                                           i_isolate);
  if (value->IsNull(i_isolate)) {
    i::wasm::UpdateDispatchTables(i_isolate, dispatch_tables, i,
                                  i::Handle<i::JSFunction>::null());
  } else {
    i::wasm::UpdateDispatchTables(i_isolate, dispatch_tables, i,
                                  i::Handle<i::JSFunction>::cast(value));
  }

  i::Handle<i::FixedArray>::cast(array)->set(i, *value);
}

void WebAssemblyMemoryGrow(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);
  if (!BrandCheck(isolate, Utils::OpenHandle(*args.This()),
                  i::Handle<i::Symbol>(i_context->wasm_memory_sym()),
                  "Receiver is not a WebAssembly.Memory")) {
    return;
  }
  int64_t delta_size = 0;
  if (args.Length() < 1 || !args[0]->IntegerValue(context).To(&delta_size)) {
    v8::Local<v8::Value> e = v8::Exception::TypeError(
        v8_str(isolate, "Argument 0 required, must be numeric value of pages"));
    isolate->ThrowException(e);
    return;
  }
  i::Handle<i::WasmMemoryObject> receiver =
      i::Handle<i::WasmMemoryObject>::cast(Utils::OpenHandle(*args.This()));
  int64_t max_size64 = receiver->maximum_pages();
  if (max_size64 < 0 ||
      max_size64 > static_cast<int64_t>(i::wasm::kV8MaxWasmTableSize)) {
    max_size64 = i::wasm::kV8MaxWasmMemoryPages;
  }
  i::Handle<i::JSArrayBuffer> old_buffer(receiver->buffer());
  uint32_t old_size =
      old_buffer->byte_length()->Number() / i::wasm::kSpecMaxWasmMemoryPages;
  int64_t new_size64 = old_size + delta_size;
  if (delta_size < 0 || max_size64 < new_size64 || new_size64 < old_size) {
    v8::Local<v8::Value> e = v8::Exception::RangeError(v8_str(
        isolate, new_size64 < old_size ? "trying to shrink memory"
                                       : "maximum memory size exceeded"));
    isolate->ThrowException(e);
    return;
  }
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  int32_t ret = i::wasm::GrowWebAssemblyMemory(
      i_isolate, receiver, static_cast<uint32_t>(delta_size));
  if (ret == -1) {
    v8::Local<v8::Value> e = v8::Exception::RangeError(
        v8_str(isolate, "Unable to grow instance memory."));
    isolate->ThrowException(e);
    return;
  }
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(ret);
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
  i::Handle<i::WasmMemoryObject> receiver =
      i::Handle<i::WasmMemoryObject>::cast(Utils::OpenHandle(*args.This()));
  i::Handle<i::Object> buffer(receiver->buffer(), i_isolate);
  DCHECK(buffer->IsJSArrayBuffer());
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(buffer));
}
}  // namespace

// TODO(titzer): we use the API to create the function template because the
// internal guts are too ugly to replicate here.
static i::Handle<i::FunctionTemplateInfo> NewTemplate(i::Isolate* i_isolate,
                                                      FunctionCallback func) {
  Isolate* isolate = reinterpret_cast<Isolate*>(i_isolate);
  Local<FunctionTemplate> templ = FunctionTemplate::New(isolate, func);
  templ->ReadOnlyPrototype();
  return v8::Utils::OpenHandle(*templ);
}

namespace internal {

Handle<JSFunction> InstallFunc(Isolate* isolate, Handle<JSObject> object,
                               const char* str, FunctionCallback func,
                               int length = 0) {
  Handle<String> name = v8_str(isolate, str);
  Handle<FunctionTemplateInfo> temp = NewTemplate(isolate, func);
  Handle<JSFunction> function =
      ApiNatives::InstantiateFunction(temp).ToHandleChecked();
  JSFunction::SetName(function, name, isolate->factory()->empty_string());
  function->shared()->set_length(length);
  PropertyAttributes attributes = static_cast<PropertyAttributes>(DONT_ENUM);
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
      static_cast<v8::PropertyAttribute>(v8::DontEnum);
  Utils::ToLocal(object)->SetAccessorProperty(Utils::ToLocal(name),
                                              Utils::ToLocal(function),
                                              Local<Function>(), attributes);
  return function;
}

void WasmJs::Install(Isolate* isolate) {
  Handle<JSGlobalObject> global = isolate->global_object();
  Handle<Context> context(global->native_context(), isolate);
  // TODO(titzer): once FLAG_expose_wasm is gone, this should become a DCHECK.
  if (context->get(Context::WASM_FUNCTION_MAP_INDEX)->IsMap()) return;

  // Install Maps.

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

  // Install symbols.

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

  // Install the JS API.

  // Setup WebAssembly
  Handle<String> name = v8_str(isolate, "WebAssembly");
  Handle<JSFunction> cons = factory->NewFunction(name);
  JSFunction::SetInstancePrototype(
      cons, Handle<Object>(context->initial_object_prototype(), isolate));
  cons->shared()->set_instance_class_name(*name);
  Handle<JSObject> webassembly = factory->NewJSObject(cons, TENURED);
  PropertyAttributes attributes = static_cast<PropertyAttributes>(DONT_ENUM);
  JSObject::AddProperty(global, name, webassembly, attributes);
  PropertyAttributes ro_attributes =
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);
  JSObject::AddProperty(webassembly, factory->to_string_tag_symbol(),
                        v8_str(isolate, "WebAssembly"), ro_attributes);
  InstallFunc(isolate, webassembly, "compile", WebAssemblyCompile, 1);
  InstallFunc(isolate, webassembly, "validate", WebAssemblyValidate, 1);
  InstallFunc(isolate, webassembly, "instantiate", WebAssemblyInstantiate, 1);

  // Setup Module
  Handle<JSFunction> module_constructor =
      InstallFunc(isolate, webassembly, "Module", WebAssemblyModule, 1);
  context->set_wasm_module_constructor(*module_constructor);
  Handle<JSObject> module_proto =
      factory->NewJSObject(module_constructor, TENURED);
  i::Handle<i::Map> module_map = isolate->factory()->NewMap(
      i::JS_API_OBJECT_TYPE, i::JSObject::kHeaderSize +
                             WasmModuleObject::kFieldCount * i::kPointerSize);
  JSFunction::SetInitialMap(module_constructor, module_map, module_proto);
  InstallFunc(isolate, module_constructor, "imports", WebAssemblyModuleImports,
              1);
  InstallFunc(isolate, module_constructor, "exports", WebAssemblyModuleExports,
              1);
  InstallFunc(isolate, module_constructor, "customSections",
              WebAssemblyModuleCustomSections, 2);
  JSObject::AddProperty(module_proto, isolate->factory()->constructor_string(),
                        module_constructor, DONT_ENUM);
  JSObject::AddProperty(module_proto, factory->to_string_tag_symbol(),
                        v8_str(isolate, "WebAssembly.Module"), ro_attributes);

  // Setup Instance
  Handle<JSFunction> instance_constructor =
      InstallFunc(isolate, webassembly, "Instance", WebAssemblyInstance, 1);
  context->set_wasm_instance_constructor(*instance_constructor);
  Handle<JSObject> instance_proto =
      factory->NewJSObject(instance_constructor, TENURED);
  i::Handle<i::Map> instance_map = isolate->factory()->NewMap(
      i::JS_API_OBJECT_TYPE, i::JSObject::kHeaderSize +
                             WasmInstanceObject::kFieldCount * i::kPointerSize);
  JSFunction::SetInitialMap(instance_constructor, instance_map, instance_proto);
  JSObject::AddProperty(instance_proto,
                        isolate->factory()->constructor_string(),
                        instance_constructor, DONT_ENUM);
  JSObject::AddProperty(instance_proto, factory->to_string_tag_symbol(),
                        v8_str(isolate, "WebAssembly.Instance"), ro_attributes);

  // Setup Table
  Handle<JSFunction> table_constructor =
      InstallFunc(isolate, webassembly, "Table", WebAssemblyTable, 1);
  context->set_wasm_table_constructor(*table_constructor);
  Handle<JSObject> table_proto =
      factory->NewJSObject(table_constructor, TENURED);
  i::Handle<i::Map> table_map = isolate->factory()->NewMap(
      i::JS_API_OBJECT_TYPE, i::JSObject::kHeaderSize +
                             WasmTableObject::kFieldCount * i::kPointerSize);
  JSFunction::SetInitialMap(table_constructor, table_map, table_proto);
  JSObject::AddProperty(table_proto, isolate->factory()->constructor_string(),
                        table_constructor, DONT_ENUM);
  InstallGetter(isolate, table_proto, "length", WebAssemblyTableGetLength);
  InstallFunc(isolate, table_proto, "grow", WebAssemblyTableGrow, 1);
  InstallFunc(isolate, table_proto, "get", WebAssemblyTableGet, 1);
  InstallFunc(isolate, table_proto, "set", WebAssemblyTableSet, 2);
  JSObject::AddProperty(table_proto, factory->to_string_tag_symbol(),
                        v8_str(isolate, "WebAssembly.Table"), ro_attributes);

  // Setup Memory
  Handle<JSFunction> memory_constructor =
      InstallFunc(isolate, webassembly, "Memory", WebAssemblyMemory, 1);
  context->set_wasm_memory_constructor(*memory_constructor);
  Handle<JSObject> memory_proto =
      factory->NewJSObject(memory_constructor, TENURED);
  i::Handle<i::Map> memory_map = isolate->factory()->NewMap(
      i::JS_API_OBJECT_TYPE, i::JSObject::kHeaderSize +
                             WasmMemoryObject::kFieldCount * i::kPointerSize);
  JSFunction::SetInitialMap(memory_constructor, memory_map, memory_proto);
  JSObject::AddProperty(memory_proto, isolate->factory()->constructor_string(),
                        memory_constructor, DONT_ENUM);
  InstallFunc(isolate, memory_proto, "grow", WebAssemblyMemoryGrow, 1);
  InstallGetter(isolate, memory_proto, "buffer", WebAssemblyMemoryGetBuffer);
  JSObject::AddProperty(memory_proto, factory->to_string_tag_symbol(),
                        v8_str(isolate, "WebAssembly.Memory"), ro_attributes);

  // Setup errors
  attributes = static_cast<PropertyAttributes>(DONT_ENUM);
  Handle<JSFunction> compile_error(
      isolate->native_context()->wasm_compile_error_function());
  JSObject::AddProperty(webassembly, isolate->factory()->CompileError_string(),
                        compile_error, attributes);
  Handle<JSFunction> link_error(
      isolate->native_context()->wasm_link_error_function());
  JSObject::AddProperty(webassembly, isolate->factory()->LinkError_string(),
                        link_error, attributes);
  Handle<JSFunction> runtime_error(
      isolate->native_context()->wasm_runtime_error_function());
  JSObject::AddProperty(webassembly, isolate->factory()->RuntimeError_string(),
                        runtime_error, attributes);
}

static bool HasBrand(i::Handle<i::Object> value, i::Handle<i::Symbol> symbol) {
  if (value->IsJSObject()) {
    i::Handle<i::JSObject> object = i::Handle<i::JSObject>::cast(value);
    Maybe<bool> has_brand = i::JSObject::HasOwnProperty(object, symbol);
    if (has_brand.IsNothing()) return false;
    if (has_brand.ToChecked()) return true;
  }
  return false;
}

bool WasmJs::IsWasmMemoryObject(Isolate* isolate, Handle<Object> value) {
  i::Handle<i::Symbol> symbol(isolate->context()->wasm_memory_sym(), isolate);
  return HasBrand(value, symbol);
}

bool WasmJs::IsWasmTableObject(Isolate* isolate, Handle<Object> value) {
  i::Handle<i::Symbol> symbol(isolate->context()->wasm_table_sym(), isolate);
  return HasBrand(value, symbol);
}
}  // namespace internal
}  // namespace v8

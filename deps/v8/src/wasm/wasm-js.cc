// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api-natives.h"
#include "src/api.h"
#include "src/assert-scope.h"
#include "src/ast/ast.h"
#include "src/execution.h"
#include "src/factory.h"
#include "src/handles.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
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

#define ASSIGN(type, var, expr)                      \
  Local<type> var;                                   \
  do {                                               \
    if (!expr.ToLocal(&var)) {                       \
      DCHECK(i_isolate->has_scheduled_exception());  \
      return;                                        \
    } else {                                         \
      DCHECK(!i_isolate->has_scheduled_exception()); \
    }                                                \
  } while (false)

// TODO(wasm): move brand check to the respective types, and don't throw
// in it, rather, use a provided ErrorThrower, or let caller handle it.
static bool HasBrand(i::Handle<i::Object> value, i::Handle<i::Symbol> sym) {
  if (!value->IsJSObject()) return false;
  i::Handle<i::JSObject> object = i::Handle<i::JSObject>::cast(value);
  Maybe<bool> has_brand = i::JSObject::HasOwnProperty(object, sym);
  return has_brand.FromMaybe(false);
}

static bool BrandCheck(i::Handle<i::Object> value, i::Handle<i::Symbol> sym,
                       ErrorThrower* thrower, const char* msg) {
  return HasBrand(value, sym) ? true : (thrower->TypeError("%s", msg), false);
}

i::Handle<i::String> v8_str(i::Isolate* isolate, const char* str) {
  return isolate->factory()->NewStringFromAsciiChecked(str);
}
Local<String> v8_str(Isolate* isolate, const char* str) {
  return Utils::ToLocal(v8_str(reinterpret_cast<i::Isolate*>(isolate), str));
}

i::MaybeHandle<i::WasmModuleObject> GetFirstArgumentAsModule(
    const v8::FunctionCallbackInfo<v8::Value>& args, ErrorThrower* thrower) {
  v8::Isolate* isolate = args.GetIsolate();
  if (args.Length() < 1) {
    thrower->TypeError("Argument 0 must be a WebAssembly.Module");
    return {};
  }

  Local<Context> context = isolate->GetCurrentContext();
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);
  if (!BrandCheck(Utils::OpenHandle(*args[0]),
                  i::handle(i_context->wasm_module_sym()), thrower,
                  "Argument 0 must be a WebAssembly.Module")) {
    return {};
  }

  Local<Object> module_obj = Local<Object>::Cast(args[0]);
  return i::Handle<i::WasmModuleObject>::cast(
      v8::Utils::OpenHandle(*module_obj));
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

i::MaybeHandle<i::JSReceiver> GetValueAsImports(Local<Value> arg,
                                                ErrorThrower* thrower) {
  if (arg->IsUndefined()) return {};

  if (!arg->IsObject()) {
    thrower->TypeError("Argument 1 must be an object");
    return {};
  }
  Local<Object> obj = Local<Object>::Cast(arg);
  return i::Handle<i::JSReceiver>::cast(v8::Utils::OpenHandle(*obj));
}

void RejectResponseAPI(const v8::FunctionCallbackInfo<v8::Value>& args,
                       ErrorThrower* thrower) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  HandleScope scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();

  ASSIGN(Promise::Resolver, resolver, Promise::Resolver::New(context));
  Local<Promise> module_promise = resolver->GetPromise();
  args.GetReturnValue().Set(module_promise);
  thrower->TypeError(
      "Argument 0 must be provided and must be a Response or Response promise");
  auto maybe = resolver->Reject(context, Utils::ToLocal(thrower->Reify()));
  CHECK_IMPLIES(!maybe.FromMaybe(false), i_isolate->has_scheduled_exception());
}

void WebAssemblyCompileStreaming(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  MicrotasksScope runs_microtasks(isolate, MicrotasksScope::kRunMicrotasks);
  if (!i_isolate->wasm_compile_callback()(args)) {
    ErrorThrower thrower(i_isolate, "WebAssembly.compileStreaming()");
    RejectResponseAPI(args, &thrower);
  }
}

// WebAssembly.compile(bytes) -> Promise
void WebAssemblyCompile(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  MicrotasksScope runs_microtasks(isolate, MicrotasksScope::kRunMicrotasks);
  if (i_isolate->wasm_compile_callback()(args)) return;

  HandleScope scope(isolate);
  ErrorThrower thrower(i_isolate, "WebAssembly.compile()");

  Local<Context> context = isolate->GetCurrentContext();
  ASSIGN(Promise::Resolver, resolver, Promise::Resolver::New(context));
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(resolver->GetPromise());

  auto bytes = GetFirstArgumentAsBytes(args, &thrower);
  if (thrower.error()) {
    auto maybe = resolver->Reject(context, Utils::ToLocal(thrower.Reify()));
    CHECK_IMPLIES(!maybe.FromMaybe(false),
                  i_isolate->has_scheduled_exception());
    return;
  }
  i::Handle<i::JSPromise> promise = Utils::OpenHandle(*resolver->GetPromise());
  i::wasm::AsyncCompile(i_isolate, promise, bytes);
}

// WebAssembly.validate(bytes) -> bool
void WebAssemblyValidate(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ErrorThrower thrower(i_isolate, "WebAssembly.validate()");

  auto bytes = GetFirstArgumentAsBytes(args, &thrower);

  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  if (!thrower.error() &&
      i::wasm::SyncValidate(reinterpret_cast<i::Isolate*>(isolate), bytes)) {
    return_value.Set(v8::True(isolate));
  } else {
    if (thrower.wasm_error()) thrower.Reset();  // Clear error.
    return_value.Set(v8::False(isolate));
  }
}

// new WebAssembly.Module(bytes) -> WebAssembly.Module
void WebAssemblyModule(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  if (i_isolate->wasm_module_callback()(args)) return;

  HandleScope scope(isolate);
  ErrorThrower thrower(i_isolate, "WebAssembly.Module()");

  auto bytes = GetFirstArgumentAsBytes(args, &thrower);

  if (thrower.error()) {
    return;
  }
  i::MaybeHandle<i::Object> module_obj =
      i::wasm::SyncCompile(i_isolate, &thrower, bytes);
  if (module_obj.is_null()) return;

  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(module_obj.ToHandleChecked()));
}

// WebAssembly.Module.imports(module) -> Array<Import>
void WebAssemblyModuleImports(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ErrorThrower thrower(i_isolate, "WebAssembly.Module.imports()");

  auto maybe_module = GetFirstArgumentAsModule(args, &thrower);
  if (thrower.error()) return;
  auto imports = i::wasm::GetImports(i_isolate, maybe_module.ToHandleChecked());
  args.GetReturnValue().Set(Utils::ToLocal(imports));
}

// WebAssembly.Module.exports(module) -> Array<Export>
void WebAssemblyModuleExports(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ErrorThrower thrower(i_isolate, "WebAssembly.Module.exports()");

  auto maybe_module = GetFirstArgumentAsModule(args, &thrower);
  if (thrower.error()) return;
  auto exports = i::wasm::GetExports(i_isolate, maybe_module.ToHandleChecked());
  args.GetReturnValue().Set(Utils::ToLocal(exports));
}

// WebAssembly.Module.customSections(module, name) -> Array<Section>
void WebAssemblyModuleCustomSections(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ErrorThrower thrower(i_isolate, "WebAssembly.Module.customSections()");

  auto maybe_module = GetFirstArgumentAsModule(args, &thrower);
  if (thrower.error()) return;

  if (args.Length() < 2) {
    thrower.TypeError("Argument 1 must be a string");
    return;
  }

  i::Handle<i::Object> name = Utils::OpenHandle(*args[1]);
  if (!name->IsString()) {
    thrower.TypeError("Argument 1 must be a string");
    return;
  }

  auto custom_sections =
      i::wasm::GetCustomSections(i_isolate, maybe_module.ToHandleChecked(),
                                 i::Handle<i::String>::cast(name), &thrower);
  if (thrower.error()) return;
  args.GetReturnValue().Set(Utils::ToLocal(custom_sections));
}

MaybeLocal<Value> WebAssemblyInstantiateImpl(Isolate* isolate,
                                             Local<Value> module,
                                             Local<Value> ffi) {
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  ErrorThrower thrower(i_isolate, "WebAssembly Instantiation");
  i::MaybeHandle<i::JSReceiver> maybe_imports =
      GetValueAsImports(ffi, &thrower);
  if (thrower.error()) return {};

  i::Handle<i::WasmModuleObject> module_obj =
      i::Handle<i::WasmModuleObject>::cast(
          Utils::OpenHandle(Object::Cast(*module)));
  i::MaybeHandle<i::Object> instance_object =
      i::wasm::SyncInstantiate(i_isolate, &thrower, module_obj, maybe_imports,
                               i::MaybeHandle<i::JSArrayBuffer>());

  if (instance_object.is_null()) {
    // TODO(wasm): this *should* mean there's an error to throw, but
    // we exit sometimes the instantiation pipeline without throwing.
    // v8:6232.
    return {};
  }
  return Utils::ToLocal(instance_object.ToHandleChecked());
}

// Entered as internal implementation detail of sync and async instantiate.
// args[0] *must* be a WebAssembly.Module.
void WebAssemblyInstantiateImplCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK_GE(args.Length(), 1);
  v8::Isolate* isolate = args.GetIsolate();
  MicrotasksScope does_not_run_microtasks(isolate,
                                          MicrotasksScope::kDoNotRunMicrotasks);

  HandleScope scope(args.GetIsolate());
  Local<Value> module = args[0];
  Local<Value> ffi = args.Data();
  Local<Value> instance;
  if (WebAssemblyInstantiateImpl(isolate, module, ffi).ToLocal(&instance)) {
    args.GetReturnValue().Set(instance);
  }
}

void WebAssemblyInstantiateToPairCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK_GE(args.Length(), 1);
  Isolate* isolate = args.GetIsolate();
  MicrotasksScope does_not_run_microtasks(isolate,
                                          MicrotasksScope::kDoNotRunMicrotasks);

  HandleScope scope(args.GetIsolate());

  Local<Context> context = isolate->GetCurrentContext();
  Local<Value> module = args[0];

  const uint8_t* instance_str = reinterpret_cast<const uint8_t*>("instance");
  const uint8_t* module_str = reinterpret_cast<const uint8_t*>("module");
  Local<Value> instance;
  if (!WebAssemblyInstantiateImpl(isolate, module, args.Data())
           .ToLocal(&instance)) {
    return;
  }

  Local<Object> ret = Object::New(isolate);
  Local<String> instance_name =
      String::NewFromOneByte(isolate, instance_str,
                             NewStringType::kInternalized)
          .ToLocalChecked();
  Local<String> module_name =
      String::NewFromOneByte(isolate, module_str, NewStringType::kInternalized)
          .ToLocalChecked();

  CHECK(ret->CreateDataProperty(context, instance_name, instance).IsJust());
  CHECK(ret->CreateDataProperty(context, module_name, module).IsJust());
  args.GetReturnValue().Set(ret);
}

// new WebAssembly.Instance(module, imports) -> WebAssembly.Instance
void WebAssemblyInstance(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = args.GetIsolate();
  MicrotasksScope does_not_run_microtasks(isolate,
                                          MicrotasksScope::kDoNotRunMicrotasks);

  HandleScope scope(args.GetIsolate());
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  if (i_isolate->wasm_instance_callback()(args)) return;

  ErrorThrower thrower(i_isolate, "WebAssembly.Instance()");

  auto maybe_module = GetFirstArgumentAsModule(args, &thrower);
  if (thrower.error()) return;

  // If args.Length < 2, this will be undefined - see FunctionCallbackInfo.
  // We'll check for that in WebAssemblyInstantiateImpl.
  Local<Value> data = args[1];

  Local<Value> instance;
  if (WebAssemblyInstantiateImpl(isolate, args[0], data).ToLocal(&instance)) {
    args.GetReturnValue().Set(instance);
  }
}

void WebAssemblyInstantiateStreaming(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  // we use i_isolate in DCHECKS in the ASSIGN statements.
  USE(i_isolate);
  MicrotasksScope runs_microtasks(isolate, MicrotasksScope::kRunMicrotasks);
  HandleScope scope(isolate);

  Local<Context> context = isolate->GetCurrentContext();
  ASSIGN(Promise::Resolver, resolver, Promise::Resolver::New(context));
  Local<Value> first_arg_value = args[0];

  ASSIGN(Function, compileStreaming,
         Function::New(context, WebAssemblyCompileStreaming));
  ASSIGN(Value, compile_retval,
         compileStreaming->Call(context, args.Holder(), 1, &first_arg_value));
  Local<Promise> module_promise = Local<Promise>::Cast(compile_retval);

  DCHECK(!module_promise.IsEmpty());
  Local<Value> data = args[1];
  ASSIGN(Function, instantiate_impl,
         Function::New(context, WebAssemblyInstantiateToPairCallback, data));
  ASSIGN(Promise, result, module_promise->Then(context, instantiate_impl));
  args.GetReturnValue().Set(result);
}

// WebAssembly.instantiate(module, imports) -> WebAssembly.Instance
// WebAssembly.instantiate(bytes, imports) ->
//     {module: WebAssembly.Module, instance: WebAssembly.Instance}
void WebAssemblyInstantiate(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  MicrotasksScope runs_microtasks(isolate, MicrotasksScope::kRunMicrotasks);
  if (i_isolate->wasm_instantiate_callback()(args)) return;

  ErrorThrower thrower(i_isolate, "WebAssembly.instantiate()");

  HandleScope scope(isolate);

  Local<Context> context = isolate->GetCurrentContext();
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);

  ASSIGN(Promise::Resolver, resolver, Promise::Resolver::New(context));
  Local<Promise> module_promise = resolver->GetPromise();
  args.GetReturnValue().Set(module_promise);

  if (args.Length() < 1) {
    thrower.TypeError(
        "Argument 0 must be provided and must be either a buffer source or a "
        "WebAssembly.Module object");
    auto maybe = resolver->Reject(context, Utils::ToLocal(thrower.Reify()));
    CHECK_IMPLIES(!maybe.FromMaybe(false),
                  i_isolate->has_scheduled_exception());
    return;
  }

  Local<Value> first_arg_value = args[0];
  i::Handle<i::Object> first_arg = Utils::OpenHandle(*first_arg_value);
  if (!first_arg->IsJSObject()) {
    thrower.TypeError(
        "Argument 0 must be a buffer source or a WebAssembly.Module object");
    auto maybe = resolver->Reject(context, Utils::ToLocal(thrower.Reify()));
    CHECK_IMPLIES(!maybe.FromMaybe(false),
                  i_isolate->has_scheduled_exception());
    return;
  }

  FunctionCallback instantiator = nullptr;
  if (HasBrand(first_arg, i::Handle<i::Symbol>(i_context->wasm_module_sym()))) {
    module_promise = resolver->GetPromise();
    if (!resolver->Resolve(context, first_arg_value).IsJust()) return;
    instantiator = WebAssemblyInstantiateImplCallback;
  } else {
    ASSIGN(Function, async_compile, Function::New(context, WebAssemblyCompile));
    ASSIGN(Value, async_compile_retval,
           async_compile->Call(context, args.Holder(), 1, &first_arg_value));
    module_promise = Local<Promise>::Cast(async_compile_retval);
    instantiator = WebAssemblyInstantiateToPairCallback;
  }
  DCHECK(!module_promise.IsEmpty());
  DCHECK_NOT_NULL(instantiator);
  // If args.Length < 2, this will be undefined - see FunctionCallbackInfo.
  // We'll check for that in WebAssemblyInstantiateImpl.
  Local<Value> data = args[1];
  ASSIGN(Function, instantiate_impl,
         Function::New(context, instantiator, data));
  ASSIGN(Promise, result, module_promise->Then(context, instantiate_impl));
  args.GetReturnValue().Set(result);
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

// new WebAssembly.Table(args) -> WebAssembly.Table
void WebAssemblyTable(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ErrorThrower thrower(i_isolate, "WebAssembly.Module()");
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
                          i::FLAG_wasm_max_table_size)) {
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

  i::Handle<i::FixedArray> fixed_array;
  i::Handle<i::JSObject> table_obj =
      i::WasmTableObject::New(i_isolate, initial, maximum, &fixed_array);
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(table_obj));
}

void WebAssemblyMemory(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ErrorThrower thrower(i_isolate, "WebAssembly.Memory()");
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
                          i::FLAG_wasm_max_mem_pages)) {
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
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ErrorThrower thrower(i_isolate, "WebAssembly.Table.length()");
  Local<Context> context = isolate->GetCurrentContext();
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);
  if (!BrandCheck(Utils::OpenHandle(*args.This()),
                  i::Handle<i::Symbol>(i_context->wasm_table_sym()), &thrower,
                  "Receiver is not a WebAssembly.Table")) {
    return;
  }
  auto receiver =
      i::Handle<i::WasmTableObject>::cast(Utils::OpenHandle(*args.This()));
  args.GetReturnValue().Set(
      v8::Number::New(isolate, receiver->current_length()));
}

// WebAssembly.Table.grow(num) -> num
void WebAssemblyTableGrow(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ErrorThrower thrower(i_isolate, "WebAssembly.Table.grow()");
  Local<Context> context = isolate->GetCurrentContext();
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);
  if (!BrandCheck(Utils::OpenHandle(*args.This()),
                  i::Handle<i::Symbol>(i_context->wasm_table_sym()), &thrower,
                  "Receiver is not a WebAssembly.Table")) {
    return;
  }

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
      max_size64 > static_cast<int64_t>(i::FLAG_wasm_max_table_size)) {
    max_size64 = i::FLAG_wasm_max_table_size;
  }

  if (new_size64 < old_size || new_size64 > max_size64) {
    thrower.RangeError(new_size64 < old_size ? "trying to shrink table"
                                             : "maximum table size exceeded");
    return;
  }

  int new_size = static_cast<int>(new_size64);
  receiver->grow(i_isolate, static_cast<uint32_t>(new_size - old_size));

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

// WebAssembly.Table.get(num) -> JSFunction
void WebAssemblyTableGet(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ErrorThrower thrower(i_isolate, "WebAssembly.Table.get()");
  Local<Context> context = isolate->GetCurrentContext();
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);
  if (!BrandCheck(Utils::OpenHandle(*args.This()),
                  i::Handle<i::Symbol>(i_context->wasm_table_sym()), &thrower,
                  "Receiver is not a WebAssembly.Table")) {
    return;
  }

  auto receiver =
      i::Handle<i::WasmTableObject>::cast(Utils::OpenHandle(*args.This()));
  i::Handle<i::FixedArray> array(receiver->functions(), i_isolate);
  int i = 0;
  if (args.Length() > 0 && !args[0]->Int32Value(context).To(&i)) return;
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  if (i < 0 || i >= array->length()) {
    thrower.RangeError("index out of bounds");
    return;
  }

  i::Handle<i::Object> value(array->get(i), i_isolate);
  return_value.Set(Utils::ToLocal(value));
}

// WebAssembly.Table.set(num, JSFunction)
void WebAssemblyTableSet(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ErrorThrower thrower(i_isolate, "WebAssembly.Table.set()");
  Local<Context> context = isolate->GetCurrentContext();
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);
  if (!BrandCheck(Utils::OpenHandle(*args.This()),
                  i::Handle<i::Symbol>(i_context->wasm_table_sym()), &thrower,
                  "Receiver is not a WebAssembly.Table")) {
    return;
  }
  if (args.Length() < 2) {
    thrower.TypeError("Argument 1 must be null or a function");
    return;
  }
  i::Handle<i::Object> value = Utils::OpenHandle(*args[1]);
  if (!value->IsNull(i_isolate) &&
      (!value->IsJSFunction() ||
       i::Handle<i::JSFunction>::cast(value)->code()->kind() !=
           i::Code::JS_TO_WASM_FUNCTION)) {
    thrower.TypeError("Argument 1 must be null or a WebAssembly function");
    return;
  }

  auto receiver =
      i::Handle<i::WasmTableObject>::cast(Utils::OpenHandle(*args.This()));
  i::Handle<i::FixedArray> array(receiver->functions(), i_isolate);
  int i;
  if (!args[0]->Int32Value(context).To(&i)) return;
  if (i < 0 || i >= array->length()) {
    thrower.RangeError("index out of bounds");
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

// WebAssembly.Memory.grow(num) -> num
void WebAssemblyMemoryGrow(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ErrorThrower thrower(i_isolate, "WebAssembly.Memory.grow()");
  Local<Context> context = isolate->GetCurrentContext();
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);
  if (!BrandCheck(Utils::OpenHandle(*args.This()),
                  i::Handle<i::Symbol>(i_context->wasm_memory_sym()), &thrower,
                  "Receiver is not a WebAssembly.Memory")) {
    return;
  }
  int64_t delta_size = 0;
  if (args.Length() < 1 || !args[0]->IntegerValue(context).To(&delta_size)) {
    thrower.TypeError("Argument 0 required, must be numeric value of pages");
    return;
  }
  i::Handle<i::WasmMemoryObject> receiver =
      i::Handle<i::WasmMemoryObject>::cast(Utils::OpenHandle(*args.This()));
  int64_t max_size64 = receiver->maximum_pages();
  if (max_size64 < 0 ||
      max_size64 > static_cast<int64_t>(i::FLAG_wasm_max_mem_pages)) {
    max_size64 = i::FLAG_wasm_max_mem_pages;
  }
  i::Handle<i::JSArrayBuffer> old_buffer(receiver->buffer());
  uint32_t old_size =
      old_buffer->byte_length()->Number() / i::wasm::kSpecMaxWasmMemoryPages;
  int64_t new_size64 = old_size + delta_size;
  if (delta_size < 0 || max_size64 < new_size64 || new_size64 < old_size) {
    thrower.RangeError(new_size64 < old_size ? "trying to shrink memory"
                                             : "maximum memory size exceeded");
    return;
  }
  int32_t ret = i::WasmMemoryObject::Grow(i_isolate, receiver,
                                          static_cast<uint32_t>(delta_size));
  if (ret == -1) {
    thrower.RangeError("Unable to grow instance memory.");
    return;
  }
  bool free_memory = (delta_size != 0);
  i::wasm::DetachWebAssemblyMemoryBuffer(i_isolate, old_buffer, free_memory);
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(ret);
}

// WebAssembly.Memory.buffer -> ArrayBuffer
void WebAssemblyMemoryGetBuffer(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ErrorThrower thrower(i_isolate, "WebAssembly.Memory.buffer");
  Local<Context> context = isolate->GetCurrentContext();
  i::Handle<i::Context> i_context = Utils::OpenHandle(*context);
  if (!BrandCheck(Utils::OpenHandle(*args.This()),
                  i::Handle<i::Symbol>(i_context->wasm_memory_sym()), &thrower,
                  "Receiver is not a WebAssembly.Memory")) {
    return;
  }
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
  int embedder_fields = JSObject::GetEmbedderFieldCount(*prev_map);
  CHECK_EQ(0, embedder_fields);
  int pre_allocated =
      prev_map->GetInObjectProperties() - prev_map->unused_property_fields();
  int instance_size = 0;
  int in_object_properties = 0;
  int wasm_embedder_fields = embedder_fields + 1  // module instance object
                             + 1                  // function arity
                             + 1;                 // function signature
  JSFunction::CalculateInstanceSizeHelper(instance_type, wasm_embedder_fields,
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
  JSFunction::SetPrototype(cons, isolate->initial_object_prototype());
  cons->shared()->set_instance_class_name(*name);
  Handle<JSObject> webassembly = factory->NewJSObject(cons, TENURED);
  PropertyAttributes attributes = static_cast<PropertyAttributes>(DONT_ENUM);
  JSObject::AddProperty(global, name, webassembly, attributes);
  PropertyAttributes ro_attributes =
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);
  JSObject::AddProperty(webassembly, factory->to_string_tag_symbol(),
                        v8_str(isolate, "WebAssembly"), ro_attributes);
  InstallFunc(isolate, webassembly, "compile", WebAssemblyCompile, 1);
  InstallFunc(isolate, webassembly, "compileStreaming",
              WebAssemblyCompileStreaming, 1);
  InstallFunc(isolate, webassembly, "validate", WebAssemblyValidate, 1);
  InstallFunc(isolate, webassembly, "instantiate", WebAssemblyInstantiate, 1);
  InstallFunc(isolate, webassembly, "instantiateStreaming",
              WebAssemblyInstantiateStreaming, 1);

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

// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-js.h"

#include "src/api-natives.h"
#include "src/api.h"
#include "src/assert-scope.h"
#include "src/ast/ast.h"
#include "src/execution.h"
#include "src/handles.h"
#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-memory.h"
#include "src/wasm/wasm-objects-inl.h"

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

// Like an ErrorThrower, but turns all pending exceptions into scheduled
// exceptions when going out of scope. Use this in API methods.
// Note that pending exceptions are not necessarily created by the ErrorThrower,
// but e.g. by the wasm start function. There might also be a scheduled
// exception, created by another API call (e.g. v8::Object::Get). But there
// should never be both pending and scheduled exceptions.
class ScheduledErrorThrower : public ErrorThrower {
 public:
  ScheduledErrorThrower(i::Isolate* isolate, const char* context)
      : ErrorThrower(isolate, context) {}

  ~ScheduledErrorThrower();
};

ScheduledErrorThrower::~ScheduledErrorThrower() {
  // There should never be both a pending and a scheduled exception.
  DCHECK(!isolate()->has_scheduled_exception() ||
         !isolate()->has_pending_exception());
  // Don't throw another error if there is already a scheduled error.
  if (isolate()->has_scheduled_exception()) {
    Reset();
  } else if (isolate()->has_pending_exception()) {
    Reset();
    isolate()->OptionalRescheduleException(false);
  } else if (error()) {
    isolate()->ScheduleThrow(*Reify());
  }
}

i::Handle<i::String> v8_str(i::Isolate* isolate, const char* str) {
  return isolate->factory()->NewStringFromAsciiChecked(str);
}
Local<String> v8_str(Isolate* isolate, const char* str) {
  return Utils::ToLocal(v8_str(reinterpret_cast<i::Isolate*>(isolate), str));
}

i::MaybeHandle<i::WasmModuleObject> GetFirstArgumentAsModule(
    const v8::FunctionCallbackInfo<v8::Value>& args, ErrorThrower* thrower) {
  i::Handle<i::Object> arg0 = Utils::OpenHandle(*args[0]);
  if (!arg0->IsWasmModuleObject()) {
    thrower->TypeError("Argument 0 must be a WebAssembly.Module");
    return {};
  }

  Local<Object> module_obj = Local<Object>::Cast(args[0]);
  return i::Handle<i::WasmModuleObject>::cast(
      v8::Utils::OpenHandle(*module_obj));
}

i::wasm::ModuleWireBytes GetFirstArgumentAsBytes(
    const v8::FunctionCallbackInfo<v8::Value>& args, ErrorThrower* thrower,
    bool* is_shared) {
  const uint8_t* start = nullptr;
  size_t length = 0;
  v8::Local<v8::Value> source = args[0];
  if (source->IsArrayBuffer()) {
    // A raw array buffer was passed.
    Local<ArrayBuffer> buffer = Local<ArrayBuffer>::Cast(source);
    ArrayBuffer::Contents contents = buffer->GetContents();

    start = reinterpret_cast<const uint8_t*>(contents.Data());
    length = contents.ByteLength();
    *is_shared = buffer->IsSharedArrayBuffer();
  } else if (source->IsTypedArray()) {
    // A TypedArray was passed.
    Local<TypedArray> array = Local<TypedArray>::Cast(source);
    Local<ArrayBuffer> buffer = array->Buffer();

    ArrayBuffer::Contents contents = buffer->GetContents();

    start =
        reinterpret_cast<const uint8_t*>(contents.Data()) + array->ByteOffset();
    length = array->ByteLength();
    *is_shared = buffer->IsSharedArrayBuffer();
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

void WebAssemblyCompileStreaming(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);

  if (!i::wasm::IsWasmCodegenAllowed(i_isolate, i_isolate->native_context())) {
    // Manually create a promise and reject it.
    Local<Context> context = isolate->GetCurrentContext();
    ASSIGN(Promise::Resolver, resolver, Promise::Resolver::New(context));
    v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
    return_value.Set(resolver->GetPromise());
    ScheduledErrorThrower thrower(i_isolate, "WebAssembly.compileStreaming()");
    thrower.CompileError("Wasm code generation disallowed by embedder");
    auto maybe = resolver->Reject(context, Utils::ToLocal(thrower.Reify()));
    CHECK_IMPLIES(!maybe.FromMaybe(false),
                  i_isolate->has_scheduled_exception());
    return;
  }

  MicrotasksScope runs_microtasks(isolate, MicrotasksScope::kRunMicrotasks);
  DCHECK_NOT_NULL(i_isolate->wasm_compile_streaming_callback());
  i_isolate->wasm_compile_streaming_callback()(args);
}

// WebAssembly.compile(bytes) -> Promise
void WebAssemblyCompile(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  MicrotasksScope runs_microtasks(isolate, MicrotasksScope::kRunMicrotasks);

  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.compile()");

  if (!i::wasm::IsWasmCodegenAllowed(i_isolate, i_isolate->native_context())) {
    thrower.CompileError("Wasm code generation disallowed by embedder");
  }

  Local<Context> context = isolate->GetCurrentContext();
  ASSIGN(Promise::Resolver, resolver, Promise::Resolver::New(context));
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(resolver->GetPromise());

  bool is_shared = false;
  auto bytes = GetFirstArgumentAsBytes(args, &thrower, &is_shared);
  if (thrower.error()) {
    auto maybe = resolver->Reject(context, Utils::ToLocal(thrower.Reify()));
    CHECK_IMPLIES(!maybe.FromMaybe(false),
                  i_isolate->has_scheduled_exception());
    return;
  }
  i::Handle<i::JSPromise> promise = Utils::OpenHandle(*resolver->GetPromise());
  // Asynchronous compilation handles copying wire bytes if necessary.
  i_isolate->wasm_engine()->AsyncCompile(i_isolate, promise, bytes, is_shared);
}

// WebAssembly.validate(bytes) -> bool
void WebAssemblyValidate(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.validate()");

  bool is_shared = false;
  auto bytes = GetFirstArgumentAsBytes(args, &thrower, &is_shared);

  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();

  if (thrower.error()) {
    if (thrower.wasm_error()) thrower.Reset();  // Clear error.
    return_value.Set(v8::False(isolate));
    return;
  }

  bool validated = false;
  if (is_shared) {
    // Make a copy of the wire bytes to avoid concurrent modification.
    std::unique_ptr<uint8_t[]> copy(new uint8_t[bytes.length()]);
    memcpy(copy.get(), bytes.start(), bytes.length());
    i::wasm::ModuleWireBytes bytes_copy(copy.get(),
                                        copy.get() + bytes.length());
    validated = i_isolate->wasm_engine()->SyncValidate(i_isolate, bytes_copy);
  } else {
    // The wire bytes are not shared, OK to use them directly.
    validated = i_isolate->wasm_engine()->SyncValidate(i_isolate, bytes);
  }

  return_value.Set(Boolean::New(isolate, validated));
}

// new WebAssembly.Module(bytes) -> WebAssembly.Module
void WebAssemblyModule(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  if (i_isolate->wasm_module_callback()(args)) return;

  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Module()");

  if (!args.IsConstructCall()) {
    thrower.TypeError("WebAssembly.Module must be invoked with 'new'");
    return;
  }
  if (!i::wasm::IsWasmCodegenAllowed(i_isolate, i_isolate->native_context())) {
    thrower.CompileError("Wasm code generation disallowed by embedder");
    return;
  }

  bool is_shared = false;
  auto bytes = GetFirstArgumentAsBytes(args, &thrower, &is_shared);

  if (thrower.error()) {
    return;
  }
  i::MaybeHandle<i::Object> module_obj;
  if (is_shared) {
    // Make a copy of the wire bytes to avoid concurrent modification.
    std::unique_ptr<uint8_t[]> copy(new uint8_t[bytes.length()]);
    memcpy(copy.get(), bytes.start(), bytes.length());
    i::wasm::ModuleWireBytes bytes_copy(copy.get(),
                                        copy.get() + bytes.length());
    module_obj =
        i_isolate->wasm_engine()->SyncCompile(i_isolate, &thrower, bytes_copy);
  } else {
    // The wire bytes are not shared, OK to use them directly.
    module_obj =
        i_isolate->wasm_engine()->SyncCompile(i_isolate, &thrower, bytes);
  }

  if (module_obj.is_null()) return;

  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(module_obj.ToHandleChecked()));
}

// WebAssembly.Module.imports(module) -> Array<Import>
void WebAssemblyModuleImports(const v8::FunctionCallbackInfo<v8::Value>& args) {
  HandleScope scope(args.GetIsolate());
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Module.imports()");

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
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Module.exports()");

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
  ScheduledErrorThrower thrower(i_isolate,
                                "WebAssembly.Module.customSections()");

  auto maybe_module = GetFirstArgumentAsModule(args, &thrower);
  if (thrower.error()) return;

  i::MaybeHandle<i::Object> maybe_name =
      i::Object::ToString(i_isolate, Utils::OpenHandle(*args[1]));
  i::Handle<i::Object> name;
  if (!maybe_name.ToHandle(&name)) return;
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

  i::MaybeHandle<i::Object> instance_object;
  {
    ScheduledErrorThrower thrower(i_isolate, "WebAssembly Instantiation");

    // TODO(ahaas): These checks on the module should not be necessary here They
    // are just a workaround for https://crbug.com/837417.
    i::Handle<i::Object> module_obj = Utils::OpenHandle(*module);
    if (!module_obj->IsWasmModuleObject()) {
      thrower.TypeError("Argument 0 must be a WebAssembly.Module object");
      return {};
    }

    i::MaybeHandle<i::JSReceiver> maybe_imports =
        GetValueAsImports(ffi, &thrower);
    if (thrower.error()) return {};

    instance_object = i_isolate->wasm_engine()->SyncInstantiate(
        i_isolate, &thrower, i::Handle<i::WasmModuleObject>::cast(module_obj),
        maybe_imports, i::MaybeHandle<i::JSArrayBuffer>());
  }

  DCHECK_EQ(instance_object.is_null(), i_isolate->has_scheduled_exception());
  if (instance_object.is_null()) return {};
  return Utils::ToLocal(instance_object.ToHandleChecked());
}

void WebAssemblyInstantiateCallback(
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
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i_isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kWebAssemblyInstantiation);
  MicrotasksScope does_not_run_microtasks(isolate,
                                          MicrotasksScope::kDoNotRunMicrotasks);

  HandleScope scope(args.GetIsolate());
  if (i_isolate->wasm_instance_callback()(args)) return;

  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Instance()");
  if (!args.IsConstructCall()) {
    thrower.TypeError("WebAssembly.Instance must be invoked with 'new'");
    return;
  }

  GetFirstArgumentAsModule(args, &thrower);
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
  i_isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kWebAssemblyInstantiation);
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
         Function::New(context, WebAssemblyInstantiateCallback, data));
  ASSIGN(Promise, result, module_promise->Then(context, instantiate_impl));
  args.GetReturnValue().Set(result);
}

// WebAssembly.instantiate(module, imports) -> WebAssembly.Instance
// WebAssembly.instantiate(bytes, imports) ->
//     {module: WebAssembly.Module, instance: WebAssembly.Instance}
void WebAssemblyInstantiate(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i_isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kWebAssemblyInstantiation);
  MicrotasksScope runs_microtasks(isolate, MicrotasksScope::kRunMicrotasks);

  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.instantiate()");

  HandleScope scope(isolate);

  Local<Context> context = isolate->GetCurrentContext();

  ASSIGN(Promise::Resolver, resolver, Promise::Resolver::New(context));
  Local<Promise> promise = resolver->GetPromise();
  args.GetReturnValue().Set(promise);

  Local<Value> first_arg_value = args[0];
  // If args.Length < 2, this will be undefined - see FunctionCallbackInfo.
  Local<Value> ffi = args[1];
  i::Handle<i::Object> first_arg = Utils::OpenHandle(*first_arg_value);
  if (!first_arg->IsJSObject()) {
    thrower.TypeError(
        "Argument 0 must be a buffer source or a WebAssembly.Module object");
    auto maybe = resolver->Reject(context, Utils::ToLocal(thrower.Reify()));
    CHECK_IMPLIES(!maybe.FromMaybe(false),
                  i_isolate->has_scheduled_exception());
    return;
  }

  if (first_arg->IsWasmModuleObject()) {
    i::Handle<i::WasmModuleObject> module_obj =
        i::Handle<i::WasmModuleObject>::cast(first_arg);
    // If args.Length < 2, this will be undefined - see FunctionCallbackInfo.
    i::MaybeHandle<i::JSReceiver> maybe_imports =
        GetValueAsImports(ffi, &thrower);

    if (thrower.error()) {
      auto maybe = resolver->Reject(context, Utils::ToLocal(thrower.Reify()));
      CHECK_IMPLIES(!maybe.FromMaybe(false),
                    i_isolate->has_scheduled_exception());
      return;
    }

    i_isolate->wasm_engine()->AsyncInstantiate(
        i_isolate, Utils::OpenHandle(*promise), module_obj, maybe_imports);
    return;
  }

  // We did not get a WasmModuleObject as input, we first have to compile the
  // input.
  ASSIGN(Function, async_compile, Function::New(context, WebAssemblyCompile));
  ASSIGN(Value, async_compile_retval,
         async_compile->Call(context, args.Holder(), 1, &first_arg_value));
  promise = Local<Promise>::Cast(async_compile_retval);
  DCHECK(!promise.IsEmpty());
  ASSIGN(Function, instantiate_impl,
         Function::New(context, WebAssemblyInstantiateCallback, ffi));
  ASSIGN(Promise, result, promise->Then(context, instantiate_impl));
  args.GetReturnValue().Set(result);
}

bool GetIntegerProperty(v8::Isolate* isolate, ErrorThrower* thrower,
                        Local<Context> context, Local<v8::Object> object,
                        Local<String> property, int64_t* result,
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
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Module()");
  if (!args.IsConstructCall()) {
    thrower.TypeError("WebAssembly.Table must be invoked with 'new'");
    return;
  }
  if (!args[0]->IsObject()) {
    thrower.TypeError("Argument 0 must be a table descriptor");
    return;
  }
  Local<Context> context = isolate->GetCurrentContext();
  Local<v8::Object> descriptor = Local<Object>::Cast(args[0]);
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
  int64_t initial = 0;
  if (!GetIntegerProperty(isolate, &thrower, context, descriptor,
                          v8_str(isolate, "initial"), &initial, 0,
                          i::FLAG_wasm_max_table_size)) {
    return;
  }
  // The descriptor's 'maximum'.
  int64_t maximum = -1;
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
  i::Handle<i::JSObject> table_obj = i::WasmTableObject::New(
      i_isolate, static_cast<uint32_t>(initial), maximum, &fixed_array);
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(table_obj));
}

void WebAssemblyMemory(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Memory()");
  if (!args.IsConstructCall()) {
    thrower.TypeError("WebAssembly.Memory must be invoked with 'new'");
    return;
  }
  if (!args[0]->IsObject()) {
    thrower.TypeError("Argument 0 must be a memory descriptor");
    return;
  }
  Local<Context> context = isolate->GetCurrentContext();
  Local<v8::Object> descriptor = Local<Object>::Cast(args[0]);
  // The descriptor's 'initial'.
  int64_t initial = 0;
  if (!GetIntegerProperty(isolate, &thrower, context, descriptor,
                          v8_str(isolate, "initial"), &initial, 0,
                          i::FLAG_wasm_max_mem_pages)) {
    return;
  }
  // The descriptor's 'maximum'.
  int64_t maximum = -1;
  Local<String> maximum_key = v8_str(isolate, "maximum");
  Maybe<bool> has_maximum = descriptor->Has(context, maximum_key);

  if (!has_maximum.IsNothing() && has_maximum.FromJust()) {
    if (!GetIntegerProperty(isolate, &thrower, context, descriptor, maximum_key,
                            &maximum, initial,
                            i::wasm::kSpecMaxWasmMemoryPages)) {
      return;
    }
  }

  bool is_shared_memory = false;
  if (i::FLAG_experimental_wasm_threads) {
    // Shared property of descriptor
    Local<String> shared_key = v8_str(isolate, "shared");
    Maybe<bool> has_shared = descriptor->Has(context, shared_key);
    if (!has_shared.IsNothing() && has_shared.FromJust()) {
      v8::MaybeLocal<v8::Value> maybe = descriptor->Get(context, shared_key);
      v8::Local<v8::Value> value;
      if (maybe.ToLocal(&value)) {
        if (!value->BooleanValue(context).To(&is_shared_memory)) return;
      }
    }
    // Throw TypeError if shared is true, and the descriptor has no "maximum"
    if (is_shared_memory && maximum == -1) {
      thrower.TypeError(
          "If shared is true, maximum property should be defined.");
    }
  }

  size_t size = static_cast<size_t>(i::wasm::kWasmPageSize) *
                static_cast<size_t>(initial);
  const bool enable_guard_regions =
      internal::trap_handler::IsTrapHandlerEnabled();
  i::SharedFlag shared_flag =
      is_shared_memory ? i::SharedFlag::kShared : i::SharedFlag::kNotShared;
  i::Handle<i::JSArrayBuffer> buffer;
  if (!i::wasm::NewArrayBuffer(i_isolate, size, enable_guard_regions,
                               shared_flag)
           .ToHandle(&buffer)) {
    thrower.RangeError("could not allocate memory");
    return;
  }
  if (buffer->is_shared()) {
    Maybe<bool> result =
        buffer->SetIntegrityLevel(buffer, i::FROZEN, i::kDontThrow);
    if (!result.FromJust()) {
      thrower.TypeError(
          "Status of setting SetIntegrityLevel of buffer is false.");
    }
  }
  i::Handle<i::JSObject> memory_obj = i::WasmMemoryObject::New(
      i_isolate, buffer, static_cast<int32_t>(maximum));
  args.GetReturnValue().Set(Utils::ToLocal(memory_obj));
}

void WebAssemblyGlobal(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Global()");
  if (!args.IsConstructCall()) {
    thrower.TypeError("WebAssembly.Global must be invoked with 'new'");
    return;
  }
  if (!args[0]->IsObject()) {
    thrower.TypeError("Argument 0 must be a global descriptor");
    return;
  }
  Local<Context> context = isolate->GetCurrentContext();
  Local<v8::Object> descriptor = Local<Object>::Cast(args[0]);

  // The descriptor's 'value'.
  v8::MaybeLocal<v8::Value> maybe_value =
      descriptor->Get(context, v8_str(isolate, "value"));

  // The descriptor's 'mutable'.
  bool is_mutable = false;
  {
    Local<String> mutable_key = v8_str(isolate, "mutable");
    v8::MaybeLocal<v8::Value> maybe = descriptor->Get(context, mutable_key);
    v8::Local<v8::Value> value;
    if (maybe.ToLocal(&value)) {
      if (!value->BooleanValue(context).To(&is_mutable)) return;
    }
  }

  // The descriptor's 'type'.
  i::wasm::ValueType type;
  {
    v8::MaybeLocal<v8::Value> maybe =
        descriptor->Get(context, v8_str(isolate, "type"));
    v8::Local<v8::Value> value;
    if (!maybe.ToLocal(&value)) return;
    v8::Local<v8::String> string;
    if (!value->ToString(context).ToLocal(&string)) return;

    bool equal;
    if (string->Equals(context, v8_str(isolate, "i32")).To(&equal) && equal) {
      type = i::wasm::kWasmI32;
    } else if (string->Equals(context, v8_str(isolate, "f32")).To(&equal) &&
               equal) {
      type = i::wasm::kWasmF32;
    } else if (string->Equals(context, v8_str(isolate, "f64")).To(&equal) &&
               equal) {
      type = i::wasm::kWasmF64;
    } else {
      thrower.TypeError(
          "Descriptor property 'type' must be 'i32', 'f32', or 'f64'");
      return;
    }
  }

  const uint32_t offset = 0;
  i::MaybeHandle<i::WasmGlobalObject> maybe_global_obj =
      i::WasmGlobalObject::New(i_isolate, i::MaybeHandle<i::JSArrayBuffer>(),
                               type, offset, is_mutable);

  i::Handle<i::WasmGlobalObject> global_obj;
  if (!maybe_global_obj.ToHandle(&global_obj)) {
    thrower.RangeError("could not allocate memory");
    return;
  }

  // Convert value to a WebAssembly value.
  v8::Local<v8::Value> value;
  if (maybe_value.ToLocal(&value)) {
    switch (type) {
      case i::wasm::kWasmI32: {
        int32_t i32_value = 0;
        v8::Local<v8::Int32> int32_value;
        if (!value->ToInt32(context).ToLocal(&int32_value)) return;
        if (!int32_value->Int32Value(context).To(&i32_value)) return;
        global_obj->SetI32(i32_value);
        break;
      }
      case i::wasm::kWasmF32: {
        double f64_value = 0;
        v8::Local<v8::Number> number_value;
        if (!value->ToNumber(context).ToLocal(&number_value)) return;
        if (!number_value->NumberValue(context).To(&f64_value)) return;
        float f32_value = static_cast<float>(f64_value);
        global_obj->SetF32(f32_value);
        break;
      }
      case i::wasm::kWasmF64: {
        double f64_value = 0;
        v8::Local<v8::Number> number_value;
        if (!value->ToNumber(context).ToLocal(&number_value)) return;
        if (!number_value->NumberValue(context).To(&f64_value)) return;
        global_obj->SetF64(f64_value);
        break;
      }
      default:
        UNREACHABLE();
    }
  }

  i::Handle<i::JSObject> global_js_object(global_obj);
  args.GetReturnValue().Set(Utils::ToLocal(global_js_object));
}

constexpr const char* kName_WasmGlobalObject = "WebAssembly.Global";
constexpr const char* kName_WasmMemoryObject = "WebAssembly.Memory";
constexpr const char* kName_WasmInstanceObject = "WebAssembly.Instance";
constexpr const char* kName_WasmTableObject = "WebAssembly.Table";

#define EXTRACT_THIS(var, WasmType)                                  \
  i::Handle<i::WasmType> var;                                        \
  {                                                                  \
    i::Handle<i::Object> this_arg = Utils::OpenHandle(*args.This()); \
    if (!this_arg->Is##WasmType()) {                                 \
      thrower.TypeError("Receiver is not a %s", kName_##WasmType);   \
      return;                                                        \
    }                                                                \
    var = i::Handle<i::WasmType>::cast(this_arg);                    \
  }

void WebAssemblyInstanceGetExports(
  const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Instance.exports()");
  EXTRACT_THIS(receiver, WasmInstanceObject);
  i::Handle<i::JSObject> exports_object(receiver->exports_object());
  args.GetReturnValue().Set(Utils::ToLocal(exports_object));
}

void WebAssemblyTableGetLength(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Table.length()");
  EXTRACT_THIS(receiver, WasmTableObject);
  args.GetReturnValue().Set(
      v8::Number::New(isolate, receiver->current_length()));
}

// WebAssembly.Table.grow(num) -> num
void WebAssemblyTableGrow(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Table.grow()");
  Local<Context> context = isolate->GetCurrentContext();
  EXTRACT_THIS(receiver, WasmTableObject);

  int64_t grow_by = 0;
  if (!args[0]->IntegerValue(context).To(&grow_by)) return;
  i::Handle<i::FixedArray> old_array(receiver->functions(), i_isolate);
  int old_size = old_array->length();

  int64_t max_size64 = receiver->maximum_length()->Number();
  if (max_size64 < 0 || max_size64 > i::FLAG_wasm_max_table_size) {
    max_size64 = i::FLAG_wasm_max_table_size;
  }

  if (grow_by < 0 || grow_by > max_size64 - old_size) {
    thrower.RangeError(grow_by < 0 ? "trying to shrink table"
                                   : "maximum table size exceeded");
    return;
  }

  int new_size = static_cast<int>(old_size + grow_by);
  receiver->Grow(i_isolate, static_cast<uint32_t>(new_size - old_size));

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
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Table.get()");
  Local<Context> context = isolate->GetCurrentContext();
  EXTRACT_THIS(receiver, WasmTableObject);
  i::Handle<i::FixedArray> array(receiver->functions(), i_isolate);
  int64_t i = 0;
  if (!args[0]->IntegerValue(context).To(&i)) return;
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  if (i < 0 || i >= array->length()) {
    thrower.RangeError("index out of bounds");
    return;
  }

  i::Handle<i::Object> value(array->get(static_cast<int>(i)), i_isolate);
  return_value.Set(Utils::ToLocal(value));
}

// WebAssembly.Table.set(num, JSFunction)
void WebAssemblyTableSet(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Table.set()");
  Local<Context> context = isolate->GetCurrentContext();
  EXTRACT_THIS(receiver, WasmTableObject);

  // Parameter 0.
  int64_t index;
  if (!args[0]->IntegerValue(context).To(&index)) return;

  // Parameter 1.
  i::Handle<i::Object> value = Utils::OpenHandle(*args[1]);
  if (!value->IsNull(i_isolate) &&
      !i::WasmExportedFunction::IsWasmExportedFunction(*value)) {
    thrower.TypeError("Argument 1 must be null or a WebAssembly function");
    return;
  }

  if (index < 0 || index >= receiver->functions()->length()) {
    thrower.RangeError("index out of bounds");
    return;
  }

  // TODO(v8:7232) Allow reset/mutation after addressing referenced issue.
  int32_t int_index = static_cast<int32_t>(index);
  if (receiver->functions()->get(int_index) !=
          i_isolate->heap()->undefined_value() &&
      receiver->functions()->get(int_index) !=
          i_isolate->heap()->null_value()) {
    for (i::StackFrameIterator it(i_isolate); !it.done(); it.Advance()) {
      if (it.frame()->type() == i::StackFrame::WASM_TO_JS) {
        thrower.RangeError("Modifying existing entry in table not supported.");
        return;
      }
    }
  }
  i::WasmTableObject::Set(i_isolate, receiver, static_cast<int32_t>(int_index),
                          value->IsNull(i_isolate)
                              ? i::Handle<i::JSFunction>::null()
                              : i::Handle<i::JSFunction>::cast(value));
}

// WebAssembly.Memory.grow(num) -> num
void WebAssemblyMemoryGrow(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Memory.grow()");
  Local<Context> context = isolate->GetCurrentContext();
  EXTRACT_THIS(receiver, WasmMemoryObject);

  int64_t delta_size = 0;
  if (!args[0]->IntegerValue(context).To(&delta_size)) return;

  int64_t max_size64 = receiver->maximum_pages();
  if (max_size64 < 0 ||
      max_size64 > static_cast<int64_t>(i::FLAG_wasm_max_mem_pages)) {
    max_size64 = i::FLAG_wasm_max_mem_pages;
  }
  i::Handle<i::JSArrayBuffer> old_buffer(receiver->array_buffer());
  if (!old_buffer->is_growable()) {
    thrower.RangeError("This memory cannot be grown");
    return;
  }
  uint32_t old_size =
      old_buffer->byte_length()->Number() / i::wasm::kWasmPageSize;
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
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(ret);
}

// WebAssembly.Memory.buffer -> ArrayBuffer
void WebAssemblyMemoryGetBuffer(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, "WebAssembly.Memory.buffer");
  EXTRACT_THIS(receiver, WasmMemoryObject);

  i::Handle<i::Object> buffer_obj(receiver->array_buffer(), i_isolate);
  DCHECK(buffer_obj->IsJSArrayBuffer());
  i::Handle<i::JSArrayBuffer> buffer(i::JSArrayBuffer::cast(*buffer_obj));
  if (buffer->is_shared()) {
    // TODO(gdeepti): More needed here for when cached buffer, and current
    // buffer are out of sync, handle that here when bounds checks, and Grow
    // are handled correctly.
    Maybe<bool> result =
        buffer->SetIntegrityLevel(buffer, i::FROZEN, i::kDontThrow);
    if (!result.FromJust()) {
      thrower.TypeError(
          "Status of setting SetIntegrityLevel of buffer is false.");
    }
  }
  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();
  return_value.Set(Utils::ToLocal(buffer));
}

void WebAssemblyGlobalGetValueCommon(
    const v8::FunctionCallbackInfo<v8::Value>& args, const char* name) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  ScheduledErrorThrower thrower(i_isolate, name);
  EXTRACT_THIS(receiver, WasmGlobalObject);

  v8::ReturnValue<v8::Value> return_value = args.GetReturnValue();

  switch (receiver->type()) {
    case i::wasm::kWasmI32:
      return_value.Set(receiver->GetI32());
      break;
    case i::wasm::kWasmI64:
      thrower.TypeError("Can't get the value of i64 WebAssembly.Global");
      break;
    case i::wasm::kWasmF32:
      return_value.Set(receiver->GetF32());
      break;
    case i::wasm::kWasmF64:
      return_value.Set(receiver->GetF64());
      break;
    default:
      UNREACHABLE();
  }
}

// WebAssembly.Global.valueOf() -> num
void WebAssemblyGlobalValueOf(const v8::FunctionCallbackInfo<v8::Value>& args) {
  return WebAssemblyGlobalGetValueCommon(args, "WebAssembly.Global.valueOf()");
}

// get WebAssembly.Global.value -> num
void WebAssemblyGlobalGetValue(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  return WebAssemblyGlobalGetValueCommon(args, "get WebAssembly.Global.value");
}

// set WebAssembly.Global.value(num)
void WebAssemblyGlobalSetValue(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  HandleScope scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  ScheduledErrorThrower thrower(i_isolate, "set WebAssembly.Global.value");
  EXTRACT_THIS(receiver, WasmGlobalObject);

  switch (receiver->type()) {
    case i::wasm::kWasmI32: {
      int32_t i32_value = 0;
      if (!args[0]->Int32Value(context).To(&i32_value)) return;
      receiver->SetI32(i32_value);
      break;
    }
    case i::wasm::kWasmI64:
      thrower.TypeError("Can't set the value of i64 WebAssembly.Global");
      break;
    case i::wasm::kWasmF32: {
      double f64_value = 0;
      if (!args[0]->NumberValue(context).To(&f64_value)) return;
      receiver->SetF32(static_cast<float>(f64_value));
      break;
    }
    case i::wasm::kWasmF64: {
      double f64_value = 0;
      if (!args[0]->NumberValue(context).To(&f64_value)) return;
      receiver->SetF64(f64_value);
      break;
    }
    default:
      UNREACHABLE();
  }
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

Handle<JSFunction> CreateFunc(Isolate* isolate, Handle<String> name,
                              FunctionCallback func) {
  Handle<FunctionTemplateInfo> temp = NewTemplate(isolate, func);
  Handle<JSFunction> function =
      ApiNatives::InstantiateFunction(temp, name).ToHandleChecked();
  DCHECK(function->shared()->HasSharedName());
  return function;
}

Handle<JSFunction> InstallFunc(Isolate* isolate, Handle<JSObject> object,
                               const char* str, FunctionCallback func,
                               int length = 0) {
  Handle<String> name = v8_str(isolate, str);
  Handle<JSFunction> function = CreateFunc(isolate, name, func);
  function->shared()->set_length(length);
  PropertyAttributes attributes = static_cast<PropertyAttributes>(DONT_ENUM);
  JSObject::AddProperty(object, name, function, attributes);
  return function;
}

Handle<String> GetterName(Isolate* isolate, Handle<String> name) {
  return Name::ToFunctionName(name, isolate->factory()->get_string())
      .ToHandleChecked();
}

void InstallGetter(Isolate* isolate, Handle<JSObject> object,
                   const char* str, FunctionCallback func) {
  Handle<String> name = v8_str(isolate, str);
  Handle<JSFunction> function =
      CreateFunc(isolate, GetterName(isolate, name), func);

  v8::PropertyAttribute attributes =
      static_cast<v8::PropertyAttribute>(v8::DontEnum);
  Utils::ToLocal(object)->SetAccessorProperty(Utils::ToLocal(name),
                                              Utils::ToLocal(function),
                                              Local<Function>(), attributes);
}

Handle<String> SetterName(Isolate* isolate, Handle<String> name) {
  return Name::ToFunctionName(name, isolate->factory()->set_string())
      .ToHandleChecked();
}

void InstallGetterSetter(Isolate* isolate, Handle<JSObject> object,
                         const char* str, FunctionCallback getter,
                         FunctionCallback setter) {
  Handle<String> name = v8_str(isolate, str);
  Handle<JSFunction> getter_func =
      CreateFunc(isolate, GetterName(isolate, name), getter);
  Handle<JSFunction> setter_func =
      CreateFunc(isolate, SetterName(isolate, name), setter);

  v8::PropertyAttribute attributes =
      static_cast<v8::PropertyAttribute>(v8::DontEnum);

  Utils::ToLocal(object)->SetAccessorProperty(
      Utils::ToLocal(name), Utils::ToLocal(getter_func),
      Utils::ToLocal(setter_func), attributes);
}

void WasmJs::Install(Isolate* isolate, bool exposed_on_global_object) {
  Handle<JSGlobalObject> global = isolate->global_object();
  Handle<Context> context(global->native_context(), isolate);
  // Install the JS API once only.
  Object* prev = context->get(Context::WASM_MODULE_CONSTRUCTOR_INDEX);
  if (!prev->IsUndefined(isolate)) {
    DCHECK(prev->IsJSFunction());
    return;
  }

  Factory* factory = isolate->factory();

  // Setup WebAssembly
  Handle<String> name = v8_str(isolate, "WebAssembly");
  NewFunctionArgs args = NewFunctionArgs::ForFunctionWithoutCode(
      name, isolate->strict_function_map(), LanguageMode::kStrict);
  Handle<JSFunction> cons = factory->NewFunction(args);
  JSFunction::SetPrototype(cons, isolate->initial_object_prototype());
  Handle<JSObject> webassembly = factory->NewJSObject(cons, TENURED);
  PropertyAttributes attributes = static_cast<PropertyAttributes>(DONT_ENUM);

  PropertyAttributes ro_attributes =
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY);
  JSObject::AddProperty(webassembly, factory->to_string_tag_symbol(), name,
                        ro_attributes);
  InstallFunc(isolate, webassembly, "compile", WebAssemblyCompile, 1);
  InstallFunc(isolate, webassembly, "validate", WebAssemblyValidate, 1);
  InstallFunc(isolate, webassembly, "instantiate", WebAssemblyInstantiate, 1);

  if (isolate->wasm_compile_streaming_callback() != nullptr) {
    InstallFunc(isolate, webassembly, "compileStreaming",
                WebAssemblyCompileStreaming, 1);
    InstallFunc(isolate, webassembly, "instantiateStreaming",
                WebAssemblyInstantiateStreaming, 1);
  }

  // Expose the API on the global object if configured to do so.
  if (exposed_on_global_object) {
    JSObject::AddProperty(global, name, webassembly, attributes);
  }

  // Setup Module
  Handle<JSFunction> module_constructor =
      InstallFunc(isolate, webassembly, "Module", WebAssemblyModule, 1);
  context->set_wasm_module_constructor(*module_constructor);
  JSFunction::EnsureHasInitialMap(module_constructor);
  Handle<JSObject> module_proto(
      JSObject::cast(module_constructor->instance_prototype()));
  i::Handle<i::Map> module_map =
      isolate->factory()->NewMap(i::WASM_MODULE_TYPE, WasmModuleObject::kSize);
  JSFunction::SetInitialMap(module_constructor, module_map, module_proto);
  InstallFunc(isolate, module_constructor, "imports", WebAssemblyModuleImports,
              1);
  InstallFunc(isolate, module_constructor, "exports", WebAssemblyModuleExports,
              1);
  InstallFunc(isolate, module_constructor, "customSections",
              WebAssemblyModuleCustomSections, 2);
  JSObject::AddProperty(module_proto, factory->to_string_tag_symbol(),
                        v8_str(isolate, "WebAssembly.Module"), ro_attributes);

  // Setup Instance
  Handle<JSFunction> instance_constructor =
      InstallFunc(isolate, webassembly, "Instance", WebAssemblyInstance, 1);
  context->set_wasm_instance_constructor(*instance_constructor);
  JSFunction::EnsureHasInitialMap(instance_constructor);
  Handle<JSObject> instance_proto(
      JSObject::cast(instance_constructor->instance_prototype()));
  i::Handle<i::Map> instance_map = isolate->factory()->NewMap(
      i::WASM_INSTANCE_TYPE, WasmInstanceObject::kSize);
  JSFunction::SetInitialMap(instance_constructor, instance_map, instance_proto);
  InstallGetter(isolate, instance_proto, "exports",
                WebAssemblyInstanceGetExports);
  JSObject::AddProperty(instance_proto, factory->to_string_tag_symbol(),
                        v8_str(isolate, "WebAssembly.Instance"), ro_attributes);

  // Setup Table
  Handle<JSFunction> table_constructor =
      InstallFunc(isolate, webassembly, "Table", WebAssemblyTable, 1);
  context->set_wasm_table_constructor(*table_constructor);
  JSFunction::EnsureHasInitialMap(table_constructor);
  Handle<JSObject> table_proto(
      JSObject::cast(table_constructor->instance_prototype()));
  i::Handle<i::Map> table_map =
      isolate->factory()->NewMap(i::WASM_TABLE_TYPE, WasmTableObject::kSize);
  JSFunction::SetInitialMap(table_constructor, table_map, table_proto);
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
  JSFunction::EnsureHasInitialMap(memory_constructor);
  Handle<JSObject> memory_proto(
      JSObject::cast(memory_constructor->instance_prototype()));
  i::Handle<i::Map> memory_map =
      isolate->factory()->NewMap(i::WASM_MEMORY_TYPE, WasmMemoryObject::kSize);
  JSFunction::SetInitialMap(memory_constructor, memory_map, memory_proto);
  InstallFunc(isolate, memory_proto, "grow", WebAssemblyMemoryGrow, 1);
  InstallGetter(isolate, memory_proto, "buffer", WebAssemblyMemoryGetBuffer);
  JSObject::AddProperty(memory_proto, factory->to_string_tag_symbol(),
                        v8_str(isolate, "WebAssembly.Memory"), ro_attributes);

  // Setup Global
  if (i::FLAG_experimental_wasm_mut_global) {
    Handle<JSFunction> global_constructor =
        InstallFunc(isolate, webassembly, "Global", WebAssemblyGlobal, 1);
    context->set_wasm_global_constructor(*global_constructor);
    JSFunction::EnsureHasInitialMap(global_constructor);
    Handle<JSObject> global_proto(
        JSObject::cast(global_constructor->instance_prototype()));
    i::Handle<i::Map> global_map = isolate->factory()->NewMap(
        i::WASM_GLOBAL_TYPE, WasmGlobalObject::kSize);
    JSFunction::SetInitialMap(global_constructor, global_map, global_proto);
    InstallFunc(isolate, global_proto, "valueOf", WebAssemblyGlobalValueOf, 0);
    InstallGetterSetter(isolate, global_proto, "value",
                        WebAssemblyGlobalGetValue, WebAssemblyGlobalSetValue);
    JSObject::AddProperty(global_proto, factory->to_string_tag_symbol(),
                          v8_str(isolate, "WebAssembly.Global"), ro_attributes);
  }

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

#undef ASSIGN
#undef EXTRACT_THIS

}  // namespace internal
}  // namespace v8

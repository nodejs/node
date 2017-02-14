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
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-result.h"

typedef uint8_t byte;

using v8::internal::wasm::ErrorThrower;

namespace v8 {

enum WasmMemoryObjectData {
  kWasmMemoryBuffer,
  kWasmMemoryMaximum,
  kWasmMemoryInstanceObject
};

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
      thrower->CompileError("ArrayBuffer argument is empty");
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
      thrower->TypeError("ArrayBuffer argument is empty");
    }
  } else {
    thrower->TypeError("Argument 0 must be an ArrayBuffer or Uint8Array");
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
      i::Handle<i::Script>::null(), nullptr, nullptr);
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
  i::Handle<i::JSObject> i_obj =
      i::Handle<i::JSObject>::cast(v8::Utils::OpenHandle(*obj));

  i::Handle<i::JSReceiver> ffi = i::Handle<i::JSObject>::null();
  if (args.Length() > 1 && args[1]->IsObject()) {
    Local<Object> obj = Local<Object>::Cast(args[1]);
    ffi = i::Handle<i::JSReceiver>::cast(v8::Utils::OpenHandle(*obj));
  }

  i::Handle<i::JSArrayBuffer> memory = i::Handle<i::JSArrayBuffer>::null();
  if (args.Length() > 2 && args[2]->IsObject()) {
    Local<Object> obj = Local<Object>::Cast(args[2]);
    i::Handle<i::Object> mem_obj = v8::Utils::OpenHandle(*obj);
    if (i::WasmJs::IsWasmMemoryObject(i_isolate, mem_obj)) {
      memory = i::Handle<i::JSArrayBuffer>(
          i::Handle<i::WasmMemoryObject>::cast(mem_obj)->get_buffer(),
          i_isolate);
    } else {
      thrower.TypeError("Argument 2 must be a WebAssembly.Memory");
    }
  }
  i::MaybeHandle<i::JSObject> instance =
      i::wasm::WasmModule::Instantiate(i_isolate, &thrower, i_obj, ffi, memory);
  if (instance.is_null()) {
    if (!thrower.error()) thrower.RuntimeError("Could not instantiate module");
    return;
  }
  DCHECK(!i_isolate->has_pending_exception());
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

const int max_table_size = 1 << 26;

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
  } else {
    maximum = static_cast<int>(i::wasm::WasmModule::kV8MaxTableSize);
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
                       "WebAssembly.Module()");
  if (args.Length() < 1 || !args[0]->IsObject()) {
    thrower.TypeError("Argument 0 must be a memory descriptor");
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

  i::Handle<i::JSObject> memory_obj = i::WasmMemoryObject::New(
      i_isolate, buffer, has_maximum.FromJust() ? maximum : -1);
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
  i::Handle<i::FixedArray> old_array(receiver->get_functions(), i_isolate);
  int old_size = old_array->length();
  int64_t new_size64 = 0;
  if (args.Length() > 0 && !args[0]->IntegerValue(context).To(&new_size64)) {
    return;
  }
  new_size64 += old_size;

  if (new_size64 < old_size || new_size64 > receiver->maximum_length()) {
    v8::Local<v8::Value> e = v8::Exception::RangeError(
        v8_str(isolate, new_size64 < old_size ? "trying to shrink table"
                                              : "maximum table size exceeded"));
    isolate->ThrowException(e);
    return;
  }
  int new_size = static_cast<int>(new_size64);

  if (new_size != old_size) {
    i::Handle<i::FixedArray> new_array =
        i_isolate->factory()->NewFixedArray(new_size);
    for (int i = 0; i < old_size; ++i) new_array->set(i, old_array->get(i));
    i::Object* null = i_isolate->heap()->null_value();
    for (int i = old_size; i < new_size; ++i) new_array->set(i, null);
    receiver->set_functions(*new_array);
  }

  // TODO(titzer): update relevant instances.
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
  i::Handle<i::FixedArray> array(receiver->get_functions(), i_isolate);
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
  i::Handle<i::FixedArray> array(receiver->get_functions(), i_isolate);
  int i;
  if (!args[0]->Int32Value(context).To(&i)) return;
  if (i < 0 || i >= array->length()) {
    v8::Local<v8::Value> e =
        v8::Exception::RangeError(v8_str(isolate, "index out of bounds"));
    isolate->ThrowException(e);
    return;
  }

  i::Handle<i::FixedArray> dispatch_tables(receiver->get_dispatch_tables(),
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
  if (args.Length() < 1) {
    v8::Local<v8::Value> e = v8::Exception::TypeError(
        v8_str(isolate, "Argument 0 required, must be numeric value of pages"));
    isolate->ThrowException(e);
    return;
  }

  uint32_t delta = args[0]->Uint32Value(context).FromJust();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  i::Handle<i::JSObject> receiver =
      i::Handle<i::JSObject>::cast(Utils::OpenHandle(*args.This()));
  i::Handle<i::Object> instance_object(
      receiver->GetInternalField(kWasmMemoryInstanceObject), i_isolate);
  i::Handle<i::JSObject> instance(
      i::Handle<i::JSObject>::cast(instance_object));

  // TODO(gdeepti) Implement growing memory when shared by different
  // instances.
  int32_t ret = internal::wasm::GrowInstanceMemory(i_isolate, instance, delta);
  if (ret == -1) {
    v8::Local<v8::Value> e = v8::Exception::Error(
        v8_str(isolate, "Unable to grow instance memory."));
    isolate->ThrowException(e);
    return;
  }
  i::MaybeHandle<i::JSArrayBuffer> buffer =
      internal::wasm::GetInstanceMemory(i_isolate, instance);
  if (buffer.is_null()) {
    v8::Local<v8::Value> e = v8::Exception::Error(
        v8_str(isolate, "WebAssembly.Memory buffer object not set."));
    isolate->ThrowException(e);
    return;
  }
  receiver->SetInternalField(kWasmMemoryBuffer, *buffer.ToHandleChecked());
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
  i::Handle<i::JSObject> receiver =
      i::Handle<i::JSObject>::cast(Utils::OpenHandle(*args.This()));
  i::Handle<i::Object> buffer(receiver->GetInternalField(kWasmMemoryBuffer),
                              i_isolate);
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
  Handle<JSObject> webassembly = factory->NewJSObject(cons, TENURED);
  PropertyAttributes attributes = static_cast<PropertyAttributes>(DONT_ENUM);
  JSObject::AddProperty(global, name, webassembly, attributes);

  // Setup compile
  InstallFunc(isolate, webassembly, "compile", WebAssemblyCompile);

  // Setup compile
  InstallFunc(isolate, webassembly, "validate", WebAssemblyValidate);

  // Setup Module
  Handle<JSFunction> module_constructor =
      InstallFunc(isolate, webassembly, "Module", WebAssemblyModule);
  context->set_wasm_module_constructor(*module_constructor);
  Handle<JSObject> module_proto =
      factory->NewJSObject(module_constructor, TENURED);
  i::Handle<i::Map> map = isolate->factory()->NewMap(
      i::JS_OBJECT_TYPE, i::JSObject::kHeaderSize +
                             WasmModuleObject::kFieldCount * i::kPointerSize);
  JSFunction::SetInitialMap(module_constructor, map, module_proto);
  JSObject::AddProperty(module_proto, isolate->factory()->constructor_string(),
                        module_constructor, DONT_ENUM);

  // Setup Instance
  Handle<JSFunction> instance_constructor =
      InstallFunc(isolate, webassembly, "Instance", WebAssemblyInstance);
  context->set_wasm_instance_constructor(*instance_constructor);

  // Setup Table
  Handle<JSFunction> table_constructor =
      InstallFunc(isolate, webassembly, "Table", WebAssemblyTable);
  context->set_wasm_table_constructor(*table_constructor);
  Handle<JSObject> table_proto =
      factory->NewJSObject(table_constructor, TENURED);
  map = isolate->factory()->NewMap(
      i::JS_OBJECT_TYPE, i::JSObject::kHeaderSize +
                             WasmTableObject::kFieldCount * i::kPointerSize);
  JSFunction::SetInitialMap(table_constructor, map, table_proto);
  JSObject::AddProperty(table_proto, isolate->factory()->constructor_string(),
                        table_constructor, DONT_ENUM);
  InstallGetter(isolate, table_proto, "length", WebAssemblyTableGetLength);
  InstallFunc(isolate, table_proto, "grow", WebAssemblyTableGrow);
  InstallFunc(isolate, table_proto, "get", WebAssemblyTableGet);
  InstallFunc(isolate, table_proto, "set", WebAssemblyTableSet);

  // Setup Memory
  Handle<JSFunction> memory_constructor =
      InstallFunc(isolate, webassembly, "Memory", WebAssemblyMemory);
  context->set_wasm_memory_constructor(*memory_constructor);
  Handle<JSObject> memory_proto =
      factory->NewJSObject(memory_constructor, TENURED);
  map = isolate->factory()->NewMap(
      i::JS_OBJECT_TYPE, i::JSObject::kHeaderSize +
                             WasmMemoryObject::kFieldCount * i::kPointerSize);
  JSFunction::SetInitialMap(memory_constructor, map, memory_proto);
  JSObject::AddProperty(memory_proto, isolate->factory()->constructor_string(),
                        memory_constructor, DONT_ENUM);
  InstallFunc(isolate, memory_proto, "grow", WebAssemblyMemoryGrow);
  InstallGetter(isolate, memory_proto, "buffer", WebAssemblyMemoryGetBuffer);

  // Setup errors
  attributes = static_cast<PropertyAttributes>(DONT_DELETE | READ_ONLY);
  Handle<JSFunction> compile_error(
      isolate->native_context()->wasm_compile_error_function());
  JSObject::AddProperty(webassembly, isolate->factory()->CompileError_string(),
                        compile_error, attributes);
  Handle<JSFunction> runtime_error(
      isolate->native_context()->wasm_runtime_error_function());
  JSObject::AddProperty(webassembly, isolate->factory()->RuntimeError_string(),
                        runtime_error, attributes);
}

void WasmJs::Install(Isolate* isolate, Handle<JSGlobalObject> global) {
  if (!FLAG_expose_wasm && !FLAG_validate_asm) {
    return;
  }

  // Setup wasm function map.
  Handle<Context> context(global->native_context(), isolate);
  InstallWasmMapsIfNeeded(isolate, context);

  if (FLAG_expose_wasm) {
    InstallWasmConstructors(isolate, global, context);
  }
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

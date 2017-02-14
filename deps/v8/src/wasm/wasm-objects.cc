// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-module.h"

#define TRACE(...)                                      \
  do {                                                  \
    if (FLAG_trace_wasm_instances) PrintF(__VA_ARGS__); \
  } while (false)

#define TRACE_CHAIN(instance)        \
  do {                               \
    instance->PrintInstancesChain(); \
  } while (false)

using namespace v8::internal;
using namespace v8::internal::wasm;

#define DEFINE_ACCESSORS(Container, name, field, type) \
  type* Container::get_##name() {                      \
    return type::cast(GetInternalField(field));        \
  }                                                    \
  void Container::set_##name(type* value) {            \
    return SetInternalField(field, value);             \
  }

#define DEFINE_OPTIONAL_ACCESSORS(Container, name, field, type) \
  bool Container::has_##name() {                                \
    return !GetInternalField(field)->IsUndefined(GetIsolate()); \
  }                                                             \
  type* Container::get_##name() {                               \
    return type::cast(GetInternalField(field));                 \
  }                                                             \
  void Container::set_##name(type* value) {                     \
    return SetInternalField(field, value);                      \
  }

#define DEFINE_GETTER(Container, name, field, type) \
  type* Container::get_##name() { return type::cast(GetInternalField(field)); }

static uint32_t SafeUint32(Object* value) {
  if (value->IsSmi()) {
    int32_t val = Smi::cast(value)->value();
    CHECK_GE(val, 0);
    return static_cast<uint32_t>(val);
  }
  DCHECK(value->IsHeapNumber());
  HeapNumber* num = HeapNumber::cast(value);
  CHECK_GE(num->value(), 0.0);
  CHECK_LE(num->value(), static_cast<double>(kMaxUInt32));
  return static_cast<uint32_t>(num->value());
}

static int32_t SafeInt32(Object* value) {
  if (value->IsSmi()) {
    return Smi::cast(value)->value();
  }
  DCHECK(value->IsHeapNumber());
  HeapNumber* num = HeapNumber::cast(value);
  CHECK_GE(num->value(), static_cast<double>(Smi::kMinValue));
  CHECK_LE(num->value(), static_cast<double>(Smi::kMaxValue));
  return static_cast<int32_t>(num->value());
}

Handle<WasmModuleObject> WasmModuleObject::New(
    Isolate* isolate, Handle<WasmCompiledModule> compiled_module) {
  ModuleOrigin origin = compiled_module->module()->origin;

  Handle<JSObject> module_object;
  if (origin == ModuleOrigin::kWasmOrigin) {
    Handle<JSFunction> module_cons(
        isolate->native_context()->wasm_module_constructor());
    module_object = isolate->factory()->NewJSObject(module_cons);
    Handle<Symbol> module_sym(isolate->native_context()->wasm_module_sym());
    Object::SetProperty(module_object, module_sym, module_object, STRICT)
        .Check();
  } else {
    DCHECK(origin == ModuleOrigin::kAsmJsOrigin);
    Handle<Map> map = isolate->factory()->NewMap(
        JS_OBJECT_TYPE,
        JSObject::kHeaderSize + WasmModuleObject::kFieldCount * kPointerSize);
    module_object = isolate->factory()->NewJSObjectFromMap(map, TENURED);
  }
  module_object->SetInternalField(WasmModuleObject::kCompiledModule,
                                  *compiled_module);
  Handle<WeakCell> link_to_module =
      isolate->factory()->NewWeakCell(module_object);
  compiled_module->set_weak_wasm_module(link_to_module);
  return Handle<WasmModuleObject>::cast(module_object);
}

WasmModuleObject* WasmModuleObject::cast(Object* object) {
  DCHECK(object->IsJSObject());
  // TODO(titzer): brand check for WasmModuleObject.
  return reinterpret_cast<WasmModuleObject*>(object);
}

Handle<WasmTableObject> WasmTableObject::New(Isolate* isolate, uint32_t initial,
                                             uint32_t maximum,
                                             Handle<FixedArray>* js_functions) {
  Handle<JSFunction> table_ctor(
      isolate->native_context()->wasm_table_constructor());
  Handle<JSObject> table_obj = isolate->factory()->NewJSObject(table_ctor);
  *js_functions = isolate->factory()->NewFixedArray(initial);
  Object* null = isolate->heap()->null_value();
  for (int i = 0; i < static_cast<int>(initial); ++i) {
    (*js_functions)->set(i, null);
  }
  table_obj->SetInternalField(kFunctions, *(*js_functions));
  table_obj->SetInternalField(kMaximum,
                              static_cast<Object*>(Smi::FromInt(maximum)));

  Handle<FixedArray> dispatch_tables = isolate->factory()->NewFixedArray(0);
  table_obj->SetInternalField(kDispatchTables, *dispatch_tables);
  Handle<Symbol> table_sym(isolate->native_context()->wasm_table_sym());
  Object::SetProperty(table_obj, table_sym, table_obj, STRICT).Check();
  return Handle<WasmTableObject>::cast(table_obj);
}

DEFINE_GETTER(WasmTableObject, dispatch_tables, kDispatchTables, FixedArray)

Handle<FixedArray> WasmTableObject::AddDispatchTable(
    Isolate* isolate, Handle<WasmTableObject> table_obj,
    Handle<WasmInstanceObject> instance, int table_index,
    Handle<FixedArray> dispatch_table) {
  Handle<FixedArray> dispatch_tables(
      FixedArray::cast(table_obj->GetInternalField(kDispatchTables)), isolate);
  DCHECK_EQ(0, dispatch_tables->length() % 3);

  if (instance.is_null()) return dispatch_tables;
  // TODO(titzer): use weak cells here to avoid leaking instances.

  // Grow the dispatch table and add a new triple at the end.
  Handle<FixedArray> new_dispatch_tables =
      isolate->factory()->CopyFixedArrayAndGrow(dispatch_tables, 3);

  new_dispatch_tables->set(dispatch_tables->length() + 0, *instance);
  new_dispatch_tables->set(dispatch_tables->length() + 1,
                           Smi::FromInt(table_index));
  new_dispatch_tables->set(dispatch_tables->length() + 2, *dispatch_table);

  table_obj->SetInternalField(WasmTableObject::kDispatchTables,
                              *new_dispatch_tables);

  return new_dispatch_tables;
}

DEFINE_ACCESSORS(WasmTableObject, functions, kFunctions, FixedArray)

uint32_t WasmTableObject::current_length() { return get_functions()->length(); }

uint32_t WasmTableObject::maximum_length() {
  return SafeUint32(GetInternalField(kMaximum));
}

WasmTableObject* WasmTableObject::cast(Object* object) {
  DCHECK(object && object->IsJSObject());
  // TODO(titzer): brand check for WasmTableObject.
  return reinterpret_cast<WasmTableObject*>(object);
}

Handle<WasmMemoryObject> WasmMemoryObject::New(Isolate* isolate,
                                               Handle<JSArrayBuffer> buffer,
                                               int maximum) {
  Handle<JSFunction> memory_ctor(
      isolate->native_context()->wasm_memory_constructor());
  Handle<JSObject> memory_obj = isolate->factory()->NewJSObject(memory_ctor);
  memory_obj->SetInternalField(kArrayBuffer, *buffer);
  memory_obj->SetInternalField(kMaximum,
                               static_cast<Object*>(Smi::FromInt(maximum)));
  Handle<Symbol> memory_sym(isolate->native_context()->wasm_memory_sym());
  Object::SetProperty(memory_obj, memory_sym, memory_obj, STRICT).Check();
  return Handle<WasmMemoryObject>::cast(memory_obj);
}

DEFINE_ACCESSORS(WasmMemoryObject, buffer, kArrayBuffer, JSArrayBuffer)

uint32_t WasmMemoryObject::current_pages() {
  return SafeUint32(get_buffer()->byte_length()) / wasm::WasmModule::kPageSize;
}

int32_t WasmMemoryObject::maximum_pages() {
  return SafeInt32(GetInternalField(kMaximum));
}

WasmMemoryObject* WasmMemoryObject::cast(Object* object) {
  DCHECK(object && object->IsJSObject());
  // TODO(titzer): brand check for WasmMemoryObject.
  return reinterpret_cast<WasmMemoryObject*>(object);
}

void WasmMemoryObject::AddInstance(WasmInstanceObject* instance) {
  // TODO(gdeepti): This should be a weak list of instance objects
  // for instances that share memory.
  SetInternalField(kInstance, instance);
}

DEFINE_ACCESSORS(WasmInstanceObject, compiled_module, kCompiledModule,
                 WasmCompiledModule)
DEFINE_OPTIONAL_ACCESSORS(WasmInstanceObject, globals_buffer,
                          kGlobalsArrayBuffer, JSArrayBuffer)
DEFINE_OPTIONAL_ACCESSORS(WasmInstanceObject, memory_buffer, kMemoryArrayBuffer,
                          JSArrayBuffer)
DEFINE_OPTIONAL_ACCESSORS(WasmInstanceObject, memory_object, kMemoryObject,
                          WasmMemoryObject)
DEFINE_OPTIONAL_ACCESSORS(WasmInstanceObject, debug_info, kDebugInfo,
                          WasmDebugInfo)

WasmModuleObject* WasmInstanceObject::module_object() {
  return WasmModuleObject::cast(*get_compiled_module()->wasm_module());
}

WasmModule* WasmInstanceObject::module() {
  return reinterpret_cast<WasmModuleWrapper*>(
             *get_compiled_module()->module_wrapper())
      ->get();
}

WasmInstanceObject* WasmInstanceObject::cast(Object* object) {
  DCHECK(IsWasmInstanceObject(object));
  return reinterpret_cast<WasmInstanceObject*>(object);
}

bool WasmInstanceObject::IsWasmInstanceObject(Object* object) {
  if (!object->IsObject()) return false;
  if (!object->IsJSObject()) return false;

  JSObject* obj = JSObject::cast(object);
  Isolate* isolate = obj->GetIsolate();
  if (obj->GetInternalFieldCount() != kFieldCount) {
    return false;
  }

  Object* mem = obj->GetInternalField(kMemoryArrayBuffer);
  if (!(mem->IsUndefined(isolate) || mem->IsJSArrayBuffer()) ||
      !WasmCompiledModule::IsWasmCompiledModule(
          obj->GetInternalField(kCompiledModule))) {
    return false;
  }

  // All checks passed.
  return true;
}

Handle<WasmInstanceObject> WasmInstanceObject::New(
    Isolate* isolate, Handle<WasmCompiledModule> compiled_module) {
  Handle<Map> map = isolate->factory()->NewMap(
      JS_OBJECT_TYPE, JSObject::kHeaderSize + kFieldCount * kPointerSize);
  Handle<WasmInstanceObject> instance(
      reinterpret_cast<WasmInstanceObject*>(
          *isolate->factory()->NewJSObjectFromMap(map, TENURED)),
      isolate);

  instance->SetInternalField(kCompiledModule, *compiled_module);
  instance->SetInternalField(kMemoryObject, isolate->heap()->undefined_value());
  return instance;
}

WasmInstanceObject* WasmExportedFunction::instance() {
  return WasmInstanceObject::cast(GetInternalField(kInstance));
}

int WasmExportedFunction::function_index() {
  return SafeInt32(GetInternalField(kIndex));
}

WasmExportedFunction* WasmExportedFunction::cast(Object* object) {
  DCHECK(object && object->IsJSFunction());
  DCHECK_EQ(Code::JS_TO_WASM_FUNCTION,
            JSFunction::cast(object)->code()->kind());
  // TODO(titzer): brand check for WasmExportedFunction.
  return reinterpret_cast<WasmExportedFunction*>(object);
}

Handle<WasmExportedFunction> WasmExportedFunction::New(
    Isolate* isolate, Handle<WasmInstanceObject> instance, Handle<String> name,
    Handle<Code> export_wrapper, int arity, int func_index) {
  DCHECK_EQ(Code::JS_TO_WASM_FUNCTION, export_wrapper->kind());
  Handle<SharedFunctionInfo> shared =
      isolate->factory()->NewSharedFunctionInfo(name, export_wrapper, false);
  shared->set_length(arity);
  shared->set_internal_formal_parameter_count(arity);
  Handle<JSFunction> function = isolate->factory()->NewFunction(
      isolate->wasm_function_map(), name, export_wrapper);
  function->set_shared(*shared);

  function->SetInternalField(kInstance, *instance);
  function->SetInternalField(kIndex, Smi::FromInt(func_index));
  return Handle<WasmExportedFunction>::cast(function);
}

Handle<WasmCompiledModule> WasmCompiledModule::New(
    Isolate* isolate, Handle<WasmModuleWrapper> module_wrapper) {
  Handle<FixedArray> ret =
      isolate->factory()->NewFixedArray(PropertyIndices::Count, TENURED);
  // WasmCompiledModule::cast would fail since module bytes are not set yet.
  Handle<WasmCompiledModule> compiled_module(
      reinterpret_cast<WasmCompiledModule*>(*ret), isolate);
  compiled_module->InitId();
  compiled_module->set_module_wrapper(module_wrapper);
  return compiled_module;
}

wasm::WasmModule* WasmCompiledModule::module() const {
  return reinterpret_cast<WasmModuleWrapper*>(*module_wrapper())->get();
}

void WasmCompiledModule::InitId() {
#if DEBUG
  static uint32_t instance_id_counter = 0;
  set(kID_instance_id, Smi::FromInt(instance_id_counter++));
  TRACE("New compiled module id: %d\n", instance_id());
#endif
}

bool WasmCompiledModule::IsWasmCompiledModule(Object* obj) {
  if (!obj->IsFixedArray()) return false;
  FixedArray* arr = FixedArray::cast(obj);
  if (arr->length() != PropertyIndices::Count) return false;
  Isolate* isolate = arr->GetIsolate();
#define WCM_CHECK_SMALL_NUMBER(TYPE, NAME) \
  if (!arr->get(kID_##NAME)->IsSmi()) return false;
#define WCM_CHECK_OBJECT_OR_WEAK(TYPE, NAME)         \
  if (!arr->get(kID_##NAME)->IsUndefined(isolate) && \
      !arr->get(kID_##NAME)->Is##TYPE())             \
    return false;
#define WCM_CHECK_OBJECT(TYPE, NAME) WCM_CHECK_OBJECT_OR_WEAK(TYPE, NAME)
#define WCM_CHECK_WEAK_LINK(TYPE, NAME) WCM_CHECK_OBJECT_OR_WEAK(WeakCell, NAME)
#define WCM_CHECK(KIND, TYPE, NAME) WCM_CHECK_##KIND(TYPE, NAME)
  WCM_PROPERTY_TABLE(WCM_CHECK)
#undef WCM_CHECK

  // All checks passed.
  return true;
}

void WasmCompiledModule::PrintInstancesChain() {
#if DEBUG
  if (!FLAG_trace_wasm_instances) return;
  for (WasmCompiledModule* current = this; current != nullptr;) {
    PrintF("->%d", current->instance_id());
    if (current->ptr_to_weak_next_instance() == nullptr) break;
    CHECK(!current->ptr_to_weak_next_instance()->cleared());
    current =
        WasmCompiledModule::cast(current->ptr_to_weak_next_instance()->value());
  }
  PrintF("\n");
#endif
}

uint32_t WasmCompiledModule::mem_size() const {
  return has_memory() ? memory()->byte_length()->Number() : default_mem_size();
}

uint32_t WasmCompiledModule::default_mem_size() const {
  return min_mem_pages() * WasmModule::kPageSize;
}

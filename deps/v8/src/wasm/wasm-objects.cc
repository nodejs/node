// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-objects.h"
#include "src/utils.h"

#include "src/assembler-inl.h"
#include "src/base/iterator.h"
#include "src/compiler/wasm-compiler.h"
#include "src/debug/debug-interface.h"
#include "src/objects-inl.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-code-specialization.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-text.h"

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

#define DEFINE_GETTER0(getter, Container, name, field, type) \
  type* Container::name() { return type::cast(getter(field)); }

#define DEFINE_ACCESSORS0(getter, setter, Container, name, field, type) \
  DEFINE_GETTER0(getter, Container, name, field, type)                  \
  void Container::set_##name(type* value) { return setter(field, value); }

#define DEFINE_OPTIONAL_ACCESSORS0(getter, setter, Container, name, field, \
                                   type)                                   \
  DEFINE_ACCESSORS0(getter, setter, Container, name, field, type)          \
  bool Container::has_##name() {                                           \
    return !getter(field)->IsUndefined(GetIsolate());                      \
  }

#define DEFINE_OPTIONAL_GETTER0(getter, Container, name, field, type) \
  DEFINE_GETTER0(getter, Container, name, field, type)                \
  bool Container::has_##name() {                                      \
    return !getter(field)->IsUndefined(GetIsolate());                 \
  }

#define DEFINE_GETTER0(getter, Container, name, field, type) \
  type* Container::name() { return type::cast(getter(field)); }

#define DEFINE_OBJ_GETTER(Container, name, field, type) \
  DEFINE_GETTER0(GetEmbedderField, Container, name, field, type)
#define DEFINE_OBJ_ACCESSORS(Container, name, field, type)               \
  DEFINE_ACCESSORS0(GetEmbedderField, SetEmbedderField, Container, name, \
                    field, type)
#define DEFINE_OPTIONAL_OBJ_ACCESSORS(Container, name, field, type)         \
  DEFINE_OPTIONAL_ACCESSORS0(GetEmbedderField, SetEmbedderField, Container, \
                             name, field, type)
#define DEFINE_ARR_GETTER(Container, name, field, type) \
  DEFINE_GETTER0(get, Container, name, field, type)
#define DEFINE_ARR_ACCESSORS(Container, name, field, type) \
  DEFINE_ACCESSORS0(get, set, Container, name, field, type)
#define DEFINE_OPTIONAL_ARR_ACCESSORS(Container, name, field, type) \
  DEFINE_OPTIONAL_ACCESSORS0(get, set, Container, name, field, type)
#define DEFINE_OPTIONAL_ARR_GETTER(Container, name, field, type) \
  DEFINE_OPTIONAL_GETTER0(get, Container, name, field, type)

namespace {

// An iterator that returns first the module itself, then all modules linked via
// next, then all linked via prev.
class CompiledModulesIterator
    : public std::iterator<std::input_iterator_tag,
                           Handle<WasmCompiledModule>> {
 public:
  CompiledModulesIterator(Isolate* isolate,
                          Handle<WasmCompiledModule> start_module, bool at_end)
      : isolate_(isolate),
        start_module_(start_module),
        current_(at_end ? Handle<WasmCompiledModule>::null() : start_module) {}

  Handle<WasmCompiledModule> operator*() const {
    DCHECK(!current_.is_null());
    return current_;
  }

  void operator++() { Advance(); }

  bool operator!=(const CompiledModulesIterator& other) {
    DCHECK(start_module_.is_identical_to(other.start_module_));
    return !current_.is_identical_to(other.current_);
  }

 private:
  void Advance() {
    DCHECK(!current_.is_null());
    if (!is_backwards_) {
      if (current_->has_weak_next_instance()) {
        WeakCell* weak_next = current_->ptr_to_weak_next_instance();
        if (!weak_next->cleared()) {
          current_ =
              handle(WasmCompiledModule::cast(weak_next->value()), isolate_);
          return;
        }
      }
      // No more modules in next-links, now try the previous-links.
      is_backwards_ = true;
      current_ = start_module_;
    }
    if (current_->has_weak_prev_instance()) {
      WeakCell* weak_prev = current_->ptr_to_weak_prev_instance();
      if (!weak_prev->cleared()) {
        current_ =
            handle(WasmCompiledModule::cast(weak_prev->value()), isolate_);
        return;
      }
    }
    current_ = Handle<WasmCompiledModule>::null();
  }

  friend class CompiledModuleInstancesIterator;
  Isolate* isolate_;
  Handle<WasmCompiledModule> start_module_;
  Handle<WasmCompiledModule> current_;
  bool is_backwards_ = false;
};

// An iterator based on the CompiledModulesIterator, but it returns all live
// instances, not the WasmCompiledModules itself.
class CompiledModuleInstancesIterator
    : public std::iterator<std::input_iterator_tag,
                           Handle<WasmInstanceObject>> {
 public:
  CompiledModuleInstancesIterator(Isolate* isolate,
                                  Handle<WasmCompiledModule> start_module,
                                  bool at_end)
      : it(isolate, start_module, at_end) {
    while (NeedToAdvance()) ++it;
  }

  Handle<WasmInstanceObject> operator*() {
    return handle(
        WasmInstanceObject::cast((*it)->weak_owning_instance()->value()),
        it.isolate_);
  }

  void operator++() {
    do {
      ++it;
    } while (NeedToAdvance());
  }

  bool operator!=(const CompiledModuleInstancesIterator& other) {
    return it != other.it;
  }

 private:
  bool NeedToAdvance() {
    return !it.current_.is_null() &&
           (!it.current_->has_weak_owning_instance() ||
            it.current_->ptr_to_weak_owning_instance()->cleared());
  }
  CompiledModulesIterator it;
};

v8::base::iterator_range<CompiledModuleInstancesIterator>
iterate_compiled_module_instance_chain(
    Isolate* isolate, Handle<WasmCompiledModule> compiled_module) {
  return {CompiledModuleInstancesIterator(isolate, compiled_module, false),
          CompiledModuleInstancesIterator(isolate, compiled_module, true)};
}

#ifdef DEBUG
bool IsBreakablePosition(Handle<WasmCompiledModule> compiled_module,
                         int func_index, int offset_in_func) {
  DisallowHeapAllocation no_gc;
  AccountingAllocator alloc;
  Zone tmp(&alloc, ZONE_NAME);
  BodyLocalDecls locals(&tmp);
  const byte* module_start = compiled_module->module_bytes()->GetChars();
  WasmFunction& func = compiled_module->module()->functions[func_index];
  BytecodeIterator iterator(module_start + func.code_start_offset,
                            module_start + func.code_end_offset, &locals);
  DCHECK_LT(0, locals.encoded_size);
  for (uint32_t offset : iterator.offsets()) {
    if (offset > static_cast<uint32_t>(offset_in_func)) break;
    if (offset == static_cast<uint32_t>(offset_in_func)) return true;
  }
  return false;
}
#endif  // DEBUG

}  // namespace

Handle<WasmModuleObject> WasmModuleObject::New(
    Isolate* isolate, Handle<WasmCompiledModule> compiled_module) {
  WasmModule* module = compiled_module->module();
  Handle<JSObject> module_object;
  if (module->is_wasm()) {
    Handle<JSFunction> module_cons(
        isolate->native_context()->wasm_module_constructor());
    module_object = isolate->factory()->NewJSObject(module_cons);
    Handle<Symbol> module_sym(isolate->native_context()->wasm_module_sym());
    Object::SetProperty(module_object, module_sym, module_object, STRICT)
        .Check();
  } else {
    DCHECK(module->is_asm_js());
    Handle<Map> map = isolate->factory()->NewMap(
        JS_OBJECT_TYPE,
        JSObject::kHeaderSize + WasmModuleObject::kFieldCount * kPointerSize);
    module_object = isolate->factory()->NewJSObjectFromMap(map, TENURED);
  }
  module_object->SetEmbedderField(WasmModuleObject::kCompiledModule,
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

bool WasmModuleObject::IsWasmModuleObject(Object* object) {
  return object->IsJSObject() &&
         JSObject::cast(object)->GetEmbedderFieldCount() == kFieldCount;
}

DEFINE_OBJ_GETTER(WasmModuleObject, compiled_module, kCompiledModule,
                  WasmCompiledModule)

Handle<WasmTableObject> WasmTableObject::New(Isolate* isolate, uint32_t initial,
                                             int64_t maximum,
                                             Handle<FixedArray>* js_functions) {
  Handle<JSFunction> table_ctor(
      isolate->native_context()->wasm_table_constructor());
  Handle<JSObject> table_obj = isolate->factory()->NewJSObject(table_ctor);
  table_obj->SetEmbedderField(kWrapperTracerHeader, Smi::kZero);

  *js_functions = isolate->factory()->NewFixedArray(initial);
  Object* null = isolate->heap()->null_value();
  for (int i = 0; i < static_cast<int>(initial); ++i) {
    (*js_functions)->set(i, null);
  }
  table_obj->SetEmbedderField(kFunctions, *(*js_functions));
  Handle<Object> max = isolate->factory()->NewNumber(maximum);
  table_obj->SetEmbedderField(kMaximum, *max);

  Handle<FixedArray> dispatch_tables = isolate->factory()->NewFixedArray(0);
  table_obj->SetEmbedderField(kDispatchTables, *dispatch_tables);
  Handle<Symbol> table_sym(isolate->native_context()->wasm_table_sym());
  Object::SetProperty(table_obj, table_sym, table_obj, STRICT).Check();
  return Handle<WasmTableObject>::cast(table_obj);
}

Handle<FixedArray> WasmTableObject::AddDispatchTable(
    Isolate* isolate, Handle<WasmTableObject> table_obj,
    Handle<WasmInstanceObject> instance, int table_index,
    Handle<FixedArray> function_table, Handle<FixedArray> signature_table) {
  Handle<FixedArray> dispatch_tables(
      FixedArray::cast(table_obj->GetEmbedderField(kDispatchTables)), isolate);
  DCHECK_EQ(0, dispatch_tables->length() % 4);

  if (instance.is_null()) return dispatch_tables;
  // TODO(titzer): use weak cells here to avoid leaking instances.

  // Grow the dispatch table and add a new triple at the end.
  Handle<FixedArray> new_dispatch_tables =
      isolate->factory()->CopyFixedArrayAndGrow(dispatch_tables, 4);

  new_dispatch_tables->set(dispatch_tables->length() + 0, *instance);
  new_dispatch_tables->set(dispatch_tables->length() + 1,
                           Smi::FromInt(table_index));
  new_dispatch_tables->set(dispatch_tables->length() + 2, *function_table);
  new_dispatch_tables->set(dispatch_tables->length() + 3, *signature_table);

  table_obj->SetEmbedderField(WasmTableObject::kDispatchTables,
                              *new_dispatch_tables);

  return new_dispatch_tables;
}

DEFINE_OBJ_ACCESSORS(WasmTableObject, functions, kFunctions, FixedArray)

DEFINE_OBJ_GETTER(WasmTableObject, dispatch_tables, kDispatchTables, FixedArray)

uint32_t WasmTableObject::current_length() { return functions()->length(); }

bool WasmTableObject::has_maximum_length() {
  return GetEmbedderField(kMaximum)->Number() >= 0;
}

int64_t WasmTableObject::maximum_length() {
  return static_cast<int64_t>(GetEmbedderField(kMaximum)->Number());
}

WasmTableObject* WasmTableObject::cast(Object* object) {
  DCHECK(object && object->IsJSObject());
  // TODO(titzer): brand check for WasmTableObject.
  return reinterpret_cast<WasmTableObject*>(object);
}

void WasmTableObject::grow(Isolate* isolate, uint32_t count) {
  Handle<FixedArray> dispatch_tables(
      FixedArray::cast(GetEmbedderField(kDispatchTables)));
  DCHECK_EQ(0, dispatch_tables->length() % 4);
  uint32_t old_size = functions()->length();

  Zone specialization_zone(isolate->allocator(), ZONE_NAME);
  for (int i = 0; i < dispatch_tables->length(); i += 4) {
    Handle<FixedArray> old_function_table(
        FixedArray::cast(dispatch_tables->get(i + 2)));
    Handle<FixedArray> old_signature_table(
        FixedArray::cast(dispatch_tables->get(i + 3)));
    Handle<FixedArray> new_function_table =
        isolate->factory()->CopyFixedArrayAndGrow(old_function_table, count);
    Handle<FixedArray> new_signature_table =
        isolate->factory()->CopyFixedArrayAndGrow(old_signature_table, count);

    // Update dispatch tables with new function/signature tables
    dispatch_tables->set(i + 2, *new_function_table);
    dispatch_tables->set(i + 3, *new_signature_table);

    // Patch the code of the respective instance.
    CodeSpecialization code_specialization(isolate, &specialization_zone);
    code_specialization.PatchTableSize(old_size, old_size + count);
    code_specialization.RelocateObject(old_function_table, new_function_table);
    code_specialization.RelocateObject(old_signature_table,
                                       new_signature_table);
    code_specialization.ApplyToWholeInstance(
        WasmInstanceObject::cast(dispatch_tables->get(i)));
  }
}

namespace {

Handle<JSArrayBuffer> GrowMemoryBuffer(Isolate* isolate,
                                       Handle<JSArrayBuffer> old_buffer,
                                       uint32_t pages, uint32_t max_pages) {
  Address old_mem_start = nullptr;
  uint32_t old_size = 0;
  if (!old_buffer.is_null()) {
    DCHECK(old_buffer->byte_length()->IsNumber());
    old_mem_start = static_cast<Address>(old_buffer->backing_store());
    old_size = old_buffer->byte_length()->Number();
  }
  DCHECK_GE(std::numeric_limits<uint32_t>::max(),
            old_size + pages * WasmModule::kPageSize);
  uint32_t new_size = old_size + pages * WasmModule::kPageSize;
  if (new_size <= old_size || max_pages * WasmModule::kPageSize < new_size ||
      FLAG_wasm_max_mem_pages * WasmModule::kPageSize < new_size) {
    return Handle<JSArrayBuffer>::null();
  }

  // TODO(gdeepti): Change the protection here instead of allocating a new
  // buffer before guard regions are turned on, see issue #5886.
  const bool enable_guard_regions =
      (old_buffer.is_null() && EnableGuardRegions()) ||
      (!old_buffer.is_null() && old_buffer->has_guard_region());
  Handle<JSArrayBuffer> new_buffer =
      NewArrayBuffer(isolate, new_size, enable_guard_regions);
  if (new_buffer.is_null()) return new_buffer;
  Address new_mem_start = static_cast<Address>(new_buffer->backing_store());
  memcpy(new_mem_start, old_mem_start, old_size);
  return new_buffer;
}

// May GC, because SetSpecializationMemInfoFrom may GC
void SetInstanceMemory(Isolate* isolate, Handle<WasmInstanceObject> instance,
                       Handle<JSArrayBuffer> buffer) {
  instance->set_memory_buffer(*buffer);
  WasmCompiledModule::SetSpecializationMemInfoFrom(
      isolate->factory(), handle(instance->compiled_module()), buffer);
  if (instance->has_debug_info()) {
    instance->debug_info()->UpdateMemory(*buffer);
  }
}

void UncheckedUpdateInstanceMemory(Isolate* isolate,
                                   Handle<WasmInstanceObject> instance,
                                   Address old_mem_start, uint32_t old_size) {
  DCHECK(instance->has_memory_buffer());
  Handle<JSArrayBuffer> mem_buffer(instance->memory_buffer());
  uint32_t new_size = mem_buffer->byte_length()->Number();
  Address new_mem_start = static_cast<Address>(mem_buffer->backing_store());
  DCHECK_NOT_NULL(new_mem_start);
  Zone specialization_zone(isolate->allocator(), ZONE_NAME);
  CodeSpecialization code_specialization(isolate, &specialization_zone);
  code_specialization.RelocateMemoryReferences(old_mem_start, old_size,
                                               new_mem_start, new_size);
  code_specialization.ApplyToWholeInstance(*instance);
}

}  // namespace

Handle<WasmMemoryObject> WasmMemoryObject::New(Isolate* isolate,
                                               Handle<JSArrayBuffer> buffer,
                                               int32_t maximum) {
  Handle<JSFunction> memory_ctor(
      isolate->native_context()->wasm_memory_constructor());
  Handle<JSObject> memory_obj =
      isolate->factory()->NewJSObject(memory_ctor, TENURED);
  memory_obj->SetEmbedderField(kWrapperTracerHeader, Smi::kZero);
  buffer.is_null() ? memory_obj->SetEmbedderField(
                         kArrayBuffer, isolate->heap()->undefined_value())
                   : memory_obj->SetEmbedderField(kArrayBuffer, *buffer);
  Handle<Object> max = isolate->factory()->NewNumber(maximum);
  memory_obj->SetEmbedderField(kMaximum, *max);
  Handle<Symbol> memory_sym(isolate->native_context()->wasm_memory_sym());
  Object::SetProperty(memory_obj, memory_sym, memory_obj, STRICT).Check();
  return Handle<WasmMemoryObject>::cast(memory_obj);
}

DEFINE_OPTIONAL_OBJ_ACCESSORS(WasmMemoryObject, buffer, kArrayBuffer,
                              JSArrayBuffer)
DEFINE_OPTIONAL_OBJ_ACCESSORS(WasmMemoryObject, instances_link, kInstancesLink,
                              WasmInstanceWrapper)

uint32_t WasmMemoryObject::current_pages() {
  uint32_t byte_length;
  CHECK(buffer()->byte_length()->ToUint32(&byte_length));
  return byte_length / wasm::WasmModule::kPageSize;
}

bool WasmMemoryObject::has_maximum_pages() {
  return GetEmbedderField(kMaximum)->Number() >= 0;
}

int32_t WasmMemoryObject::maximum_pages() {
  return static_cast<int32_t>(GetEmbedderField(kMaximum)->Number());
}

WasmMemoryObject* WasmMemoryObject::cast(Object* object) {
  DCHECK(object && object->IsJSObject());
  // TODO(titzer): brand check for WasmMemoryObject.
  return reinterpret_cast<WasmMemoryObject*>(object);
}

void WasmMemoryObject::AddInstance(Isolate* isolate,
                                   Handle<WasmInstanceObject> instance) {
  Handle<WasmInstanceWrapper> instance_wrapper =
      handle(instance->instance_wrapper());
  if (has_instances_link()) {
    Handle<WasmInstanceWrapper> current_wrapper(instances_link());
    DCHECK(WasmInstanceWrapper::IsWasmInstanceWrapper(*current_wrapper));
    DCHECK(!current_wrapper->has_previous());
    instance_wrapper->set_next_wrapper(*current_wrapper);
    current_wrapper->set_previous_wrapper(*instance_wrapper);
  }
  set_instances_link(*instance_wrapper);
}

void WasmMemoryObject::ResetInstancesLink(Isolate* isolate) {
  Handle<Object> undefined = isolate->factory()->undefined_value();
  SetEmbedderField(kInstancesLink, *undefined);
}

// static
int32_t WasmMemoryObject::Grow(Isolate* isolate,
                               Handle<WasmMemoryObject> memory_object,
                               uint32_t pages) {
  Handle<JSArrayBuffer> old_buffer;
  uint32_t old_size = 0;
  Address old_mem_start = nullptr;
  if (memory_object->has_buffer()) {
    old_buffer = handle(memory_object->buffer());
    old_size = old_buffer->byte_length()->Number();
    old_mem_start = static_cast<Address>(old_buffer->backing_store());
  }
  Handle<JSArrayBuffer> new_buffer;
  // Return current size if grow by 0.
  if (pages == 0) {
    // Even for pages == 0, we need to attach a new JSArrayBuffer with the same
    // backing store and neuter the old one to be spec compliant.
    if (!old_buffer.is_null() && old_size != 0) {
      new_buffer = SetupArrayBuffer(
          isolate, old_buffer->allocation_base(),
          old_buffer->allocation_length(), old_buffer->backing_store(),
          old_size, old_buffer->is_external(), old_buffer->has_guard_region());
      memory_object->set_buffer(*new_buffer);
    }
    DCHECK_EQ(0, old_size % WasmModule::kPageSize);
    return old_size / WasmModule::kPageSize;
  }
  if (!memory_object->has_instances_link()) {
    // Memory object does not have an instance associated with it, just grow
    uint32_t max_pages;
    if (memory_object->has_maximum_pages()) {
      max_pages = static_cast<uint32_t>(memory_object->maximum_pages());
      if (FLAG_wasm_max_mem_pages < max_pages) return -1;
    } else {
      max_pages = FLAG_wasm_max_mem_pages;
    }
    new_buffer = GrowMemoryBuffer(isolate, old_buffer, pages, max_pages);
    if (new_buffer.is_null()) return -1;
  } else {
    Handle<WasmInstanceWrapper> instance_wrapper(
        memory_object->instances_link());
    DCHECK(WasmInstanceWrapper::IsWasmInstanceWrapper(*instance_wrapper));
    DCHECK(instance_wrapper->has_instance());
    Handle<WasmInstanceObject> instance = instance_wrapper->instance_object();
    DCHECK(IsWasmInstance(*instance));
    uint32_t max_pages = instance->GetMaxMemoryPages();

    // Grow memory object buffer and update instances associated with it.
    new_buffer = GrowMemoryBuffer(isolate, old_buffer, pages, max_pages);
    if (new_buffer.is_null()) return -1;
    DCHECK(!instance_wrapper->has_previous());
    SetInstanceMemory(isolate, instance, new_buffer);
    UncheckedUpdateInstanceMemory(isolate, instance, old_mem_start, old_size);
    while (instance_wrapper->has_next()) {
      instance_wrapper = instance_wrapper->next_wrapper();
      DCHECK(WasmInstanceWrapper::IsWasmInstanceWrapper(*instance_wrapper));
      Handle<WasmInstanceObject> instance = instance_wrapper->instance_object();
      DCHECK(IsWasmInstance(*instance));
      SetInstanceMemory(isolate, instance, new_buffer);
      UncheckedUpdateInstanceMemory(isolate, instance, old_mem_start, old_size);
    }
  }
  memory_object->set_buffer(*new_buffer);
  DCHECK_EQ(0, old_size % WasmModule::kPageSize);
  return old_size / WasmModule::kPageSize;
}

DEFINE_OBJ_ACCESSORS(WasmInstanceObject, compiled_module, kCompiledModule,
                     WasmCompiledModule)
DEFINE_OPTIONAL_OBJ_ACCESSORS(WasmInstanceObject, globals_buffer,
                              kGlobalsArrayBuffer, JSArrayBuffer)
DEFINE_OPTIONAL_OBJ_ACCESSORS(WasmInstanceObject, memory_buffer,
                              kMemoryArrayBuffer, JSArrayBuffer)
DEFINE_OPTIONAL_OBJ_ACCESSORS(WasmInstanceObject, memory_object, kMemoryObject,
                              WasmMemoryObject)
DEFINE_OPTIONAL_OBJ_ACCESSORS(WasmInstanceObject, debug_info, kDebugInfo,
                              WasmDebugInfo)
DEFINE_OPTIONAL_OBJ_ACCESSORS(WasmInstanceObject, instance_wrapper,
                              kWasmMemInstanceWrapper, WasmInstanceWrapper)

WasmModuleObject* WasmInstanceObject::module_object() {
  return *compiled_module()->wasm_module();
}

WasmModule* WasmInstanceObject::module() { return compiled_module()->module(); }

Handle<WasmDebugInfo> WasmInstanceObject::GetOrCreateDebugInfo(
    Handle<WasmInstanceObject> instance) {
  if (instance->has_debug_info()) return handle(instance->debug_info());
  Handle<WasmDebugInfo> new_info = WasmDebugInfo::New(instance);
  DCHECK(instance->has_debug_info());
  return new_info;
}

WasmInstanceObject* WasmInstanceObject::cast(Object* object) {
  DCHECK(IsWasmInstanceObject(object));
  return reinterpret_cast<WasmInstanceObject*>(object);
}

bool WasmInstanceObject::IsWasmInstanceObject(Object* object) {
  if (!object->IsJSObject()) return false;

  JSObject* obj = JSObject::cast(object);
  Isolate* isolate = obj->GetIsolate();
  if (obj->GetEmbedderFieldCount() != kFieldCount) {
    return false;
  }

  Object* mem = obj->GetEmbedderField(kMemoryArrayBuffer);
  if (!(mem->IsUndefined(isolate) || mem->IsJSArrayBuffer()) ||
      !WasmCompiledModule::IsWasmCompiledModule(
          obj->GetEmbedderField(kCompiledModule))) {
    return false;
  }

  // All checks passed.
  return true;
}

Handle<WasmInstanceObject> WasmInstanceObject::New(
    Isolate* isolate, Handle<WasmCompiledModule> compiled_module) {
  Handle<JSFunction> instance_cons(
      isolate->native_context()->wasm_instance_constructor());
  Handle<JSObject> instance_object =
      isolate->factory()->NewJSObject(instance_cons, TENURED);
  instance_object->SetEmbedderField(kWrapperTracerHeader, Smi::kZero);

  Handle<Symbol> instance_sym(isolate->native_context()->wasm_instance_sym());
  Object::SetProperty(instance_object, instance_sym, instance_object, STRICT)
      .Check();
  Handle<WasmInstanceObject> instance(
      reinterpret_cast<WasmInstanceObject*>(*instance_object), isolate);

  instance->SetEmbedderField(kCompiledModule, *compiled_module);
  instance->SetEmbedderField(kMemoryObject, isolate->heap()->undefined_value());
  Handle<WasmInstanceWrapper> instance_wrapper =
      WasmInstanceWrapper::New(isolate, instance);
  instance->SetEmbedderField(kWasmMemInstanceWrapper, *instance_wrapper);
  return instance;
}

int32_t WasmInstanceObject::GetMemorySize() {
  if (!has_memory_buffer()) return 0;
  uint32_t bytes = memory_buffer()->byte_length()->Number();
  DCHECK_EQ(0, bytes % WasmModule::kPageSize);
  return bytes / WasmModule::kPageSize;
}

int32_t WasmInstanceObject::GrowMemory(Isolate* isolate,
                                       Handle<WasmInstanceObject> instance,
                                       uint32_t pages) {
  if (pages == 0) return instance->GetMemorySize();
  if (instance->has_memory_object()) {
    return WasmMemoryObject::Grow(
        isolate, handle(instance->memory_object(), isolate), pages);
  }

  // No other instances to grow, grow just the one.
  uint32_t old_size = 0;
  Address old_mem_start = nullptr;
  Handle<JSArrayBuffer> old_buffer;
  if (instance->has_memory_buffer()) {
    old_buffer = handle(instance->memory_buffer(), isolate);
    old_size = old_buffer->byte_length()->Number();
    old_mem_start = static_cast<Address>(old_buffer->backing_store());
  }
  uint32_t max_pages = instance->GetMaxMemoryPages();
  Handle<JSArrayBuffer> buffer =
      GrowMemoryBuffer(isolate, old_buffer, pages, max_pages);
  if (buffer.is_null()) return -1;
  SetInstanceMemory(isolate, instance, buffer);
  UncheckedUpdateInstanceMemory(isolate, instance, old_mem_start, old_size);
  DCHECK_EQ(0, old_size % WasmModule::kPageSize);
  return old_size / WasmModule::kPageSize;
}

uint32_t WasmInstanceObject::GetMaxMemoryPages() {
  if (has_memory_object()) {
    if (memory_object()->has_maximum_pages()) {
      uint32_t maximum =
          static_cast<uint32_t>(memory_object()->maximum_pages());
      if (maximum < FLAG_wasm_max_mem_pages) return maximum;
    }
  }
  uint32_t compiled_max_pages = compiled_module()->module()->max_mem_pages;
  Isolate* isolate = GetIsolate();
  assert(compiled_module()->module()->is_wasm());
  isolate->counters()->wasm_wasm_max_mem_pages_count()->AddSample(
      compiled_max_pages);
  if (compiled_max_pages != 0) return compiled_max_pages;
  return FLAG_wasm_max_mem_pages;
}

WasmInstanceObject* WasmExportedFunction::instance() {
  return WasmInstanceObject::cast(GetEmbedderField(kInstance));
}

int WasmExportedFunction::function_index() {
  int32_t func_index;
  CHECK(GetEmbedderField(kIndex)->ToInt32(&func_index));
  return func_index;
}

WasmExportedFunction* WasmExportedFunction::cast(Object* object) {
  DCHECK(object && object->IsJSFunction());
  DCHECK_EQ(Code::JS_TO_WASM_FUNCTION,
            JSFunction::cast(object)->code()->kind());
  // TODO(titzer): brand check for WasmExportedFunction.
  return reinterpret_cast<WasmExportedFunction*>(object);
}

Handle<WasmExportedFunction> WasmExportedFunction::New(
    Isolate* isolate, Handle<WasmInstanceObject> instance,
    MaybeHandle<String> maybe_name, int func_index, int arity,
    Handle<Code> export_wrapper) {
  Handle<String> name;
  if (maybe_name.is_null()) {
    EmbeddedVector<char, 16> buffer;
    int length = SNPrintF(buffer, "%d", func_index);
    name = isolate->factory()
               ->NewStringFromOneByte(
                   Vector<uint8_t>::cast(buffer.SubVector(0, length)))
               .ToHandleChecked();
  } else {
    name = maybe_name.ToHandleChecked();
  }
  DCHECK_EQ(Code::JS_TO_WASM_FUNCTION, export_wrapper->kind());
  Handle<SharedFunctionInfo> shared =
      isolate->factory()->NewSharedFunctionInfo(name, export_wrapper, false);
  shared->set_length(arity);
  shared->set_internal_formal_parameter_count(arity);
  Handle<JSFunction> function = isolate->factory()->NewFunction(
      isolate->wasm_function_map(), name, export_wrapper);
  function->SetEmbedderField(kWrapperTracerHeader, Smi::kZero);

  function->set_shared(*shared);

  function->SetEmbedderField(kInstance, *instance);
  function->SetEmbedderField(kIndex, Smi::FromInt(func_index));
  return Handle<WasmExportedFunction>::cast(function);
}

bool WasmSharedModuleData::IsWasmSharedModuleData(Object* object) {
  if (!object->IsFixedArray()) return false;
  FixedArray* arr = FixedArray::cast(object);
  if (arr->length() != kFieldCount) return false;
  Isolate* isolate = arr->GetIsolate();
  if (!arr->get(kModuleWrapper)->IsForeign()) return false;
  if (!arr->get(kModuleBytes)->IsUndefined(isolate) &&
      !arr->get(kModuleBytes)->IsSeqOneByteString())
    return false;
  if (!arr->get(kScript)->IsScript()) return false;
  if (!arr->get(kAsmJsOffsetTable)->IsUndefined(isolate) &&
      !arr->get(kAsmJsOffsetTable)->IsByteArray())
    return false;
  if (!arr->get(kBreakPointInfos)->IsUndefined(isolate) &&
      !arr->get(kBreakPointInfos)->IsFixedArray())
    return false;
  return true;
}

WasmSharedModuleData* WasmSharedModuleData::cast(Object* object) {
  DCHECK(IsWasmSharedModuleData(object));
  return reinterpret_cast<WasmSharedModuleData*>(object);
}

wasm::WasmModule* WasmSharedModuleData::module() {
  // We populate the kModuleWrapper field with a Foreign holding the
  // address to the address of a WasmModule. This is because we can
  // handle both cases when the WasmModule's lifetime is managed through
  // a Managed<WasmModule> object, as well as cases when it's managed
  // by the embedder. CcTests fall into the latter case.
  return *(reinterpret_cast<wasm::WasmModule**>(
      Foreign::cast(get(kModuleWrapper))->foreign_address()));
}

DEFINE_OPTIONAL_ARR_ACCESSORS(WasmSharedModuleData, module_bytes, kModuleBytes,
                              SeqOneByteString);
DEFINE_ARR_GETTER(WasmSharedModuleData, script, kScript, Script);
DEFINE_OPTIONAL_ARR_ACCESSORS(WasmSharedModuleData, asm_js_offset_table,
                              kAsmJsOffsetTable, ByteArray);
DEFINE_OPTIONAL_ARR_GETTER(WasmSharedModuleData, breakpoint_infos,
                           kBreakPointInfos, FixedArray);
DEFINE_OPTIONAL_ARR_GETTER(WasmSharedModuleData, lazy_compilation_orchestrator,
                           kLazyCompilationOrchestrator, Foreign);

Handle<WasmSharedModuleData> WasmSharedModuleData::New(
    Isolate* isolate, Handle<Foreign> module_wrapper,
    Handle<SeqOneByteString> module_bytes, Handle<Script> script,
    Handle<ByteArray> asm_js_offset_table) {
  Handle<FixedArray> arr =
      isolate->factory()->NewFixedArray(kFieldCount, TENURED);
  arr->set(kWrapperTracerHeader, Smi::kZero);
  arr->set(kModuleWrapper, *module_wrapper);
  if (!module_bytes.is_null()) {
    arr->set(kModuleBytes, *module_bytes);
  }
  if (!script.is_null()) {
    arr->set(kScript, *script);
  }
  if (!asm_js_offset_table.is_null()) {
    arr->set(kAsmJsOffsetTable, *asm_js_offset_table);
  }

  DCHECK(WasmSharedModuleData::IsWasmSharedModuleData(*arr));
  return Handle<WasmSharedModuleData>::cast(arr);
}

bool WasmSharedModuleData::is_asm_js() {
  bool asm_js = module()->is_asm_js();
  DCHECK_EQ(asm_js, script()->IsUserJavaScript());
  DCHECK_EQ(asm_js, has_asm_js_offset_table());
  return asm_js;
}

void WasmSharedModuleData::ReinitializeAfterDeserialization(
    Isolate* isolate, Handle<WasmSharedModuleData> shared) {
  DCHECK(shared->get(kModuleWrapper)->IsUndefined(isolate));
#ifdef DEBUG
  // No BreakpointInfo objects should survive deserialization.
  if (shared->has_breakpoint_infos()) {
    for (int i = 0, e = shared->breakpoint_infos()->length(); i < e; ++i) {
      DCHECK(shared->breakpoint_infos()->get(i)->IsUndefined(isolate));
    }
  }
#endif

  shared->set(kBreakPointInfos, isolate->heap()->undefined_value());

  WasmModule* module = nullptr;
  {
    // We parse the module again directly from the module bytes, so
    // the underlying storage must not be moved meanwhile.
    DisallowHeapAllocation no_allocation;
    SeqOneByteString* module_bytes = shared->module_bytes();
    const byte* start =
        reinterpret_cast<const byte*>(module_bytes->GetCharsAddress());
    const byte* end = start + module_bytes->length();
    // TODO(titzer): remember the module origin in the compiled_module
    // For now, we assume serialized modules did not originate from asm.js.
    ModuleResult result =
        DecodeWasmModule(isolate, start, end, false, kWasmOrigin);
    CHECK(result.ok());
    CHECK_NOT_NULL(result.val);
    // Take ownership of the WasmModule and immediately transfer it to the
    // WasmModuleWrapper below.
    module = result.val.release();
  }

  Handle<WasmModuleWrapper> module_wrapper =
      WasmModuleWrapper::New(isolate, module);

  shared->set(kModuleWrapper, *module_wrapper);
  DCHECK(WasmSharedModuleData::IsWasmSharedModuleData(*shared));
}

namespace {

int GetBreakpointPos(Isolate* isolate, Object* break_point_info_or_undef) {
  if (break_point_info_or_undef->IsUndefined(isolate)) return kMaxInt;
  return BreakPointInfo::cast(break_point_info_or_undef)->source_position();
}

int FindBreakpointInfoInsertPos(Isolate* isolate,
                                Handle<FixedArray> breakpoint_infos,
                                int position) {
  // Find insert location via binary search, taking care of undefined values on
  // the right. Position is always greater than zero.
  DCHECK_LT(0, position);

  int left = 0;                            // inclusive
  int right = breakpoint_infos->length();  // exclusive
  while (right - left > 1) {
    int mid = left + (right - left) / 2;
    Object* mid_obj = breakpoint_infos->get(mid);
    if (GetBreakpointPos(isolate, mid_obj) <= position) {
      left = mid;
    } else {
      right = mid;
    }
  }

  int left_pos = GetBreakpointPos(isolate, breakpoint_infos->get(left));
  return left_pos < position ? left + 1 : left;
}

}  // namespace

void WasmSharedModuleData::AddBreakpoint(Handle<WasmSharedModuleData> shared,
                                         int position,
                                         Handle<Object> break_point_object) {
  Isolate* isolate = shared->GetIsolate();
  Handle<FixedArray> breakpoint_infos;
  if (shared->has_breakpoint_infos()) {
    breakpoint_infos = handle(shared->breakpoint_infos(), isolate);
  } else {
    breakpoint_infos = isolate->factory()->NewFixedArray(4, TENURED);
    shared->set(kBreakPointInfos, *breakpoint_infos);
  }

  int insert_pos =
      FindBreakpointInfoInsertPos(isolate, breakpoint_infos, position);

  // If a BreakPointInfo object already exists for this position, add the new
  // breakpoint object and return.
  if (insert_pos < breakpoint_infos->length() &&
      GetBreakpointPos(isolate, breakpoint_infos->get(insert_pos)) ==
          position) {
    Handle<BreakPointInfo> old_info(
        BreakPointInfo::cast(breakpoint_infos->get(insert_pos)), isolate);
    BreakPointInfo::SetBreakPoint(old_info, break_point_object);
    return;
  }

  // Enlarge break positions array if necessary.
  bool need_realloc = !breakpoint_infos->get(breakpoint_infos->length() - 1)
                           ->IsUndefined(isolate);
  Handle<FixedArray> new_breakpoint_infos = breakpoint_infos;
  if (need_realloc) {
    new_breakpoint_infos = isolate->factory()->NewFixedArray(
        2 * breakpoint_infos->length(), TENURED);
    shared->set(kBreakPointInfos, *new_breakpoint_infos);
    // Copy over the entries [0, insert_pos).
    for (int i = 0; i < insert_pos; ++i)
      new_breakpoint_infos->set(i, breakpoint_infos->get(i));
  }

  // Move elements [insert_pos+1, ...] up by one.
  for (int i = insert_pos + 1; i < breakpoint_infos->length(); ++i) {
    Object* entry = breakpoint_infos->get(i);
    if (entry->IsUndefined(isolate)) break;
    new_breakpoint_infos->set(i + 1, entry);
  }

  // Generate new BreakpointInfo.
  Handle<BreakPointInfo> breakpoint_info =
      isolate->factory()->NewBreakPointInfo(position);
  BreakPointInfo::SetBreakPoint(breakpoint_info, break_point_object);

  // Now insert new position at insert_pos.
  new_breakpoint_infos->set(insert_pos, *breakpoint_info);
}

void WasmSharedModuleData::SetBreakpointsOnNewInstance(
    Handle<WasmSharedModuleData> shared, Handle<WasmInstanceObject> instance) {
  if (!shared->has_breakpoint_infos()) return;
  Isolate* isolate = shared->GetIsolate();
  Handle<WasmCompiledModule> compiled_module(instance->compiled_module(),
                                             isolate);
  Handle<WasmDebugInfo> debug_info =
      WasmInstanceObject::GetOrCreateDebugInfo(instance);

  Handle<FixedArray> breakpoint_infos(shared->breakpoint_infos(), isolate);
  // If the array exists, it should not be empty.
  DCHECK_LT(0, breakpoint_infos->length());

  for (int i = 0, e = breakpoint_infos->length(); i < e; ++i) {
    Handle<Object> obj(breakpoint_infos->get(i), isolate);
    if (obj->IsUndefined(isolate)) {
      for (; i < e; ++i) {
        DCHECK(breakpoint_infos->get(i)->IsUndefined(isolate));
      }
      break;
    }
    Handle<BreakPointInfo> breakpoint_info = Handle<BreakPointInfo>::cast(obj);
    int position = breakpoint_info->source_position();

    // Find the function for this breakpoint, and set the breakpoint.
    int func_index = compiled_module->GetContainingFunction(position);
    DCHECK_LE(0, func_index);
    WasmFunction& func = compiled_module->module()->functions[func_index];
    int offset_in_func = position - func.code_start_offset;
    WasmDebugInfo::SetBreakpoint(debug_info, func_index, offset_in_func);
  }
}

void WasmSharedModuleData::PrepareForLazyCompilation(
    Handle<WasmSharedModuleData> shared) {
  if (shared->has_lazy_compilation_orchestrator()) return;
  Isolate* isolate = shared->GetIsolate();
  LazyCompilationOrchestrator* orch = new LazyCompilationOrchestrator();
  Handle<Managed<LazyCompilationOrchestrator>> orch_handle =
      Managed<LazyCompilationOrchestrator>::New(isolate, orch);
  shared->set(WasmSharedModuleData::kLazyCompilationOrchestrator, *orch_handle);
}

Handle<WasmCompiledModule> WasmCompiledModule::New(
    Isolate* isolate, Handle<WasmSharedModuleData> shared,
    Handle<FixedArray> code_table,
    MaybeHandle<FixedArray> maybe_empty_function_tables,
    MaybeHandle<FixedArray> maybe_signature_tables) {
  Handle<FixedArray> ret =
      isolate->factory()->NewFixedArray(PropertyIndices::Count, TENURED);
  // WasmCompiledModule::cast would fail since fields are not set yet.
  Handle<WasmCompiledModule> compiled_module(
      reinterpret_cast<WasmCompiledModule*>(*ret), isolate);
  compiled_module->InitId();
  compiled_module->set_shared(shared);
  compiled_module->set_native_context(isolate->native_context());
  compiled_module->set_code_table(code_table);
  int function_table_count =
      static_cast<int>(shared->module()->function_tables.size());
  if (function_table_count > 0) {
    compiled_module->set_signature_tables(
        maybe_signature_tables.ToHandleChecked());
    compiled_module->set_empty_function_tables(
        maybe_empty_function_tables.ToHandleChecked());
    compiled_module->set_function_tables(
        maybe_empty_function_tables.ToHandleChecked());
  }
  // TODO(mtrofin): we copy these because the order of finalization isn't
  // reliable, and we need these at Reset (which is called at
  // finalization). If the order were reliable, and top-down, we could instead
  // just get them from shared().
  compiled_module->set_min_mem_pages(shared->module()->min_mem_pages);
  compiled_module->set_num_imported_functions(
      shared->module()->num_imported_functions);
  return compiled_module;
}

Handle<WasmCompiledModule> WasmCompiledModule::Clone(
    Isolate* isolate, Handle<WasmCompiledModule> module) {
  Handle<FixedArray> code_copy =
      isolate->factory()->CopyFixedArray(module->code_table());
  Handle<WasmCompiledModule> ret = Handle<WasmCompiledModule>::cast(
      isolate->factory()->CopyFixedArray(module));
  ret->InitId();
  ret->set_code_table(code_copy);
  ret->reset_weak_owning_instance();
  ret->reset_weak_next_instance();
  ret->reset_weak_prev_instance();
  ret->reset_weak_exported_functions();
  if (ret->has_embedded_mem_start()) {
    WasmCompiledModule::recreate_embedded_mem_start(ret, isolate->factory(),
                                                    ret->embedded_mem_start());
  }
  if (ret->has_globals_start()) {
    WasmCompiledModule::recreate_globals_start(ret, isolate->factory(),
                                               ret->globals_start());
  }
  if (ret->has_embedded_mem_size()) {
    WasmCompiledModule::recreate_embedded_mem_size(ret, isolate->factory(),
                                                   ret->embedded_mem_size());
  }
  return ret;
}

void WasmCompiledModule::Reset(Isolate* isolate,
                               WasmCompiledModule* compiled_module) {
  DisallowHeapAllocation no_gc;
  TRACE("Resetting %d\n", compiled_module->instance_id());
  Object* undefined = *isolate->factory()->undefined_value();
  Object* fct_obj = compiled_module->ptr_to_code_table();
  if (fct_obj != nullptr && fct_obj != undefined) {
    uint32_t old_mem_size = compiled_module->mem_size();
    // We use default_mem_size throughout, as the mem size of an uninstantiated
    // module, because if we can statically prove a memory access is over
    // bounds, we'll codegen a trap. See {WasmGraphBuilder::BoundsCheckMem}
    uint32_t default_mem_size = compiled_module->default_mem_size();
    Address old_mem_start = compiled_module->GetEmbeddedMemStartOrNull();

    // Patch code to update memory references, global references, and function
    // table references.
    Zone specialization_zone(isolate->allocator(), ZONE_NAME);
    CodeSpecialization code_specialization(isolate, &specialization_zone);

    if (old_mem_size > 0 && old_mem_start != nullptr) {
      code_specialization.RelocateMemoryReferences(old_mem_start, old_mem_size,
                                                   nullptr, default_mem_size);
    }

    if (compiled_module->has_globals_start()) {
      Address globals_start =
          reinterpret_cast<Address>(compiled_module->globals_start());
      code_specialization.RelocateGlobals(globals_start, nullptr);
      compiled_module->set_globals_start(0);
    }

    // Reset function tables.
    if (compiled_module->has_function_tables()) {
      FixedArray* function_tables = compiled_module->ptr_to_function_tables();
      FixedArray* empty_function_tables =
          compiled_module->ptr_to_empty_function_tables();
      if (function_tables != empty_function_tables) {
        DCHECK_EQ(function_tables->length(), empty_function_tables->length());
        for (int i = 0, e = function_tables->length(); i < e; ++i) {
          code_specialization.RelocateObject(
              handle(function_tables->get(i), isolate),
              handle(empty_function_tables->get(i), isolate));
        }
        compiled_module->set_ptr_to_function_tables(empty_function_tables);
      }
    }

    FixedArray* functions = FixedArray::cast(fct_obj);
    for (int i = compiled_module->num_imported_functions(),
             end = functions->length();
         i < end; ++i) {
      Code* code = Code::cast(functions->get(i));
      // Skip lazy compile stubs.
      if (code->builtin_index() == Builtins::kWasmCompileLazy) continue;
      if (code->kind() != Code::WASM_FUNCTION) {
        // From here on, there should only be wrappers for exported functions.
        for (; i < end; ++i) {
          DCHECK_EQ(Code::JS_TO_WASM_FUNCTION,
                    Code::cast(functions->get(i))->kind());
        }
        break;
      }
      bool changed =
          code_specialization.ApplyToWasmCode(code, SKIP_ICACHE_FLUSH);
      // TODO(wasm): Check if this is faster than passing FLUSH_ICACHE_IF_NEEDED
      // above.
      if (changed) {
        Assembler::FlushICache(isolate, code->instruction_start(),
                               code->instruction_size());
      }
    }
  }
  compiled_module->ResetSpecializationMemInfoIfNeeded();
}

void WasmCompiledModule::InitId() {
#if DEBUG
  static uint32_t instance_id_counter = 0;
  set(kID_instance_id, Smi::FromInt(instance_id_counter++));
  TRACE("New compiled module id: %d\n", instance_id());
#endif
}

void WasmCompiledModule::ResetSpecializationMemInfoIfNeeded() {
  DisallowHeapAllocation no_gc;
  if (has_embedded_mem_start()) {
    set_embedded_mem_size(default_mem_size());
    set_embedded_mem_start(0);
  }
}

void WasmCompiledModule::SetSpecializationMemInfoFrom(
    Factory* factory, Handle<WasmCompiledModule> compiled_module,
    Handle<JSArrayBuffer> buffer) {
  DCHECK(!buffer.is_null());
  size_t start_address = reinterpret_cast<size_t>(buffer->backing_store());
  uint32_t size = static_cast<uint32_t>(buffer->byte_length()->Number());
  if (!compiled_module->has_embedded_mem_start()) {
    DCHECK(!compiled_module->has_embedded_mem_size());
    WasmCompiledModule::recreate_embedded_mem_start(compiled_module, factory,
                                                    start_address);
    WasmCompiledModule::recreate_embedded_mem_size(compiled_module, factory,
                                                   size);
  } else {
    compiled_module->set_embedded_mem_start(start_address);
    compiled_module->set_embedded_mem_size(size);
  }
}

void WasmCompiledModule::SetGlobalsStartAddressFrom(
    Factory* factory, Handle<WasmCompiledModule> compiled_module,
    Handle<JSArrayBuffer> buffer) {
  DCHECK(!buffer.is_null());
  size_t start_address = reinterpret_cast<size_t>(buffer->backing_store());
  if (!compiled_module->has_globals_start()) {
    WasmCompiledModule::recreate_globals_start(compiled_module, factory,
                                               start_address);
  } else {
    compiled_module->set_globals_start(start_address);
  }
}

MaybeHandle<String> WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
    Isolate* isolate, Handle<WasmCompiledModule> compiled_module,
    uint32_t offset, uint32_t size) {
  // TODO(wasm): cache strings from modules if it's a performance win.
  Handle<SeqOneByteString> module_bytes(compiled_module->module_bytes(),
                                        isolate);
  DCHECK_GE(module_bytes->length(), offset);
  DCHECK_GE(module_bytes->length() - offset, size);
  // UTF8 validation happens at decode time.
  DCHECK(unibrow::Utf8::Validate(
      reinterpret_cast<const byte*>(module_bytes->GetCharsAddress() + offset),
      size));
  DCHECK_GE(kMaxInt, offset);
  DCHECK_GE(kMaxInt, size);
  return isolate->factory()->NewStringFromUtf8SubString(
      module_bytes, static_cast<int>(offset), static_cast<int>(size));
}

bool WasmCompiledModule::IsWasmCompiledModule(Object* obj) {
  if (!obj->IsFixedArray()) return false;
  FixedArray* arr = FixedArray::cast(obj);
  if (arr->length() != PropertyIndices::Count) return false;
  Isolate* isolate = arr->GetIsolate();
#define WCM_CHECK_TYPE(NAME, TYPE_CHECK) \
  do {                                   \
    Object* obj = arr->get(kID_##NAME);  \
    if (!(TYPE_CHECK)) return false;     \
  } while (false);
// We're OK with undefined, generally, because maybe we don't
// have a value for that item. For example, we may not have a
// memory, or globals.
// We're not OK with the const numbers being undefined. They are
// expected to be initialized at construction.
#define WCM_CHECK_OBJECT(TYPE, NAME) \
  WCM_CHECK_TYPE(NAME, obj->IsUndefined(isolate) || obj->Is##TYPE())
#define WCM_CHECK_CONST_OBJECT(TYPE, NAME) \
  WCM_CHECK_TYPE(NAME, obj->IsUndefined(isolate) || obj->Is##TYPE())
#define WCM_CHECK_WASM_OBJECT(TYPE, NAME) \
  WCM_CHECK_TYPE(NAME, TYPE::Is##TYPE(obj))
#define WCM_CHECK_WEAK_LINK(TYPE, NAME) WCM_CHECK_OBJECT(WeakCell, NAME)
#define WCM_CHECK_SMALL_NUMBER(TYPE, NAME) \
  WCM_CHECK_TYPE(NAME, obj->IsUndefined(isolate) || obj->IsSmi())
#define WCM_CHECK(KIND, TYPE, NAME) WCM_CHECK_##KIND(TYPE, NAME)
#define WCM_CHECK_SMALL_CONST_NUMBER(TYPE, NAME) \
  WCM_CHECK_TYPE(NAME, obj->IsSmi())
#define WCM_CHECK_LARGE_NUMBER(TYPE, NAME) \
  WCM_CHECK_TYPE(NAME, obj->IsUndefined(isolate) || obj->IsMutableHeapNumber())
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
    if (!current->has_weak_next_instance()) break;
    CHECK(!current->ptr_to_weak_next_instance()->cleared());
    current =
        WasmCompiledModule::cast(current->ptr_to_weak_next_instance()->value());
  }
  PrintF("\n");
#endif
}

void WasmCompiledModule::ReinitializeAfterDeserialization(
    Isolate* isolate, Handle<WasmCompiledModule> compiled_module) {
  // This method must only be called immediately after deserialization.
  // At this point, no module wrapper exists, so the shared module data is
  // incomplete.
  Handle<WasmSharedModuleData> shared(
      static_cast<WasmSharedModuleData*>(compiled_module->get(kID_shared)),
      isolate);
  DCHECK(!WasmSharedModuleData::IsWasmSharedModuleData(*shared));
  WasmSharedModuleData::ReinitializeAfterDeserialization(isolate, shared);
  WasmCompiledModule::Reset(isolate, *compiled_module);
  DCHECK(WasmSharedModuleData::IsWasmSharedModuleData(*shared));
}

uint32_t WasmCompiledModule::mem_size() const {
  DCHECK(has_embedded_mem_size() == has_embedded_mem_start());
  return has_embedded_mem_start() ? embedded_mem_size() : default_mem_size();
}

uint32_t WasmCompiledModule::default_mem_size() const {
  return min_mem_pages() * WasmModule::kPageSize;
}

MaybeHandle<String> WasmCompiledModule::GetFunctionNameOrNull(
    Isolate* isolate, Handle<WasmCompiledModule> compiled_module,
    uint32_t func_index) {
  DCHECK_LT(func_index, compiled_module->module()->functions.size());
  WasmFunction& function = compiled_module->module()->functions[func_index];
  DCHECK_IMPLIES(function.name_offset == 0, function.name_length == 0);
  if (!function.name_offset) return {};
  return WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
      isolate, compiled_module, function.name_offset, function.name_length);
}

Handle<String> WasmCompiledModule::GetFunctionName(
    Isolate* isolate, Handle<WasmCompiledModule> compiled_module,
    uint32_t func_index) {
  MaybeHandle<String> name =
      GetFunctionNameOrNull(isolate, compiled_module, func_index);
  if (!name.is_null()) return name.ToHandleChecked();
  return isolate->factory()->NewStringFromStaticChars("<WASM UNNAMED>");
}

Vector<const uint8_t> WasmCompiledModule::GetRawFunctionName(
    uint32_t func_index) {
  DCHECK_GT(module()->functions.size(), func_index);
  WasmFunction& function = module()->functions[func_index];
  SeqOneByteString* bytes = module_bytes();
  DCHECK_GE(bytes->length(), function.name_offset);
  DCHECK_GE(bytes->length() - function.name_offset, function.name_length);
  return Vector<const uint8_t>(bytes->GetCharsAddress() + function.name_offset,
                               function.name_length);
}

int WasmCompiledModule::GetFunctionOffset(uint32_t func_index) {
  std::vector<WasmFunction>& functions = module()->functions;
  if (static_cast<uint32_t>(func_index) >= functions.size()) return -1;
  DCHECK_GE(kMaxInt, functions[func_index].code_start_offset);
  return static_cast<int>(functions[func_index].code_start_offset);
}

int WasmCompiledModule::GetContainingFunction(uint32_t byte_offset) {
  std::vector<WasmFunction>& functions = module()->functions;

  // Binary search for a function containing the given position.
  int left = 0;                                    // inclusive
  int right = static_cast<int>(functions.size());  // exclusive
  if (right == 0) return false;
  while (right - left > 1) {
    int mid = left + (right - left) / 2;
    if (functions[mid].code_start_offset <= byte_offset) {
      left = mid;
    } else {
      right = mid;
    }
  }
  // If the found function does not contains the given position, return -1.
  WasmFunction& func = functions[left];
  if (byte_offset < func.code_start_offset ||
      byte_offset >= func.code_end_offset) {
    return -1;
  }

  return left;
}

bool WasmCompiledModule::GetPositionInfo(uint32_t position,
                                         Script::PositionInfo* info) {
  int func_index = GetContainingFunction(position);
  if (func_index < 0) return false;

  WasmFunction& function = module()->functions[func_index];

  info->line = func_index;
  info->column = position - function.code_start_offset;
  info->line_start = function.code_start_offset;
  info->line_end = function.code_end_offset;
  return true;
}

namespace {

enum AsmJsOffsetTableEntryLayout {
  kOTEByteOffset,
  kOTECallPosition,
  kOTENumberConvPosition,
  kOTESize
};

Handle<ByteArray> GetDecodedAsmJsOffsetTable(
    Handle<WasmCompiledModule> compiled_module, Isolate* isolate) {
  DCHECK(compiled_module->is_asm_js());
  Handle<ByteArray> offset_table(
      compiled_module->shared()->asm_js_offset_table(), isolate);

  // The last byte in the asm_js_offset_tables ByteArray tells whether it is
  // still encoded (0) or decoded (1).
  enum AsmJsTableType : int { Encoded = 0, Decoded = 1 };
  int table_type = offset_table->get(offset_table->length() - 1);
  DCHECK(table_type == Encoded || table_type == Decoded);
  if (table_type == Decoded) return offset_table;

  AsmJsOffsetsResult asm_offsets;
  {
    DisallowHeapAllocation no_gc;
    const byte* bytes_start = offset_table->GetDataStartAddress();
    const byte* bytes_end = bytes_start + offset_table->length() - 1;
    asm_offsets = wasm::DecodeAsmJsOffsets(bytes_start, bytes_end);
  }
  // Wasm bytes must be valid and must contain asm.js offset table.
  DCHECK(asm_offsets.ok());
  DCHECK_GE(kMaxInt, asm_offsets.val.size());
  int num_functions = static_cast<int>(asm_offsets.val.size());
  int num_imported_functions =
      static_cast<int>(compiled_module->module()->num_imported_functions);
  DCHECK_EQ(compiled_module->module()->functions.size(),
            static_cast<size_t>(num_functions) + num_imported_functions);
  int num_entries = 0;
  for (int func = 0; func < num_functions; ++func) {
    size_t new_size = asm_offsets.val[func].size();
    DCHECK_LE(new_size, static_cast<size_t>(kMaxInt) - num_entries);
    num_entries += static_cast<int>(new_size);
  }
  // One byte to encode that this is a decoded table.
  DCHECK_GE(kMaxInt,
            1 + static_cast<uint64_t>(num_entries) * kOTESize * kIntSize);
  int total_size = 1 + num_entries * kOTESize * kIntSize;
  Handle<ByteArray> decoded_table =
      isolate->factory()->NewByteArray(total_size, TENURED);
  decoded_table->set(total_size - 1, AsmJsTableType::Decoded);
  compiled_module->shared()->set_asm_js_offset_table(*decoded_table);

  int idx = 0;
  std::vector<WasmFunction>& wasm_funs = compiled_module->module()->functions;
  for (int func = 0; func < num_functions; ++func) {
    std::vector<AsmJsOffsetEntry>& func_asm_offsets = asm_offsets.val[func];
    if (func_asm_offsets.empty()) continue;
    int func_offset =
        wasm_funs[num_imported_functions + func].code_start_offset;
    for (AsmJsOffsetEntry& e : func_asm_offsets) {
      // Byte offsets must be strictly monotonously increasing:
      DCHECK_IMPLIES(idx > 0, func_offset + e.byte_offset >
                                  decoded_table->get_int(idx - kOTESize));
      decoded_table->set_int(idx + kOTEByteOffset, func_offset + e.byte_offset);
      decoded_table->set_int(idx + kOTECallPosition, e.source_position_call);
      decoded_table->set_int(idx + kOTENumberConvPosition,
                             e.source_position_number_conversion);
      idx += kOTESize;
    }
  }
  DCHECK_EQ(total_size, idx * kIntSize + 1);
  return decoded_table;
}

}  // namespace

int WasmCompiledModule::GetAsmJsSourcePosition(
    Handle<WasmCompiledModule> compiled_module, uint32_t func_index,
    uint32_t byte_offset, bool is_at_number_conversion) {
  Isolate* isolate = compiled_module->GetIsolate();
  Handle<ByteArray> offset_table =
      GetDecodedAsmJsOffsetTable(compiled_module, isolate);

  DCHECK_LT(func_index, compiled_module->module()->functions.size());
  uint32_t func_code_offset =
      compiled_module->module()->functions[func_index].code_start_offset;
  uint32_t total_offset = func_code_offset + byte_offset;

  // Binary search for the total byte offset.
  int left = 0;                                              // inclusive
  int right = offset_table->length() / kIntSize / kOTESize;  // exclusive
  DCHECK_LT(left, right);
  while (right - left > 1) {
    int mid = left + (right - left) / 2;
    int mid_entry = offset_table->get_int(kOTESize * mid);
    DCHECK_GE(kMaxInt, mid_entry);
    if (static_cast<uint32_t>(mid_entry) <= total_offset) {
      left = mid;
    } else {
      right = mid;
    }
  }
  // There should be an entry for each position that could show up on the stack
  // trace:
  DCHECK_EQ(total_offset, offset_table->get_int(kOTESize * left));
  int idx = is_at_number_conversion ? kOTENumberConvPosition : kOTECallPosition;
  return offset_table->get_int(kOTESize * left + idx);
}

v8::debug::WasmDisassembly WasmCompiledModule::DisassembleFunction(
    int func_index) {
  DisallowHeapAllocation no_gc;

  if (func_index < 0 ||
      static_cast<uint32_t>(func_index) >= module()->functions.size())
    return {};

  SeqOneByteString* module_bytes_str = module_bytes();
  Vector<const byte> module_bytes(module_bytes_str->GetChars(),
                                  module_bytes_str->length());

  std::ostringstream disassembly_os;
  v8::debug::WasmDisassembly::OffsetTable offset_table;

  PrintWasmText(module(), module_bytes, static_cast<uint32_t>(func_index),
                disassembly_os, &offset_table);

  return {disassembly_os.str(), std::move(offset_table)};
}

bool WasmCompiledModule::GetPossibleBreakpoints(
    const v8::debug::Location& start, const v8::debug::Location& end,
    std::vector<v8::debug::BreakLocation>* locations) {
  DisallowHeapAllocation no_gc;

  std::vector<WasmFunction>& functions = module()->functions;
  if (start.GetLineNumber() < 0 || start.GetColumnNumber() < 0 ||
      (!end.IsEmpty() &&
       (end.GetLineNumber() < 0 || end.GetColumnNumber() < 0)))
    return false;

  // start_func_index, start_offset and end_func_index is inclusive.
  // end_offset is exclusive.
  // start_offset and end_offset are module-relative byte offsets.
  uint32_t start_func_index = start.GetLineNumber();
  if (start_func_index >= functions.size()) return false;
  int start_func_len = functions[start_func_index].code_end_offset -
                       functions[start_func_index].code_start_offset;
  if (start.GetColumnNumber() > start_func_len) return false;
  uint32_t start_offset =
      functions[start_func_index].code_start_offset + start.GetColumnNumber();
  uint32_t end_func_index;
  uint32_t end_offset;
  if (end.IsEmpty()) {
    // Default: everything till the end of the Script.
    end_func_index = static_cast<uint32_t>(functions.size() - 1);
    end_offset = functions[end_func_index].code_end_offset;
  } else {
    // If end is specified: Use it and check for valid input.
    end_func_index = static_cast<uint32_t>(end.GetLineNumber());

    // Special case: Stop before the start of the next function. Change to: Stop
    // at the end of the function before, such that we don't disassemble the
    // next function also.
    if (end.GetColumnNumber() == 0 && end_func_index > 0) {
      --end_func_index;
      end_offset = functions[end_func_index].code_end_offset;
    } else {
      if (end_func_index >= functions.size()) return false;
      end_offset =
          functions[end_func_index].code_start_offset + end.GetColumnNumber();
      if (end_offset > functions[end_func_index].code_end_offset) return false;
    }
  }

  AccountingAllocator alloc;
  Zone tmp(&alloc, ZONE_NAME);
  const byte* module_start = module_bytes()->GetChars();

  for (uint32_t func_idx = start_func_index; func_idx <= end_func_index;
       ++func_idx) {
    WasmFunction& func = functions[func_idx];
    if (func.code_start_offset == func.code_end_offset) continue;

    BodyLocalDecls locals(&tmp);
    BytecodeIterator iterator(module_start + func.code_start_offset,
                              module_start + func.code_end_offset, &locals);
    DCHECK_LT(0u, locals.encoded_size);
    for (uint32_t offset : iterator.offsets()) {
      uint32_t total_offset = func.code_start_offset + offset;
      if (total_offset >= end_offset) {
        DCHECK_EQ(end_func_index, func_idx);
        break;
      }
      if (total_offset < start_offset) continue;
      locations->emplace_back(func_idx, offset, debug::kCommonBreakLocation);
    }
  }
  return true;
}

bool WasmCompiledModule::SetBreakPoint(
    Handle<WasmCompiledModule> compiled_module, int* position,
    Handle<Object> break_point_object) {
  Isolate* isolate = compiled_module->GetIsolate();

  // Find the function for this breakpoint.
  int func_index = compiled_module->GetContainingFunction(*position);
  if (func_index < 0) return false;
  WasmFunction& func = compiled_module->module()->functions[func_index];
  int offset_in_func = *position - func.code_start_offset;

  // According to the current design, we should only be called with valid
  // breakable positions.
  DCHECK(IsBreakablePosition(compiled_module, func_index, offset_in_func));

  // Insert new break point into break_positions of shared module data.
  WasmSharedModuleData::AddBreakpoint(compiled_module->shared(), *position,
                                      break_point_object);

  // Iterate over all instances of this module and tell them to set this new
  // breakpoint.
  for (Handle<WasmInstanceObject> instance :
       iterate_compiled_module_instance_chain(isolate, compiled_module)) {
    Handle<WasmDebugInfo> debug_info =
        WasmInstanceObject::GetOrCreateDebugInfo(instance);
    WasmDebugInfo::SetBreakpoint(debug_info, func_index, offset_in_func);
  }

  return true;
}

MaybeHandle<FixedArray> WasmCompiledModule::CheckBreakPoints(int position) {
  Isolate* isolate = GetIsolate();
  if (!shared()->has_breakpoint_infos()) return {};

  Handle<FixedArray> breakpoint_infos(shared()->breakpoint_infos(), isolate);
  int insert_pos =
      FindBreakpointInfoInsertPos(isolate, breakpoint_infos, position);
  if (insert_pos >= breakpoint_infos->length()) return {};

  Handle<Object> maybe_breakpoint_info(breakpoint_infos->get(insert_pos),
                                       isolate);
  if (maybe_breakpoint_info->IsUndefined(isolate)) return {};
  Handle<BreakPointInfo> breakpoint_info =
      Handle<BreakPointInfo>::cast(maybe_breakpoint_info);
  if (breakpoint_info->source_position() != position) return {};

  Handle<Object> breakpoint_objects(breakpoint_info->break_point_objects(),
                                    isolate);
  return isolate->debug()->GetHitBreakPointObjects(breakpoint_objects);
}

Handle<Code> WasmCompiledModule::CompileLazy(
    Isolate* isolate, Handle<WasmInstanceObject> instance, Handle<Code> caller,
    int offset, int func_index, bool patch_caller) {
  isolate->set_context(*instance->compiled_module()->native_context());
  Object* orch_obj =
      instance->compiled_module()->shared()->lazy_compilation_orchestrator();
  LazyCompilationOrchestrator* orch =
      Managed<LazyCompilationOrchestrator>::cast(orch_obj)->get();
  return orch->CompileLazy(isolate, instance, caller, offset, func_index,
                           patch_caller);
}

Handle<WasmInstanceWrapper> WasmInstanceWrapper::New(
    Isolate* isolate, Handle<WasmInstanceObject> instance) {
  Handle<FixedArray> array =
      isolate->factory()->NewFixedArray(kWrapperPropertyCount, TENURED);
  Handle<WasmInstanceWrapper> instance_wrapper(
      reinterpret_cast<WasmInstanceWrapper*>(*array), isolate);
  Handle<WeakCell> cell = isolate->factory()->NewWeakCell(instance);
  instance_wrapper->set(kWrapperInstanceObject, *cell);
  return instance_wrapper;
}

bool WasmInstanceWrapper::IsWasmInstanceWrapper(Object* obj) {
  if (!obj->IsFixedArray()) return false;
  Handle<FixedArray> array = handle(FixedArray::cast(obj));
  if (array->length() != kWrapperPropertyCount) return false;
  if (!array->get(kWrapperInstanceObject)->IsWeakCell()) return false;
  Isolate* isolate = array->GetIsolate();
  if (!array->get(kNextInstanceWrapper)->IsUndefined(isolate) &&
      !array->get(kNextInstanceWrapper)->IsFixedArray())
    return false;
  if (!array->get(kPreviousInstanceWrapper)->IsUndefined(isolate) &&
      !array->get(kPreviousInstanceWrapper)->IsFixedArray())
    return false;
  return true;
}

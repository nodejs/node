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
#include "src/objects/debug-objects-inl.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-code-specialization.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-memory.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-text.h"

#define TRACE(...)                                      \
  do {                                                  \
    if (FLAG_trace_wasm_instances) PrintF(__VA_ARGS__); \
  } while (false)

#define TRACE_IFT(...)              \
  do {                              \
    if (false) PrintF(__VA_ARGS__); \
  } while (false)

namespace v8 {
namespace internal {

// Import a few often used types from the wasm namespace.
using WasmFunction = wasm::WasmFunction;
using WasmModule = wasm::WasmModule;

namespace {

// Manages the natively-allocated memory for a WasmInstanceObject. Since
// an instance finalizer is not guaranteed to run upon isolate shutdown,
// we must use a Managed<WasmInstanceNativeAllocations> to guarantee
// it is freed.
// Native allocations are the signature ids and targets for indirect call
// targets, as well as the call targets for imported functions.
class WasmInstanceNativeAllocations {
 public:
// Helper macro to set an internal field and the corresponding field
// on an instance.
#define SET(instance, field, value) \
  {                                 \
    auto v = value;                 \
    this->field##_ = v;             \
    instance->set_##field(v);       \
  }

  // Allocates initial native storage for a given instance.
  WasmInstanceNativeAllocations(Handle<WasmInstanceObject> instance,
                                size_t num_imported_functions) {
    SET(instance, imported_function_targets,
        reinterpret_cast<Address*>(
            calloc(num_imported_functions, sizeof(Address))));
  }
  ~WasmInstanceNativeAllocations() { free(); }
  // Frees natively-allocated storage.
  void free() {
    ::free(indirect_function_table_sig_ids_);
    ::free(indirect_function_table_targets_);
    ::free(imported_function_targets_);
    indirect_function_table_sig_ids_ = nullptr;
    indirect_function_table_targets_ = nullptr;
    imported_function_targets_ = nullptr;
  }
  // Resizes the indirect function table.
  void resize_indirect_function_table(Isolate* isolate,
                                      Handle<WasmInstanceObject> instance,
                                      uint32_t new_size) {
    uint32_t old_size = instance->indirect_function_table_size();
    void* new_sig_ids = nullptr;
    void* new_targets = nullptr;
    Handle<FixedArray> new_instances;
    if (indirect_function_table_sig_ids_) {
      // Reallocate the old storage.
      new_sig_ids = realloc(indirect_function_table_sig_ids_,
                            new_size * sizeof(uint32_t));
      new_targets =
          realloc(indirect_function_table_targets_, new_size * sizeof(Address));

      Handle<FixedArray> old(instance->indirect_function_table_instances(),
                             isolate);
      new_instances = isolate->factory()->CopyFixedArrayAndGrow(
          old, static_cast<int>(new_size - old_size));
    } else {
      // Allocate new storage.
      new_sig_ids = malloc(new_size * sizeof(uint32_t));
      new_targets = malloc(new_size * sizeof(Address));
      new_instances =
          isolate->factory()->NewFixedArray(static_cast<int>(new_size));
    }
    // Initialize new entries.
    instance->set_indirect_function_table_size(new_size);
    SET(instance, indirect_function_table_sig_ids,
        reinterpret_cast<uint32_t*>(new_sig_ids));
    SET(instance, indirect_function_table_targets,
        reinterpret_cast<Address*>(new_targets));

    instance->set_indirect_function_table_instances(*new_instances);
    for (uint32_t j = old_size; j < new_size; j++) {
      IndirectFunctionTableEntry(*instance, static_cast<int>(j)).clear();
    }
  }
  uint32_t* indirect_function_table_sig_ids_ = nullptr;
  Address* indirect_function_table_targets_ = nullptr;
  Address* imported_function_targets_ = nullptr;
#undef SET
};

WasmInstanceNativeAllocations* GetNativeAllocations(
    WasmInstanceObject* instance) {
  return reinterpret_cast<Managed<WasmInstanceNativeAllocations>*>(
             instance->managed_native_allocations())
      ->get();
}

// An iterator that returns first the module itself, then all modules linked via
// next, then all linked via prev.
class CompiledModulesIterator
    : public v8::base::iterator<std::input_iterator_tag,
                                Handle<WasmCompiledModule>> {
 public:
  CompiledModulesIterator(Isolate* isolate,
                          Handle<WasmCompiledModule> start_module, bool at_end)
      : isolate_(isolate),
        start_module_(start_module),
        current_(
            at_end ? Handle<WasmCompiledModule>::null()
                   : Handle<WasmCompiledModule>::New(*start_module, isolate)) {}

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
      if (current_->has_next_instance()) {
        *current_.location() = current_->next_instance();
        return;
      }
      // No more modules in next-links, now try the previous-links.
      is_backwards_ = true;
      current_ = start_module_;
    }
    if (current_->has_prev_instance()) {
      *current_.location() = current_->prev_instance();
      return;
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
    : public v8::base::iterator<std::input_iterator_tag,
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
    return !it.current_.is_null() && !it.current_->has_instance();
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
bool IsBreakablePosition(WasmSharedModuleData* shared, int func_index,
                         int offset_in_func) {
  DisallowHeapAllocation no_gc;
  AccountingAllocator alloc;
  Zone tmp(&alloc, ZONE_NAME);
  wasm::BodyLocalDecls locals(&tmp);
  const byte* module_start = shared->module_bytes()->GetChars();
  WasmFunction& func = shared->module()->functions[func_index];
  wasm::BytecodeIterator iterator(module_start + func.code.offset(),
                                  module_start + func.code.end_offset(),
                                  &locals);
  DCHECK_LT(0, locals.encoded_size);
  for (uint32_t offset : iterator.offsets()) {
    if (offset > static_cast<uint32_t>(offset_in_func)) break;
    if (offset == static_cast<uint32_t>(offset_in_func)) return true;
  }
  return false;
}
#endif  // DEBUG

enum DispatchTableElements : int {
  kDispatchTableInstanceOffset,
  kDispatchTableIndexOffset,
  kDispatchTableFunctionTableOffset,
  // Marker:
  kDispatchTableNumElements
};

}  // namespace

Handle<WasmModuleObject> WasmModuleObject::New(
    Isolate* isolate, Handle<WasmCompiledModule> compiled_module) {
  Handle<JSFunction> module_cons(
      isolate->native_context()->wasm_module_constructor());
  auto module_object = Handle<WasmModuleObject>::cast(
      isolate->factory()->NewJSObject(module_cons));
  module_object->set_compiled_module(*compiled_module);
  Handle<WeakCell> link_to_module =
      isolate->factory()->NewWeakCell(module_object);
  compiled_module->set_weak_wasm_module(*link_to_module);

  compiled_module->LogWasmCodes(isolate);
  return module_object;
}

void WasmModuleObject::ValidateStateForTesting(
    Isolate* isolate, Handle<WasmModuleObject> module_obj) {
  DisallowHeapAllocation no_gc;
  WasmCompiledModule* compiled_module = module_obj->compiled_module();
  CHECK(compiled_module->has_weak_wasm_module());
  CHECK_EQ(compiled_module->weak_wasm_module()->value(), *module_obj);
  CHECK(!compiled_module->has_prev_instance());
  CHECK(!compiled_module->has_next_instance());
  CHECK(!compiled_module->has_instance());
}

Handle<WasmTableObject> WasmTableObject::New(Isolate* isolate, uint32_t initial,
                                             int64_t maximum,
                                             Handle<FixedArray>* js_functions) {
  Handle<JSFunction> table_ctor(
      isolate->native_context()->wasm_table_constructor());
  auto table_obj = Handle<WasmTableObject>::cast(
      isolate->factory()->NewJSObject(table_ctor));

  *js_functions = isolate->factory()->NewFixedArray(initial);
  Object* null = isolate->heap()->null_value();
  for (int i = 0; i < static_cast<int>(initial); ++i) {
    (*js_functions)->set(i, null);
  }
  table_obj->set_functions(**js_functions);
  DCHECK_EQ(maximum, static_cast<int>(maximum));
  Handle<Object> max = isolate->factory()->NewNumber(maximum);
  table_obj->set_maximum_length(*max);

  table_obj->set_dispatch_tables(isolate->heap()->empty_fixed_array());
  return Handle<WasmTableObject>::cast(table_obj);
}

void WasmTableObject::AddDispatchTable(Isolate* isolate,
                                       Handle<WasmTableObject> table_obj,
                                       Handle<WasmInstanceObject> instance,
                                       int table_index) {
  Handle<FixedArray> dispatch_tables(table_obj->dispatch_tables());
  int old_length = dispatch_tables->length();
  DCHECK_EQ(0, old_length % kDispatchTableNumElements);

  if (instance.is_null()) return;
  // TODO(titzer): use weak cells here to avoid leaking instances.

  // Grow the dispatch table and add a new entry at the end.
  Handle<FixedArray> new_dispatch_tables =
      isolate->factory()->CopyFixedArrayAndGrow(dispatch_tables,
                                                kDispatchTableNumElements);

  new_dispatch_tables->set(old_length + kDispatchTableInstanceOffset,
                           *instance);
  new_dispatch_tables->set(old_length + kDispatchTableIndexOffset,
                           Smi::FromInt(table_index));

  table_obj->set_dispatch_tables(*new_dispatch_tables);
}

void WasmTableObject::Grow(Isolate* isolate, uint32_t count) {
  if (count == 0) return;  // Degenerate case: nothing to do.

  Handle<FixedArray> dispatch_tables(this->dispatch_tables());
  DCHECK_EQ(0, dispatch_tables->length() % kDispatchTableNumElements);
  uint32_t old_size = functions()->length();

  // Tables are stored in the instance object, no code patching is
  // necessary. We simply have to grow the raw tables in each instance
  // that has imported this table.

  // TODO(titzer): replace the dispatch table with a weak list of all
  // the instances that import a given table.
  for (int i = 0; i < dispatch_tables->length();
       i += kDispatchTableNumElements) {
    Handle<WasmInstanceObject> instance(
        WasmInstanceObject::cast(dispatch_tables->get(i)), isolate);
    DCHECK_EQ(old_size, instance->indirect_function_table_size());
    uint32_t new_size = old_size + count;
    WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(instance,
                                                                   new_size);
  }
}

void WasmTableObject::Set(Isolate* isolate, Handle<WasmTableObject> table,
                          int32_t table_index, Handle<JSFunction> function) {
  Handle<FixedArray> array(table->functions(), isolate);
  if (function.is_null()) {
    ClearDispatchTables(table, table_index);  // Degenerate case of null value.
    array->set(table_index, isolate->heap()->null_value());
    return;
  }

  // TODO(titzer): Change this to MaybeHandle<WasmExportedFunction>
  DCHECK(WasmExportedFunction::IsWasmExportedFunction(*function));
  auto exported_function = Handle<WasmExportedFunction>::cast(function);
  Handle<WasmInstanceObject> other_instance(exported_function->instance());
  int func_index = exported_function->function_index();
  auto* wasm_function = &other_instance->module()->functions[func_index];
  DCHECK_NOT_NULL(wasm_function);
  DCHECK_NOT_NULL(wasm_function->sig);
  wasm::WasmCode* wasm_code = exported_function->GetWasmCode();
  UpdateDispatchTables(isolate, table, table_index, wasm_function->sig,
                       handle(exported_function->instance()), wasm_code);
  array->set(table_index, *function);
}

void WasmTableObject::UpdateDispatchTables(
    Isolate* isolate, Handle<WasmTableObject> table, int table_index,
    wasm::FunctionSig* sig, Handle<WasmInstanceObject> from_instance,
    wasm::WasmCode* wasm_code) {
  // We simply need to update the IFTs for each instance that imports
  // this table.
  DisallowHeapAllocation no_gc;
  FixedArray* dispatch_tables = table->dispatch_tables();
  DCHECK_EQ(0, dispatch_tables->length() % kDispatchTableNumElements);

  for (int i = 0; i < dispatch_tables->length();
       i += kDispatchTableNumElements) {
    // Note that {SignatureMap::Find} may return {-1} if the signature is
    // not found; it will simply never match any check.
    WasmInstanceObject* to_instance = WasmInstanceObject::cast(
        dispatch_tables->get(i + kDispatchTableInstanceOffset));
    auto sig_id = to_instance->module()->signature_map.Find(sig);
    IndirectFunctionTableEntry(to_instance, table_index)
        .set(sig_id, *from_instance, wasm_code);
  }
}

void WasmTableObject::ClearDispatchTables(Handle<WasmTableObject> table,
                                          int index) {
  DisallowHeapAllocation no_gc;
  FixedArray* dispatch_tables = table->dispatch_tables();
  DCHECK_EQ(0, dispatch_tables->length() % kDispatchTableNumElements);
  for (int i = 0; i < dispatch_tables->length();
       i += kDispatchTableNumElements) {
    WasmInstanceObject* target_instance = WasmInstanceObject::cast(
        dispatch_tables->get(i + kDispatchTableInstanceOffset));
    DCHECK_LT(index, target_instance->indirect_function_table_size());
    IndirectFunctionTableEntry(target_instance, index).clear();
  }
}

namespace {
MaybeHandle<JSArrayBuffer> GrowMemoryBuffer(Isolate* isolate,
                                            Handle<JSArrayBuffer> old_buffer,
                                            uint32_t pages,
                                            uint32_t maximum_pages,
                                            bool use_trap_handler) {
  if (!old_buffer->is_growable()) return {};
  Address old_mem_start = nullptr;
  uint32_t old_size = 0;
  if (!old_buffer.is_null()) {
    old_mem_start = static_cast<Address>(old_buffer->backing_store());
    CHECK(old_buffer->byte_length()->ToUint32(&old_size));
  }
  DCHECK_EQ(0, old_size % wasm::kWasmPageSize);
  uint32_t old_pages = old_size / wasm::kWasmPageSize;
  DCHECK_GE(std::numeric_limits<uint32_t>::max(),
            old_size + pages * wasm::kWasmPageSize);
  if (old_pages > maximum_pages || pages > maximum_pages - old_pages) return {};
  size_t new_size =
      static_cast<size_t>(old_pages + pages) * wasm::kWasmPageSize;
  if (new_size > FLAG_wasm_max_mem_pages * wasm::kWasmPageSize ||
      new_size > kMaxInt) {
    return {};
  }
  // Reusing the backing store from externalized buffers causes problems with
  // Blink's array buffers. The connection between the two is lost, which can
  // lead to Blink not knowing about the other reference to the buffer and
  // freeing it too early.
  if (!old_buffer->is_external() && old_size != 0 &&
      ((new_size < old_buffer->allocation_length()) || old_size == new_size)) {
    DCHECK_NOT_NULL(old_buffer->backing_store());
    if (old_size != new_size) {
      // If adjusting permissions fails, propagate error back to return
      // failure to grow.
      DCHECK(!isolate->wasm_engine()->memory_tracker()->IsEmptyBackingStore(
          old_mem_start));
      if (!i::SetPermissions(old_mem_start, new_size,
                             PageAllocator::kReadWrite)) {
        return {};
      }
      reinterpret_cast<v8::Isolate*>(isolate)
          ->AdjustAmountOfExternalAllocatedMemory(pages * wasm::kWasmPageSize);
    }
    // NOTE: We must allocate a new array buffer here because the spec
    // assumes that ArrayBuffers do not change size.
    void* backing_store = old_buffer->backing_store();
    bool is_external = old_buffer->is_external();
    // Disconnect buffer early so GC won't free it.
    i::wasm::DetachMemoryBuffer(isolate, old_buffer, false);
    Handle<JSArrayBuffer> new_buffer =
        wasm::SetupArrayBuffer(isolate, backing_store, new_size, is_external);
    return new_buffer;
  } else {
    // We couldn't reuse the old backing store, so create a new one and copy the
    // old contents in.
    Handle<JSArrayBuffer> new_buffer;
    if (!wasm::NewArrayBuffer(isolate, new_size, use_trap_handler)
             .ToHandle(&new_buffer)) {
      return {};
    }
    if (old_size == 0) return new_buffer;
    Address new_mem_start = static_cast<Address>(new_buffer->backing_store());
    memcpy(new_mem_start, old_mem_start, old_size);
    DCHECK(old_buffer.is_null() || !old_buffer->is_shared());
    constexpr bool free_memory = true;
    i::wasm::DetachMemoryBuffer(isolate, old_buffer, free_memory);
    return new_buffer;
  }
}

// May GC, because SetSpecializationMemInfoFrom may GC
void SetInstanceMemory(Isolate* isolate, Handle<WasmInstanceObject> instance,
                       Handle<JSArrayBuffer> buffer) {
  instance->SetRawMemory(reinterpret_cast<byte*>(buffer->backing_store()),
                         buffer->byte_length()->Number());
#if DEBUG
  // To flush out bugs earlier, in DEBUG mode, check that all pages of the
  // memory are accessible by reading and writing one byte on each page.
  byte* mem_start = instance->memory_start();
  uintptr_t mem_size = instance->memory_size();
  for (uint32_t offset = 0; offset < mem_size; offset += wasm::kWasmPageSize) {
    byte val = mem_start[offset];
    USE(val);
    mem_start[offset] = val;
  }
#endif
}

}  // namespace

Handle<WasmMemoryObject> WasmMemoryObject::New(
    Isolate* isolate, MaybeHandle<JSArrayBuffer> maybe_buffer,
    int32_t maximum) {
  // TODO(kschimpf): Do we need to add an argument that defines the
  // style of memory the user prefers (with/without trap handling), so
  // that the memory will match the style of the compiled wasm module.
  // See issue v8:7143
  Handle<JSFunction> memory_ctor(
      isolate->native_context()->wasm_memory_constructor());
  auto memory_obj = Handle<WasmMemoryObject>::cast(
      isolate->factory()->NewJSObject(memory_ctor, TENURED));

  Handle<JSArrayBuffer> buffer;
  if (maybe_buffer.is_null()) {
    // If no buffer was provided, create a 0-length one.
    buffer = wasm::SetupArrayBuffer(isolate, nullptr, 0, false);
  } else {
    buffer = maybe_buffer.ToHandleChecked();
    // Paranoid check that the buffer size makes sense.
    uint32_t mem_size = 0;
    CHECK(buffer->byte_length()->ToUint32(&mem_size));
  }
  memory_obj->set_array_buffer(*buffer);
  memory_obj->set_maximum_pages(maximum);

  return memory_obj;
}

uint32_t WasmMemoryObject::current_pages() {
  uint32_t byte_length;
  CHECK(array_buffer()->byte_length()->ToUint32(&byte_length));
  return byte_length / wasm::kWasmPageSize;
}

void WasmMemoryObject::AddInstance(Isolate* isolate,
                                   Handle<WasmMemoryObject> memory,
                                   Handle<WasmInstanceObject> instance) {
  Handle<FixedArrayOfWeakCells> old_instances =
      memory->has_instances()
          ? Handle<FixedArrayOfWeakCells>(memory->instances(), isolate)
          : Handle<FixedArrayOfWeakCells>::null();
  Handle<FixedArrayOfWeakCells> new_instances =
      FixedArrayOfWeakCells::Add(old_instances, instance);
  memory->set_instances(*new_instances);
  Handle<JSArrayBuffer> buffer(memory->array_buffer(), isolate);
  SetInstanceMemory(isolate, instance, buffer);
}

void WasmMemoryObject::RemoveInstance(Isolate* isolate,
                                      Handle<WasmMemoryObject> memory,
                                      Handle<WasmInstanceObject> instance) {
  if (memory->has_instances()) {
    memory->instances()->Remove(instance);
  }
}

// static
int32_t WasmMemoryObject::Grow(Isolate* isolate,
                               Handle<WasmMemoryObject> memory_object,
                               uint32_t pages) {
  Handle<JSArrayBuffer> old_buffer(memory_object->array_buffer());
  if (!old_buffer->is_growable()) return -1;
  uint32_t old_size = 0;
  CHECK(old_buffer->byte_length()->ToUint32(&old_size));
  DCHECK_EQ(0, old_size % wasm::kWasmPageSize);
  Handle<JSArrayBuffer> new_buffer;

  uint32_t maximum_pages = FLAG_wasm_max_mem_pages;
  if (memory_object->has_maximum_pages()) {
    maximum_pages = Min(FLAG_wasm_max_mem_pages,
                        static_cast<uint32_t>(memory_object->maximum_pages()));
  }
  // TODO(kschimpf): We need to fix this by adding a field to WasmMemoryObject
  // that defines the style of memory being used.
  if (!GrowMemoryBuffer(isolate, old_buffer, pages, maximum_pages,
                        trap_handler::IsTrapHandlerEnabled())
           .ToHandle(&new_buffer)) {
    return -1;
  }

  if (memory_object->has_instances()) {
    Handle<FixedArrayOfWeakCells> instances(memory_object->instances(),
                                            isolate);
    for (int i = 0; i < instances->Length(); i++) {
      Object* elem = instances->Get(i);
      if (!elem->IsWasmInstanceObject()) continue;
      Handle<WasmInstanceObject> instance(WasmInstanceObject::cast(elem),
                                          isolate);
      SetInstanceMemory(isolate, instance, new_buffer);
    }
  }
  memory_object->set_array_buffer(*new_buffer);
  return old_size / wasm::kWasmPageSize;
}

// static
MaybeHandle<WasmGlobalObject> WasmGlobalObject::New(
    Isolate* isolate, MaybeHandle<JSArrayBuffer> maybe_buffer,
    wasm::ValueType type, int32_t offset, bool is_mutable) {
  Handle<JSFunction> global_ctor(
      isolate->native_context()->wasm_global_constructor());
  auto global_obj = Handle<WasmGlobalObject>::cast(
      isolate->factory()->NewJSObject(global_ctor));

  uint32_t type_size = TypeSize(type);

  Handle<JSArrayBuffer> buffer;
  if (!maybe_buffer.ToHandle(&buffer)) {
    // If no buffer was provided, create one long enough for the given type.
    buffer =
        isolate->factory()->NewJSArrayBuffer(SharedFlag::kNotShared, TENURED);

    const bool initialize = true;
    if (!JSArrayBuffer::SetupAllocatingData(buffer, isolate, type_size,
                                            initialize)) {
      return {};
    }
  }

  // Check that the offset is in bounds.
  uint32_t buffer_size = 0;
  CHECK(buffer->byte_length()->ToUint32(&buffer_size));
  CHECK(offset + type_size <= buffer_size);

  global_obj->set_array_buffer(*buffer);
  global_obj->set_flags(0);
  global_obj->set_type(type);
  global_obj->set_offset(offset);
  global_obj->set_is_mutable(is_mutable);

  return global_obj;
}

void IndirectFunctionTableEntry::clear() {
  instance_->indirect_function_table_sig_ids()[index_] = -1;
  instance_->indirect_function_table_targets()[index_] = 0;
  instance_->indirect_function_table_instances()->set(
      index_, instance_->GetIsolate()->heap()->undefined_value());
}

void IndirectFunctionTableEntry::set(int sig_id, WasmInstanceObject* instance,
                                     const wasm::WasmCode* wasm_code) {
  TRACE_IFT("IFT entry %p[%d] = {sig_id=%d, instance=%p, target=%p}\n",
            instance_, index_, sig_id, instance,
            wasm_code->instructions().start());
  instance_->indirect_function_table_sig_ids()[index_] = sig_id;
  instance_->indirect_function_table_targets()[index_] =
      wasm_code->instructions().start();
  instance_->indirect_function_table_instances()->set(index_, instance);
}

WasmInstanceObject* IndirectFunctionTableEntry::instance() {
  return WasmInstanceObject::cast(
      instance_->indirect_function_table_instances()->get(index_));
}

int IndirectFunctionTableEntry::sig_id() {
  return instance_->indirect_function_table_sig_ids()[index_];
}

Address IndirectFunctionTableEntry::target() {
  return instance_->indirect_function_table_targets()[index_];
}

void ImportedFunctionEntry::set(JSReceiver* callable,
                                const wasm::WasmCode* wasm_to_js_wrapper) {
  TRACE_IFT("Import callable %p[%d] = {callable=%p, target=%p}\n", instance_,
            index_, callable, wasm_to_js_wrapper->instructions().start());
  DCHECK_EQ(wasm::WasmCode::kWasmToJsWrapper, wasm_to_js_wrapper->kind());
  instance_->imported_function_instances()->set(index_, instance_);
  instance_->imported_function_callables()->set(index_, callable);
  instance_->imported_function_targets()[index_] =
      wasm_to_js_wrapper->instructions().start();
}

void ImportedFunctionEntry::set(WasmInstanceObject* instance,
                                const wasm::WasmCode* wasm_code) {
  TRACE_IFT("Import WASM %p[%d] = {instance=%p, target=%p}\n", instance_,
            index_, instance, wasm_code->instructions().start());
  instance_->imported_function_instances()->set(index_, instance);
  instance_->imported_function_callables()->set(
      index_, instance_->GetHeap()->undefined_value());
  instance_->imported_function_targets()[index_] =
      wasm_code->instructions().start();
}

WasmInstanceObject* ImportedFunctionEntry::instance() {
  return WasmInstanceObject::cast(
      instance_->imported_function_instances()->get(index_));
}

JSReceiver* ImportedFunctionEntry::callable() {
  return JSReceiver::cast(
      instance_->imported_function_callables()->get(index_));
}

Address ImportedFunctionEntry::target() {
  return instance_->imported_function_targets()[index_];
}

bool ImportedFunctionEntry::is_js_receiver_entry() {
  return instance_->imported_function_callables()->get(index_)->IsJSReceiver();
}

bool WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(
    Handle<WasmInstanceObject> instance, uint32_t minimum_size) {
  uint32_t old_size = instance->indirect_function_table_size();
  if (old_size >= minimum_size) return false;  // Nothing to do.

  Isolate* isolate = instance->GetIsolate();
  HandleScope scope(isolate);
  auto native_allocations = GetNativeAllocations(*instance);
  native_allocations->resize_indirect_function_table(isolate, instance,
                                                     minimum_size);
  return true;
}

void WasmInstanceObject::SetRawMemory(byte* mem_start, uint32_t mem_size) {
  DCHECK_LE(mem_size, wasm::kV8MaxWasmMemoryPages * wasm::kWasmPageSize);
  uint32_t mem_size64 = mem_size;
  uint32_t mem_mask64 = base::bits::RoundUpToPowerOfTwo32(mem_size) - 1;
  DCHECK_LE(mem_size, mem_mask64 + 1);
  set_memory_start(mem_start);
  set_memory_size(mem_size64);
  set_memory_mask(mem_mask64);
}

WasmModuleObject* WasmInstanceObject::module_object() {
  return compiled_module()->wasm_module();
}

WasmModule* WasmInstanceObject::module() {
  return compiled_module()->shared()->module();
}

Handle<WasmDebugInfo> WasmInstanceObject::GetOrCreateDebugInfo(
    Handle<WasmInstanceObject> instance) {
  if (instance->has_debug_info()) return handle(instance->debug_info());
  Handle<WasmDebugInfo> new_info = WasmDebugInfo::New(instance);
  DCHECK(instance->has_debug_info());
  return new_info;
}

Handle<WasmInstanceObject> WasmInstanceObject::New(
    Isolate* isolate, Handle<WasmCompiledModule> compiled_module) {
  Handle<JSFunction> instance_cons(
      isolate->native_context()->wasm_instance_constructor());
  Handle<JSObject> instance_object =
      isolate->factory()->NewJSObject(instance_cons, TENURED);

  Handle<WasmInstanceObject> instance(
      reinterpret_cast<WasmInstanceObject*>(*instance_object), isolate);

  // Initialize the imported function arrays.
  auto num_imported_functions =
      compiled_module->shared()->module()->num_imported_functions;
  auto native_allocations = Managed<WasmInstanceNativeAllocations>::Allocate(
      isolate, instance, num_imported_functions);
  instance->set_managed_native_allocations(*native_allocations);

  Handle<FixedArray> imported_function_instances =
      isolate->factory()->NewFixedArray(num_imported_functions);

  instance->set_imported_function_instances(*imported_function_instances);
  Handle<FixedArray> imported_function_callables =
      isolate->factory()->NewFixedArray(num_imported_functions);

  instance->set_imported_function_callables(*imported_function_callables);

  instance->SetRawMemory(nullptr, 0);
  instance->set_globals_start(nullptr);
  instance->set_indirect_function_table_size(0);
  instance->set_indirect_function_table_sig_ids(nullptr);
  instance->set_indirect_function_table_targets(nullptr);
  instance->set_compiled_module(*compiled_module);

  return instance;
}

WasmInstanceObject* WasmInstanceObject::GetOwningInstance(
    const wasm::WasmCode* code) {
  DisallowHeapAllocation no_gc;
  Object* weak_link = nullptr;
  DCHECK(code->kind() == wasm::WasmCode::kFunction ||
         code->kind() == wasm::WasmCode::kInterpreterStub);
  weak_link = code->native_module()->compiled_module()->weak_owning_instance();
  DCHECK(weak_link->IsWeakCell());
  WeakCell* cell = WeakCell::cast(weak_link);
  if (cell->cleared()) return nullptr;
  return WasmInstanceObject::cast(cell->value());
}

void WasmInstanceObject::ValidateInstancesChainForTesting(
    Isolate* isolate, Handle<WasmModuleObject> module_obj, int instance_count) {
  CHECK_GE(instance_count, 0);
  DisallowHeapAllocation no_gc;
  WasmCompiledModule* compiled_module = module_obj->compiled_module();
  CHECK_EQ(JSObject::cast(compiled_module->weak_wasm_module()->value()),
           *module_obj);
  Object* prev = nullptr;
  int found_instances = compiled_module->has_instance() ? 1 : 0;
  WasmCompiledModule* current_instance = compiled_module;
  while (current_instance->has_next_instance()) {
    CHECK((prev == nullptr && !current_instance->has_prev_instance()) ||
          current_instance->prev_instance() == prev);
    CHECK_EQ(current_instance->weak_wasm_module()->value(), *module_obj);
    CHECK(current_instance->weak_owning_instance()
              ->value()
              ->IsWasmInstanceObject());
    prev = current_instance;
    current_instance =
        WasmCompiledModule::cast(current_instance->next_instance());
    ++found_instances;
    CHECK_LE(found_instances, instance_count);
  }
  CHECK_EQ(found_instances, instance_count);
}

void WasmInstanceObject::ValidateOrphanedInstanceForTesting(
    Isolate* isolate, Handle<WasmInstanceObject> instance) {
  DisallowHeapAllocation no_gc;
  WasmCompiledModule* compiled_module = instance->compiled_module();
  CHECK(compiled_module->has_weak_wasm_module());
  CHECK(compiled_module->weak_wasm_module()->cleared());
}

namespace {
void InstanceFinalizer(const v8::WeakCallbackInfo<void>& data) {
  DisallowHeapAllocation no_gc;
  JSObject** p = reinterpret_cast<JSObject**>(data.GetParameter());
  WasmInstanceObject* instance = reinterpret_cast<WasmInstanceObject*>(*p);
  Isolate* isolate = reinterpret_cast<Isolate*>(data.GetIsolate());
  // If a link to shared memory instances exists, update the list of memory
  // instances before the instance is destroyed.
  WasmCompiledModule* compiled_module = instance->compiled_module();
  wasm::NativeModule* native_module = compiled_module->GetNativeModule();
  if (native_module) {
    TRACE("Finalizing %zu {\n", native_module->instance_id);
  } else {
    TRACE("Finalized already cleaned up compiled module\n");
  }
  WeakCell* weak_wasm_module = compiled_module->weak_wasm_module();

  // Since the order of finalizers is not guaranteed, it can be the case
  // that {instance->compiled_module()->module()}, which is a
  // {Managed<WasmModule>} has been collected earlier in this GC cycle.
  // Weak references to this instance won't be cleared until
  // the next GC cycle, so we need to manually break some links (such as
  // the weak references from {WasmMemoryObject::instances}.
  if (instance->has_memory_object()) {
    WasmMemoryObject::RemoveInstance(isolate, handle(instance->memory_object()),
                                     handle(instance));
  }

  // weak_wasm_module may have been cleared, meaning the module object
  // was GC-ed. We still want to maintain the links between instances, to
  // release the WasmCompiledModule corresponding to the WasmModuleInstance
  // being finalized here.
  WasmModuleObject* wasm_module = nullptr;
  if (!weak_wasm_module->cleared()) {
    wasm_module = WasmModuleObject::cast(weak_wasm_module->value());
    WasmCompiledModule* current_template = wasm_module->compiled_module();

    DCHECK(!current_template->has_prev_instance());
    if (current_template == compiled_module) {
      if (!compiled_module->has_next_instance()) {
        WasmCompiledModule::Reset(isolate, compiled_module);
      } else {
        WasmModuleObject::cast(wasm_module)
            ->set_compiled_module(compiled_module->next_instance());
      }
    }
  }

  // Free raw C++ memory associated with the instance.
  GetNativeAllocations(instance)->free();

  compiled_module->RemoveFromChain();

  GlobalHandles::Destroy(reinterpret_cast<Object**>(p));
  TRACE("}\n");
}

}  // namespace

void WasmInstanceObject::InstallFinalizer(Isolate* isolate,
                                          Handle<WasmInstanceObject> instance) {
  Handle<Object> global_handle = isolate->global_handles()->Create(*instance);
  GlobalHandles::MakeWeak(global_handle.location(), global_handle.location(),
                          InstanceFinalizer, v8::WeakCallbackType::kFinalizer);
}

bool WasmExportedFunction::IsWasmExportedFunction(Object* object) {
  if (!object->IsJSFunction()) return false;
  Handle<JSFunction> js_function(JSFunction::cast(object));
  if (Code::JS_TO_WASM_FUNCTION != js_function->code()->kind()) return false;

#ifdef DEBUG
  // Any function having code of {JS_TO_WASM_FUNCTION} kind must be an exported
  // function and hence will have a property holding the instance object.
  Handle<Symbol> symbol(
      js_function->GetIsolate()->factory()->wasm_instance_symbol());
  MaybeHandle<Object> result =
      JSObject::GetPropertyOrElement(js_function, symbol);
  DCHECK(result.ToHandleChecked()->IsWasmInstanceObject());
#endif

  return true;
}

WasmExportedFunction* WasmExportedFunction::cast(Object* object) {
  DCHECK(IsWasmExportedFunction(object));
  return reinterpret_cast<WasmExportedFunction*>(object);
}

WasmInstanceObject* WasmExportedFunction::instance() {
  DisallowHeapAllocation no_allocation;
  Handle<Symbol> symbol(GetIsolate()->factory()->wasm_instance_symbol());
  MaybeHandle<Object> result =
      JSObject::GetPropertyOrElement(handle(this), symbol);
  return WasmInstanceObject::cast(*(result.ToHandleChecked()));
}

int WasmExportedFunction::function_index() {
  DisallowHeapAllocation no_allocation;
  Handle<Symbol> symbol = GetIsolate()->factory()->wasm_function_index_symbol();
  MaybeHandle<Object> result =
      JSObject::GetPropertyOrElement(handle(this), symbol);
  return result.ToHandleChecked()->Number();
}

Handle<WasmExportedFunction> WasmExportedFunction::New(
    Isolate* isolate, Handle<WasmInstanceObject> instance,
    MaybeHandle<String> maybe_name, int func_index, int arity,
    Handle<Code> export_wrapper) {
  DCHECK_EQ(Code::JS_TO_WASM_FUNCTION, export_wrapper->kind());
  Handle<String> name;
  if (!maybe_name.ToHandle(&name)) {
    EmbeddedVector<char, 16> buffer;
    int length = SNPrintF(buffer, "%d", func_index);
    name = isolate->factory()
               ->NewStringFromOneByte(
                   Vector<uint8_t>::cast(buffer.SubVector(0, length)))
               .ToHandleChecked();
  }
  NewFunctionArgs args = NewFunctionArgs::ForWasm(
      name, export_wrapper, isolate->sloppy_function_without_prototype_map());
  Handle<JSFunction> js_function = isolate->factory()->NewFunction(args);
  // According to the spec, exported functions should not have a [[Construct]]
  // method.
  DCHECK(!js_function->IsConstructor());
  js_function->shared()->set_length(arity);
  js_function->shared()->set_internal_formal_parameter_count(arity);

  Handle<Symbol> instance_symbol(isolate->factory()->wasm_instance_symbol());
  JSObject::AddProperty(js_function, instance_symbol, instance, DONT_ENUM);

  Handle<Symbol> function_index_symbol(
      isolate->factory()->wasm_function_index_symbol());
  JSObject::AddProperty(js_function, function_index_symbol,
                        isolate->factory()->NewNumber(func_index), DONT_ENUM);

  return Handle<WasmExportedFunction>::cast(js_function);
}

wasm::WasmCode* WasmExportedFunction::GetWasmCode() {
  DisallowHeapAllocation no_gc;
  Handle<Code> export_wrapper_code = handle(this->code());
  DCHECK_EQ(export_wrapper_code->kind(), Code::JS_TO_WASM_FUNCTION);
  int mask = RelocInfo::ModeMask(RelocInfo::JS_TO_WASM_CALL);
  RelocIterator it(*export_wrapper_code, mask);
  DCHECK(!it.done());
  wasm::WasmCode* target =
      GetIsolate()->wasm_engine()->code_manager()->LookupCode(
          it.rinfo()->js_to_wasm_address());
#ifdef DEBUG
  // There should only be this one call to wasm code.
  it.next();
  DCHECK(it.done());
#endif
  return target;
}

WasmModule* WasmSharedModuleData::module() const {
  // We populate the kModuleWrapper field with a Foreign holding the
  // address to the address of a WasmModule. This is because we can
  // handle both cases when the WasmModule's lifetime is managed through
  // a Managed<WasmModule> object, as well as cases when it's managed
  // by the embedder. CcTests fall into the latter case.
  return *(reinterpret_cast<WasmModule**>(
      Foreign::cast(module_wrapper())->foreign_address()));
}

Handle<WasmSharedModuleData> WasmSharedModuleData::New(
    Isolate* isolate, Handle<Foreign> module_wrapper,
    Handle<SeqOneByteString> module_bytes, Handle<Script> script,
    Handle<ByteArray> asm_js_offset_table) {
  Handle<WasmSharedModuleData> data = Handle<WasmSharedModuleData>::cast(
      isolate->factory()->NewStruct(WASM_SHARED_MODULE_DATA_TYPE, TENURED));
  data->set_module_wrapper(*module_wrapper);
  if (!module_bytes.is_null()) {
    data->set_module_bytes(*module_bytes);
  }
  if (!script.is_null()) {
    data->set_script(*script);
  }
  if (!asm_js_offset_table.is_null()) {
    data->set_asm_js_offset_table(*asm_js_offset_table);
  }
  return data;
}

bool WasmSharedModuleData::is_asm_js() {
  bool asm_js = module()->is_asm_js();
  DCHECK_EQ(asm_js, script()->IsUserJavaScript());
  DCHECK_EQ(asm_js, has_asm_js_offset_table());
  return asm_js;
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
                                         Handle<BreakPoint> break_point) {
  Isolate* isolate = shared->GetIsolate();
  Handle<FixedArray> breakpoint_infos;
  if (shared->has_breakpoint_infos()) {
    breakpoint_infos = handle(shared->breakpoint_infos(), isolate);
  } else {
    breakpoint_infos = isolate->factory()->NewFixedArray(4, TENURED);
    shared->set_breakpoint_infos(*breakpoint_infos);
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
    BreakPointInfo::SetBreakPoint(old_info, break_point);
    return;
  }

  // Enlarge break positions array if necessary.
  bool need_realloc = !breakpoint_infos->get(breakpoint_infos->length() - 1)
                           ->IsUndefined(isolate);
  Handle<FixedArray> new_breakpoint_infos = breakpoint_infos;
  if (need_realloc) {
    new_breakpoint_infos = isolate->factory()->NewFixedArray(
        2 * breakpoint_infos->length(), TENURED);
    shared->set_breakpoint_infos(*new_breakpoint_infos);
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
  BreakPointInfo::SetBreakPoint(breakpoint_info, break_point);

  // Now insert new position at insert_pos.
  new_breakpoint_infos->set(insert_pos, *breakpoint_info);
}

void WasmSharedModuleData::SetBreakpointsOnNewInstance(
    Handle<WasmSharedModuleData> shared, Handle<WasmInstanceObject> instance) {
  if (!shared->has_breakpoint_infos()) return;
  Isolate* isolate = shared->GetIsolate();
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
    int func_index = shared->GetContainingFunction(position);
    DCHECK_LE(0, func_index);
    WasmFunction& func = shared->module()->functions[func_index];
    int offset_in_func = position - func.code.offset();
    WasmDebugInfo::SetBreakpoint(debug_info, func_index, offset_in_func);
  }
}

namespace {

enum AsmJsOffsetTableEntryLayout {
  kOTEByteOffset,
  kOTECallPosition,
  kOTENumberConvPosition,
  kOTESize
};

Handle<ByteArray> GetDecodedAsmJsOffsetTable(
    Handle<WasmSharedModuleData> shared, Isolate* isolate) {
  DCHECK(shared->is_asm_js());
  Handle<ByteArray> offset_table(shared->asm_js_offset_table(), isolate);

  // The last byte in the asm_js_offset_tables ByteArray tells whether it is
  // still encoded (0) or decoded (1).
  enum AsmJsTableType : int { Encoded = 0, Decoded = 1 };
  int table_type = offset_table->get(offset_table->length() - 1);
  DCHECK(table_type == Encoded || table_type == Decoded);
  if (table_type == Decoded) return offset_table;

  wasm::AsmJsOffsetsResult asm_offsets;
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
      static_cast<int>(shared->module()->num_imported_functions);
  DCHECK_EQ(shared->module()->functions.size(),
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
  shared->set_asm_js_offset_table(*decoded_table);

  int idx = 0;
  std::vector<WasmFunction>& wasm_funs = shared->module()->functions;
  for (int func = 0; func < num_functions; ++func) {
    std::vector<wasm::AsmJsOffsetEntry>& func_asm_offsets =
        asm_offsets.val[func];
    if (func_asm_offsets.empty()) continue;
    int func_offset = wasm_funs[num_imported_functions + func].code.offset();
    for (wasm::AsmJsOffsetEntry& e : func_asm_offsets) {
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

int WasmSharedModuleData::GetSourcePosition(Handle<WasmSharedModuleData> shared,
                                            uint32_t func_index,
                                            uint32_t byte_offset,
                                            bool is_at_number_conversion) {
  Isolate* isolate = shared->GetIsolate();
  const WasmModule* module = shared->module();

  if (!module->is_asm_js()) {
    // for non-asm.js modules, we just add the function's start offset
    // to make a module-relative position.
    return byte_offset + shared->GetFunctionOffset(func_index);
  }

  // asm.js modules have an additional offset table that must be searched.
  Handle<ByteArray> offset_table = GetDecodedAsmJsOffsetTable(shared, isolate);

  DCHECK_LT(func_index, module->functions.size());
  uint32_t func_code_offset = module->functions[func_index].code.offset();
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

v8::debug::WasmDisassembly WasmSharedModuleData::DisassembleFunction(
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

bool WasmSharedModuleData::GetPossibleBreakpoints(
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
  int start_func_len = functions[start_func_index].code.length();
  if (start.GetColumnNumber() > start_func_len) return false;
  uint32_t start_offset =
      functions[start_func_index].code.offset() + start.GetColumnNumber();
  uint32_t end_func_index;
  uint32_t end_offset;
  if (end.IsEmpty()) {
    // Default: everything till the end of the Script.
    end_func_index = static_cast<uint32_t>(functions.size() - 1);
    end_offset = functions[end_func_index].code.end_offset();
  } else {
    // If end is specified: Use it and check for valid input.
    end_func_index = static_cast<uint32_t>(end.GetLineNumber());

    // Special case: Stop before the start of the next function. Change to: Stop
    // at the end of the function before, such that we don't disassemble the
    // next function also.
    if (end.GetColumnNumber() == 0 && end_func_index > 0) {
      --end_func_index;
      end_offset = functions[end_func_index].code.end_offset();
    } else {
      if (end_func_index >= functions.size()) return false;
      end_offset =
          functions[end_func_index].code.offset() + end.GetColumnNumber();
      if (end_offset > functions[end_func_index].code.end_offset())
        return false;
    }
  }

  AccountingAllocator alloc;
  Zone tmp(&alloc, ZONE_NAME);
  const byte* module_start = module_bytes()->GetChars();

  for (uint32_t func_idx = start_func_index; func_idx <= end_func_index;
       ++func_idx) {
    WasmFunction& func = functions[func_idx];
    if (func.code.length() == 0) continue;

    wasm::BodyLocalDecls locals(&tmp);
    wasm::BytecodeIterator iterator(module_start + func.code.offset(),
                                    module_start + func.code.end_offset(),
                                    &locals);
    DCHECK_LT(0u, locals.encoded_size);
    for (uint32_t offset : iterator.offsets()) {
      uint32_t total_offset = func.code.offset() + offset;
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

MaybeHandle<FixedArray> WasmSharedModuleData::CheckBreakPoints(
    Isolate* isolate, Handle<WasmSharedModuleData> shared, int position) {
  if (!shared->has_breakpoint_infos()) return {};

  Handle<FixedArray> breakpoint_infos(shared->breakpoint_infos(), isolate);
  int insert_pos =
      FindBreakpointInfoInsertPos(isolate, breakpoint_infos, position);
  if (insert_pos >= breakpoint_infos->length()) return {};

  Handle<Object> maybe_breakpoint_info(breakpoint_infos->get(insert_pos),
                                       isolate);
  if (maybe_breakpoint_info->IsUndefined(isolate)) return {};
  Handle<BreakPointInfo> breakpoint_info =
      Handle<BreakPointInfo>::cast(maybe_breakpoint_info);
  if (breakpoint_info->source_position() != position) return {};

  // There is no support for conditional break points. Just assume that every
  // break point always hits.
  Handle<Object> break_points(breakpoint_info->break_points(), isolate);
  if (break_points->IsFixedArray()) {
    return Handle<FixedArray>::cast(break_points);
  }
  Handle<FixedArray> break_points_hit = isolate->factory()->NewFixedArray(1);
  break_points_hit->set(0, *break_points);
  return break_points_hit;
}

Handle<WasmCompiledModule> WasmCompiledModule::New(
    Isolate* isolate, WasmModule* module, Handle<FixedArray> export_wrappers,
    bool use_trap_handler) {
  Handle<WasmCompiledModule> compiled_module = Handle<WasmCompiledModule>::cast(
      isolate->factory()->NewStruct(WASM_COMPILED_MODULE_TYPE, TENURED));
  Handle<WeakCell> weak_native_context =
      isolate->factory()->NewWeakCell(isolate->native_context());
  compiled_module->set_weak_native_context(*weak_native_context);
  compiled_module->set_use_trap_handler(use_trap_handler);
  if (!export_wrappers.is_null()) {
    compiled_module->set_export_wrappers(*export_wrappers);
  }
  compiled_module->set_weak_owning_instance(isolate->heap()->empty_weak_cell());
  wasm::NativeModule* native_module = nullptr;
  {
    std::unique_ptr<wasm::NativeModule> native_module_ptr =
        isolate->wasm_engine()->code_manager()->NewNativeModule(*module);
    native_module = native_module_ptr.release();
    Handle<Foreign> native_module_wrapper =
        Managed<wasm::NativeModule>::From(isolate, native_module);
    compiled_module->set_native_module(*native_module_wrapper);
    compiled_module->GetNativeModule()->SetCompiledModule(compiled_module);
  }

  // TODO(mtrofin): copy the rest of the specialization parameters over.
  // We're currently OK because we're only using defaults.
  return compiled_module;
}

Handle<WasmCompiledModule> WasmCompiledModule::Clone(
    Isolate* isolate, Handle<WasmCompiledModule> module) {
  Handle<FixedArray> code_copy;
  Handle<WasmCompiledModule> ret = Handle<WasmCompiledModule>::cast(
      isolate->factory()->NewStruct(WASM_COMPILED_MODULE_TYPE, TENURED));
  ret->set_shared(module->shared());
  ret->set_weak_native_context(module->weak_native_context());
  ret->set_export_wrappers(module->export_wrappers());
  ret->set_weak_wasm_module(module->weak_wasm_module());
  ret->set_weak_owning_instance(isolate->heap()->empty_weak_cell());
  ret->set_native_module(module->native_module());
  ret->set_use_trap_handler(module->use_trap_handler());

  Handle<FixedArray> export_copy = isolate->factory()->CopyFixedArray(
      handle(module->export_wrappers(), isolate));
  ret->set_export_wrappers(*export_copy);

  std::unique_ptr<wasm::NativeModule> native_module =
      module->GetNativeModule()->Clone();
  // construct the wrapper in 2 steps, because its construction may trigger GC,
  // which would shift the this pointer in set_native_module.
  Handle<Foreign> native_module_wrapper =
      Managed<wasm::NativeModule>::From(isolate, native_module.release());
  ret->set_native_module(*native_module_wrapper);
  ret->GetNativeModule()->SetCompiledModule(ret);

  return ret;
}

wasm::NativeModule* WasmCompiledModule::GetNativeModule() const {
  if (!has_native_module()) return nullptr;
  return Managed<wasm::NativeModule>::cast(native_module())->get();
}

void WasmCompiledModule::Reset(Isolate* isolate,
                               WasmCompiledModule* compiled_module) {
  DisallowHeapAllocation no_gc;
  compiled_module->reset_prev_instance();
  compiled_module->reset_next_instance();
  wasm::NativeModule* native_module = compiled_module->GetNativeModule();
  if (native_module == nullptr) return;
  native_module->SetExecutable(false);

  TRACE("Resetting %zu\n", native_module->instance_id);
  if (compiled_module->use_trap_handler()) {
    native_module->ReleaseProtectedInstructions();
  }

  // Patch code to update memory references, global references, and function
  // table references.
  wasm::CodeSpecialization code_specialization;

  for (uint32_t i = native_module->num_imported_functions(),
                end = native_module->FunctionCount();
       i < end; ++i) {
    wasm::WasmCode* code = native_module->GetCode(i);
    // Skip lazy compile stubs.
    if (code == nullptr || code->kind() != wasm::WasmCode::kFunction) continue;
    bool changed = code_specialization.ApplyToWasmCode(code, SKIP_ICACHE_FLUSH);
    // TODO(wasm): Check if this is faster than passing FLUSH_ICACHE_IF_NEEDED
    // above.
    if (changed) {
      Assembler::FlushICache(code->instructions().start(),
                             code->instructions().size());
    }
  }
}

MaybeHandle<String> WasmSharedModuleData::ExtractUtf8StringFromModuleBytes(
    Isolate* isolate, Handle<WasmSharedModuleData> shared,
    wasm::WireBytesRef ref) {
  // TODO(wasm): cache strings from modules if it's a performance win.
  Handle<SeqOneByteString> module_bytes(shared->module_bytes(), isolate);
  return ExtractUtf8StringFromModuleBytes(isolate, module_bytes, ref);
}

MaybeHandle<String> WasmSharedModuleData::ExtractUtf8StringFromModuleBytes(
    Isolate* isolate, Handle<SeqOneByteString> module_bytes,
    wasm::WireBytesRef ref) {
  DCHECK_GE(module_bytes->length(), ref.end_offset());
  // UTF8 validation happens at decode time.
  DCHECK(unibrow::Utf8::ValidateEncoding(
      reinterpret_cast<const byte*>(module_bytes->GetCharsAddress() +
                                    ref.offset()),
      ref.length()));
  DCHECK_GE(kMaxInt, ref.offset());
  DCHECK_GE(kMaxInt, ref.length());
  return isolate->factory()->NewStringFromUtf8SubString(
      module_bytes, static_cast<int>(ref.offset()),
      static_cast<int>(ref.length()));
}

void WasmCompiledModule::PrintInstancesChain() {
#if DEBUG
  if (!FLAG_trace_wasm_instances) return;
  for (WasmCompiledModule* current = this; current != nullptr;) {
    PrintF("->%zu", current->GetNativeModule()->instance_id);
    if (!current->has_next_instance()) break;
    current = current->next_instance();
  }
  PrintF("\n");
#endif
}

void WasmCompiledModule::InsertInChain(WasmModuleObject* module) {
  DisallowHeapAllocation no_gc;
  WasmCompiledModule* original = module->compiled_module();
  set_next_instance(original);
  original->set_prev_instance(this);
  set_weak_wasm_module(original->weak_wasm_module());
}

void WasmCompiledModule::RemoveFromChain() {
  DisallowHeapAllocation no_gc;
  Isolate* isolate = GetIsolate();

  Object* next = raw_next_instance();
  Object* prev = raw_prev_instance();

  if (!prev->IsUndefined(isolate)) {
    WasmCompiledModule::cast(prev)->set_raw_next_instance(next);
  }
  if (!next->IsUndefined(isolate)) {
    WasmCompiledModule::cast(next)->set_raw_prev_instance(prev);
  }
}

void WasmCompiledModule::ReinitializeAfterDeserialization(
    Isolate* isolate, Handle<WasmCompiledModule> compiled_module) {
  // Reset, but don't delete any global handles, because their owning instance
  // may still be active.
  WasmCompiledModule::Reset(isolate, *compiled_module);
}

MaybeHandle<String> WasmSharedModuleData::GetModuleNameOrNull(
    Isolate* isolate, Handle<WasmSharedModuleData> shared) {
  WasmModule* module = shared->module();
  if (!module->name.is_set()) return {};
  return ExtractUtf8StringFromModuleBytes(isolate, shared, module->name);
}

MaybeHandle<String> WasmSharedModuleData::GetFunctionNameOrNull(
    Isolate* isolate, Handle<WasmSharedModuleData> shared,
    uint32_t func_index) {
  DCHECK_LT(func_index, shared->module()->functions.size());
  wasm::WireBytesRef name =
      shared->module()->LookupName(shared->module_bytes(), func_index);
  if (!name.is_set()) return {};
  return ExtractUtf8StringFromModuleBytes(isolate, shared, name);
}

Handle<String> WasmSharedModuleData::GetFunctionName(
    Isolate* isolate, Handle<WasmSharedModuleData> shared,
    uint32_t func_index) {
  MaybeHandle<String> name = GetFunctionNameOrNull(isolate, shared, func_index);
  if (!name.is_null()) return name.ToHandleChecked();
  return isolate->factory()->NewStringFromStaticChars("<WASM UNNAMED>");
}

Vector<const uint8_t> WasmSharedModuleData::GetRawFunctionName(
    uint32_t func_index) {
  DCHECK_GT(module()->functions.size(), func_index);
  SeqOneByteString* bytes = module_bytes();
  wasm::WireBytesRef name = module()->LookupName(bytes, func_index);
  DCHECK_GE(bytes->length(), name.end_offset());
  return Vector<const uint8_t>(bytes->GetCharsAddress() + name.offset(),
                               name.length());
}

int WasmSharedModuleData::GetFunctionOffset(uint32_t func_index) {
  std::vector<WasmFunction>& functions = module()->functions;
  if (static_cast<uint32_t>(func_index) >= functions.size()) return -1;
  DCHECK_GE(kMaxInt, functions[func_index].code.offset());
  return static_cast<int>(functions[func_index].code.offset());
}

int WasmSharedModuleData::GetContainingFunction(uint32_t byte_offset) {
  std::vector<WasmFunction>& functions = module()->functions;

  // Binary search for a function containing the given position.
  int left = 0;                                    // inclusive
  int right = static_cast<int>(functions.size());  // exclusive
  if (right == 0) return false;
  while (right - left > 1) {
    int mid = left + (right - left) / 2;
    if (functions[mid].code.offset() <= byte_offset) {
      left = mid;
    } else {
      right = mid;
    }
  }
  // If the found function does not contains the given position, return -1.
  WasmFunction& func = functions[left];
  if (byte_offset < func.code.offset() ||
      byte_offset >= func.code.end_offset()) {
    return -1;
  }

  return left;
}

bool WasmSharedModuleData::GetPositionInfo(uint32_t position,
                                           Script::PositionInfo* info) {
  int func_index = GetContainingFunction(position);
  if (func_index < 0) return false;

  WasmFunction& function = module()->functions[func_index];

  info->line = func_index;
  info->column = position - function.code.offset();
  info->line_start = function.code.offset();
  info->line_end = function.code.end_offset();
  return true;
}

bool WasmCompiledModule::SetBreakPoint(
    Handle<WasmCompiledModule> compiled_module, int* position,
    Handle<BreakPoint> break_point) {
  Isolate* isolate = compiled_module->GetIsolate();
  Handle<WasmSharedModuleData> shared(compiled_module->shared(), isolate);

  // Find the function for this breakpoint.
  int func_index = shared->GetContainingFunction(*position);
  if (func_index < 0) return false;
  WasmFunction& func = shared->module()->functions[func_index];
  int offset_in_func = *position - func.code.offset();

  // According to the current design, we should only be called with valid
  // breakable positions.
  DCHECK(IsBreakablePosition(*shared, func_index, offset_in_func));

  // Insert new break point into break_positions of shared module data.
  WasmSharedModuleData::AddBreakpoint(shared, *position, break_point);

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

void WasmCompiledModule::LogWasmCodes(Isolate* isolate) {
  if (!wasm::WasmCode::ShouldBeLogged(isolate)) return;

  wasm::NativeModule* native_module = GetNativeModule();
  if (native_module == nullptr) return;
  const uint32_t number_of_codes = native_module->FunctionCount();
  if (has_shared()) {
    Handle<WasmSharedModuleData> shared_handle(shared(), isolate);
    for (uint32_t i = 0; i < number_of_codes; i++) {
      wasm::WasmCode* code = native_module->GetCode(i);
      if (code == nullptr) continue;
      code->LogCode(isolate);
    }
  }
}

#undef TRACE
#undef TRACE_IFT
}  // namespace internal
}  // namespace v8

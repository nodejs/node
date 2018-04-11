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

namespace v8 {
namespace internal {

// Import a few often used types from the wasm namespace.
using GlobalHandleAddress = wasm::GlobalHandleAddress;
using WasmFunction = wasm::WasmFunction;
using WasmModule = wasm::WasmModule;

namespace {

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
    return !it.current_.is_null() &&
           (!it.current_->has_weak_owning_instance() ||
            it.current_->weak_owning_instance()->cleared());
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

void CompiledModuleFinalizer(const v8::WeakCallbackInfo<void>& data) {
  DisallowHeapAllocation no_gc;
  JSObject** p = reinterpret_cast<JSObject**>(data.GetParameter());
  WasmCompiledModule* compiled_module = WasmCompiledModule::cast(*p);
  compiled_module->reset_native_module();
  GlobalHandles::Destroy(reinterpret_cast<Object**>(p));
}

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
  CHECK(!compiled_module->has_weak_owning_instance());
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
                                       int table_index,
                                       Handle<FixedArray> function_table) {
  DCHECK_EQ(0, function_table->length() % compiler::kFunctionTableEntrySize);
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
  new_dispatch_tables->set(old_length + kDispatchTableFunctionTableOffset,
                           *function_table);

  table_obj->set_dispatch_tables(*new_dispatch_tables);
}

void WasmTableObject::Grow(Isolate* isolate, uint32_t count) {
  // TODO(6792): No longer needed once WebAssembly code is off heap.
  CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
  Handle<FixedArray> dispatch_tables(this->dispatch_tables());
  DCHECK_EQ(0, dispatch_tables->length() % kDispatchTableNumElements);
  uint32_t old_size = functions()->length();

  Zone specialization_zone(isolate->allocator(), ZONE_NAME);
  for (int i = 0; i < dispatch_tables->length();
       i += kDispatchTableNumElements) {
    Handle<FixedArray> old_function_table(FixedArray::cast(
        dispatch_tables->get(i + kDispatchTableFunctionTableOffset)));
    Handle<FixedArray> new_function_table = isolate->global_handles()->Create(
        *isolate->factory()->CopyFixedArrayAndGrow(
            old_function_table, count * compiler::kFunctionTableEntrySize));

    GlobalHandleAddress new_function_table_addr = new_function_table.address();

    int table_index =
        Smi::cast(dispatch_tables->get(i + kDispatchTableIndexOffset))->value();
    // Update dispatch tables with new function tables.
    dispatch_tables->set(i + kDispatchTableFunctionTableOffset,
                         *new_function_table);

    // Patch the code of the respective instance.
    if (FLAG_wasm_jit_to_native) {
      DisallowHeapAllocation no_gc;
      wasm::CodeSpecialization code_specialization(isolate,
                                                   &specialization_zone);
      WasmInstanceObject* instance =
          WasmInstanceObject::cast(dispatch_tables->get(i));
      WasmCompiledModule* compiled_module = instance->compiled_module();
      wasm::NativeModule* native_module = compiled_module->GetNativeModule();
      wasm::NativeModuleModificationScope native_module_modification_scope(
          native_module);
      GlobalHandleAddress old_function_table_addr =
          native_module->function_tables()[table_index];
      code_specialization.PatchTableSize(old_size, old_size + count);
      code_specialization.RelocatePointer(old_function_table_addr,
                                          new_function_table_addr);
      code_specialization.ApplyToWholeInstance(instance);
      native_module->function_tables()[table_index] = new_function_table_addr;
    } else {
      DisallowHeapAllocation no_gc;
      wasm::CodeSpecialization code_specialization(isolate,
                                                   &specialization_zone);
      WasmInstanceObject* instance =
          WasmInstanceObject::cast(dispatch_tables->get(i));
      WasmCompiledModule* compiled_module = instance->compiled_module();
      GlobalHandleAddress old_function_table_addr =
          WasmCompiledModule::GetTableValue(compiled_module->function_tables(),
                                            table_index);
      code_specialization.PatchTableSize(old_size, old_size + count);
      code_specialization.RelocatePointer(old_function_table_addr,
                                          new_function_table_addr);
      code_specialization.ApplyToWholeInstance(instance);
      WasmCompiledModule::UpdateTableValue(compiled_module->function_tables(),
                                           table_index,
                                           new_function_table_addr);
    }
  }
}

void WasmTableObject::Set(Isolate* isolate, Handle<WasmTableObject> table,
                          int32_t index, Handle<JSFunction> function) {
  Handle<FixedArray> array(table->functions(), isolate);

  Handle<FixedArray> dispatch_tables(table->dispatch_tables(), isolate);

  wasm::FunctionSig* sig = nullptr;
  Handle<Object> code = Handle<Object>::null();
  Handle<Object> value = isolate->factory()->null_value();

  if (!function.is_null()) {
    auto exported_function = Handle<WasmExportedFunction>::cast(function);
    auto* wasm_function = wasm::GetWasmFunctionForExport(isolate, function);
    // The verification that {function} is an export was done
    // by the caller.
    DCHECK(wasm_function != nullptr && wasm_function->sig != nullptr);
    sig = wasm_function->sig;
    value = function;
    // TODO(titzer): Make JSToWasm wrappers just call the WASM to WASM wrapper,
    // and then we can just reuse the WASM to WASM wrapper.
    WasmCodeWrapper wasm_code = exported_function->GetWasmCode();
    wasm::NativeModule* native_module =
        wasm_code.IsCodeObject() ? nullptr : wasm_code.GetWasmCode()->owner();
    CodeSpaceMemoryModificationScope gc_modification_scope(isolate->heap());
    wasm::NativeModuleModificationScope native_modification_scope(
        native_module);
    code = wasm::GetOrCreateIndirectCallWrapper(
        isolate, handle(exported_function->instance()), wasm_code,
        exported_function->function_index(), sig);
  }
  UpdateDispatchTables(table, index, sig, code);
  array->set(index, *value);
}

void WasmTableObject::UpdateDispatchTables(Handle<WasmTableObject> table,
                                           int index, wasm::FunctionSig* sig,
                                           Handle<Object> code_or_foreign) {
  DisallowHeapAllocation no_gc;
  FixedArray* dispatch_tables = table->dispatch_tables();
  DCHECK_EQ(0, dispatch_tables->length() % kDispatchTableNumElements);
  for (int i = 0; i < dispatch_tables->length();
       i += kDispatchTableNumElements) {
    FixedArray* function_table = FixedArray::cast(
        dispatch_tables->get(i + kDispatchTableFunctionTableOffset));
    Smi* sig_smi = Smi::FromInt(-1);
    Object* code = Smi::kZero;
    if (sig) {
      DCHECK(code_or_foreign->IsCode() || code_or_foreign->IsForeign());
      WasmInstanceObject* instance = WasmInstanceObject::cast(
          dispatch_tables->get(i + kDispatchTableInstanceOffset));
      // Note that {SignatureMap::Find} may return {-1} if the signature is
      // not found; it will simply never match any check.
      auto sig_index = instance->module()->signature_map.Find(sig);
      sig_smi = Smi::FromInt(sig_index);
      code = *code_or_foreign;
    } else {
      DCHECK(code_or_foreign.is_null());
    }
    function_table->set(compiler::FunctionTableSigOffset(index), sig_smi);
    function_table->set(compiler::FunctionTableCodeOffset(index), code);
  }
}

namespace {

Handle<JSArrayBuffer> GrowMemoryBuffer(Isolate* isolate,
                                       Handle<JSArrayBuffer> old_buffer,
                                       uint32_t pages, uint32_t maximum_pages,
                                       bool use_trap_handler) {
  if (!old_buffer->is_growable()) return Handle<JSArrayBuffer>::null();
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
  if (old_pages > maximum_pages || pages > maximum_pages - old_pages) {
    return Handle<JSArrayBuffer>::null();
  }
  const bool enable_guard_regions =
      old_buffer.is_null() ? use_trap_handler : old_buffer->has_guard_region();
  size_t new_size =
      static_cast<size_t>(old_pages + pages) * wasm::kWasmPageSize;
  if (new_size > FLAG_wasm_max_mem_pages * wasm::kWasmPageSize ||
      new_size > kMaxInt) {
    return Handle<JSArrayBuffer>::null();
  }
  if ((enable_guard_regions || old_size == new_size) && old_size != 0) {
    DCHECK_NOT_NULL(old_buffer->backing_store());
    if (old_size != new_size) {
      isolate->array_buffer_allocator()->SetProtection(
          old_mem_start, new_size,
          v8::ArrayBuffer::Allocator::Protection::kReadWrite);
      reinterpret_cast<v8::Isolate*>(isolate)
          ->AdjustAmountOfExternalAllocatedMemory(pages * wasm::kWasmPageSize);
    }
    // NOTE: We must allocate a new array buffer here because the spec
    // assumes that ArrayBuffers do not change size.
    void* allocation_base = old_buffer->allocation_base();
    size_t allocation_length = old_buffer->allocation_length();
    void* backing_store = old_buffer->backing_store();
    bool has_guard_region = old_buffer->has_guard_region();
    bool is_external = old_buffer->is_external();
    // Disconnect buffer early so GC won't free it.
    i::wasm::DetachMemoryBuffer(isolate, old_buffer, false);
    Handle<JSArrayBuffer> new_buffer = wasm::SetupArrayBuffer(
        isolate, allocation_base, allocation_length, backing_store, new_size,
        is_external, has_guard_region);
    return new_buffer;
  } else {
    bool free_memory = false;
    Handle<JSArrayBuffer> new_buffer;
    if (pages != 0) {
      // Allocate a new buffer and memcpy the old contents.
      free_memory = true;
      new_buffer =
          wasm::NewArrayBuffer(isolate, new_size, enable_guard_regions);
      if (new_buffer.is_null() || old_size == 0) return new_buffer;
      Address new_mem_start = static_cast<Address>(new_buffer->backing_store());
      memcpy(new_mem_start, old_mem_start, old_size);
      DCHECK(old_buffer.is_null() || !old_buffer->is_shared());
      DCHECK(old_buffer.is_null() || !old_buffer->has_guard_region());
    } else {
      // Reuse the prior backing store, but allocate a new array buffer.
      new_buffer = wasm::SetupArrayBuffer(
          isolate, old_buffer->allocation_base(),
          old_buffer->allocation_length(), old_buffer->backing_store(),
          new_size, old_buffer->is_external(), old_buffer->has_guard_region());
    }
    i::wasm::DetachMemoryBuffer(isolate, old_buffer, free_memory);
    return new_buffer;
  }
}

// May GC, because SetSpecializationMemInfoFrom may GC
void SetInstanceMemory(Isolate* isolate, Handle<WasmInstanceObject> instance,
                       Handle<JSArrayBuffer> buffer) {
  auto wasm_context = instance->wasm_context()->get();
  wasm_context->SetRawMemory(reinterpret_cast<byte*>(buffer->backing_store()),
                             buffer->byte_length()->Number());
#if DEBUG
  // To flush out bugs earlier, in DEBUG mode, check that all pages of the
  // memory are accessible by reading and writing one byte on each page.
  for (uint32_t offset = 0; offset < wasm_context->mem_size;
       offset += wasm::kWasmPageSize) {
    byte val = wasm_context->mem_start[offset];
    wasm_context->mem_start[offset] = val;
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

    // TODO(kschimpf): Modify to use argument defining style of
    // memory.  (see above).
    buffer = wasm::SetupArrayBuffer(isolate, nullptr, 0, nullptr, 0, false,
                                    trap_handler::IsTrapHandlerEnabled());
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
  Handle<WeakFixedArray> old_instances =
      memory->has_instances()
          ? Handle<WeakFixedArray>(memory->instances(), isolate)
          : Handle<WeakFixedArray>::null();
  Handle<WeakFixedArray> new_instances =
      WeakFixedArray::Add(old_instances, instance);
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
  new_buffer = GrowMemoryBuffer(isolate, old_buffer, pages, maximum_pages,
                                trap_handler::IsTrapHandlerEnabled());
  if (new_buffer.is_null()) return -1;

  if (memory_object->has_instances()) {
    Handle<WeakFixedArray> instances(memory_object->instances(), isolate);
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

  auto wasm_context = Managed<WasmContext>::Allocate(isolate);
  wasm_context->get()->SetRawMemory(nullptr, 0);
  wasm_context->get()->globals_start = nullptr;
  instance->set_wasm_context(*wasm_context);

  instance->set_compiled_module(*compiled_module);
  return instance;
}

int32_t WasmInstanceObject::GrowMemory(Isolate* isolate,
                                       Handle<WasmInstanceObject> instance,
                                       uint32_t pages) {
  DCHECK(instance->has_memory_object());
  return WasmMemoryObject::Grow(
      isolate, handle(instance->memory_object(), isolate), pages);
}

WasmInstanceObject* WasmInstanceObject::GetOwningInstance(
    const wasm::WasmCode* code) {
  DisallowHeapAllocation no_gc;
  Object* weak_link = nullptr;
  DCHECK(code->kind() == wasm::WasmCode::kFunction ||
         code->kind() == wasm::WasmCode::kInterpreterStub);
  weak_link = code->owner()->compiled_module()->weak_owning_instance();
  DCHECK(weak_link->IsWeakCell());
  WeakCell* cell = WeakCell::cast(weak_link);
  if (cell->cleared()) return nullptr;
  return WasmInstanceObject::cast(cell->value());
}

WasmInstanceObject* WasmInstanceObject::GetOwningInstanceGC(Code* code) {
  DisallowHeapAllocation no_gc;
  DCHECK(code->kind() == Code::WASM_FUNCTION ||
         code->kind() == Code::WASM_INTERPRETER_ENTRY);
  FixedArray* deopt_data = code->deoptimization_data();
  DCHECK_EQ(code->kind() == Code::WASM_INTERPRETER_ENTRY ? 1 : 2,
            deopt_data->length());
  Object* weak_link = deopt_data->get(0);
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
  int found_instances = compiled_module->has_weak_owning_instance() ? 1 : 0;
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

bool WasmExportedFunction::IsWasmExportedFunction(Object* object) {
  if (!object->IsJSFunction()) return false;
  Handle<JSFunction> js_function(JSFunction::cast(object));
  if (Code::JS_TO_WASM_FUNCTION != js_function->code()->kind()) return false;

  Handle<Symbol> symbol(
      js_function->GetIsolate()->factory()->wasm_instance_symbol());
  MaybeHandle<Object> maybe_result =
      JSObject::GetPropertyOrElement(js_function, symbol);
  Handle<Object> result;
  if (!maybe_result.ToHandle(&result)) return false;
  return result->IsWasmInstanceObject();
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
  Handle<SharedFunctionInfo> shared =
      isolate->factory()->NewSharedFunctionInfo(name, export_wrapper, false);
  shared->set_length(arity);
  shared->set_internal_formal_parameter_count(arity);
  NewFunctionArgs args = NewFunctionArgs::ForWasm(
      name, export_wrapper, isolate->sloppy_function_map());
  Handle<JSFunction> js_function = isolate->factory()->NewFunction(args);

  js_function->set_shared(*shared);
  Handle<Symbol> instance_symbol(isolate->factory()->wasm_instance_symbol());
  JSObject::AddProperty(js_function, instance_symbol, instance, DONT_ENUM);

  Handle<Symbol> function_index_symbol(
      isolate->factory()->wasm_function_index_symbol());
  JSObject::AddProperty(js_function, function_index_symbol,
                        isolate->factory()->NewNumber(func_index), DONT_ENUM);

  return Handle<WasmExportedFunction>::cast(js_function);
}

WasmCodeWrapper WasmExportedFunction::GetWasmCode() {
  DisallowHeapAllocation no_gc;
  Handle<Code> export_wrapper_code = handle(this->code());
  DCHECK_EQ(export_wrapper_code->kind(), Code::JS_TO_WASM_FUNCTION);
  int mask =
      RelocInfo::ModeMask(FLAG_wasm_jit_to_native ? RelocInfo::JS_TO_WASM_CALL
                                                  : RelocInfo::CODE_TARGET);
  auto IsWasmFunctionCode = [](Code* code) {
    return code->kind() == Code::WASM_FUNCTION ||
           code->kind() == Code::WASM_TO_JS_FUNCTION ||
           code->kind() == Code::WASM_TO_WASM_FUNCTION ||
           code->kind() == Code::WASM_INTERPRETER_ENTRY ||
           code->builtin_index() == Builtins::kWasmCompileLazy;
  };

  for (RelocIterator it(*export_wrapper_code, mask);; it.next()) {
    DCHECK(!it.done());
    WasmCodeWrapper target;
    if (FLAG_wasm_jit_to_native) {
      target = WasmCodeWrapper(
          GetIsolate()->wasm_engine()->code_manager()->LookupCode(
              it.rinfo()->js_to_wasm_address()));
    } else {
      Code* code = Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
      if (!IsWasmFunctionCode(code)) continue;
      target = WasmCodeWrapper(handle(code));
    }
// There should only be this one call to wasm code.
#ifdef DEBUG
    for (it.next(); !it.done(); it.next()) {
      if (FLAG_wasm_jit_to_native) {
        UNREACHABLE();
      } else {
        Code* code =
            Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
        DCHECK(!IsWasmFunctionCode(code));
      }
    }
#endif
    return target;
  }
  UNREACHABLE();
}

bool WasmSharedModuleData::IsWasmSharedModuleData(Object* object) {
  if (!object->IsFixedArray()) return false;
  FixedArray* arr = FixedArray::cast(object);
  if (arr->length() != kFieldCount) return false;
  Isolate* isolate = arr->GetIsolate();
  if (!arr->get(kModuleWrapperIndex)->IsForeign()) return false;
  if (!arr->get(kModuleBytesIndex)->IsUndefined(isolate) &&
      !arr->get(kModuleBytesIndex)->IsSeqOneByteString())
    return false;
  if (!arr->get(kScriptIndex)->IsScript()) return false;
  if (!arr->get(kAsmJsOffsetTableIndex)->IsUndefined(isolate) &&
      !arr->get(kAsmJsOffsetTableIndex)->IsByteArray())
    return false;
  if (!arr->get(kBreakPointInfosIndex)->IsUndefined(isolate) &&
      !arr->get(kBreakPointInfosIndex)->IsFixedArray())
    return false;
  return true;
}

WasmSharedModuleData* WasmSharedModuleData::cast(Object* object) {
  DCHECK(IsWasmSharedModuleData(object));
  return reinterpret_cast<WasmSharedModuleData*>(object);
}

WasmModule* WasmSharedModuleData::module() {
  // We populate the kModuleWrapper field with a Foreign holding the
  // address to the address of a WasmModule. This is because we can
  // handle both cases when the WasmModule's lifetime is managed through
  // a Managed<WasmModule> object, as well as cases when it's managed
  // by the embedder. CcTests fall into the latter case.
  return *(reinterpret_cast<WasmModule**>(
      Foreign::cast(get(kModuleWrapperIndex))->foreign_address()));
}

Handle<WasmSharedModuleData> WasmSharedModuleData::New(
    Isolate* isolate, Handle<Foreign> module_wrapper,
    Handle<SeqOneByteString> module_bytes, Handle<Script> script,
    Handle<ByteArray> asm_js_offset_table) {
  Handle<FixedArray> arr =
      isolate->factory()->NewFixedArray(kFieldCount, TENURED);
  arr->set(kModuleWrapperIndex, *module_wrapper);
  if (!module_bytes.is_null()) {
    arr->set(kModuleBytesIndex, *module_bytes);
  }
  if (!script.is_null()) {
    arr->set(kScriptIndex, *script);
  }
  if (!asm_js_offset_table.is_null()) {
    arr->set(kAsmJsOffsetTableIndex, *asm_js_offset_table);
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
  DCHECK(shared->get(kModuleWrapperIndex)->IsUndefined(isolate));
#ifdef DEBUG
  // No BreakpointInfo objects should survive deserialization.
  if (shared->has_breakpoint_infos()) {
    for (int i = 0, e = shared->breakpoint_infos()->length(); i < e; ++i) {
      DCHECK(shared->breakpoint_infos()->get(i)->IsUndefined(isolate));
    }
  }
#endif

  shared->set(kBreakPointInfosIndex, isolate->heap()->undefined_value());

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
    wasm::ModuleResult result =
        SyncDecodeWasmModule(isolate, start, end, false, wasm::kWasmOrigin);
    CHECK(result.ok());
    CHECK_NOT_NULL(result.val);
    // Take ownership of the WasmModule and immediately transfer it to the
    // WasmModuleWrapper below.
    module = result.val.release();
  }

  Handle<wasm::WasmModuleWrapper> module_wrapper =
      wasm::WasmModuleWrapper::From(isolate, module);

  shared->set(kModuleWrapperIndex, *module_wrapper);
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
    shared->set(kBreakPointInfosIndex, *breakpoint_infos);
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
    shared->set(kBreakPointInfosIndex, *new_breakpoint_infos);
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

void WasmSharedModuleData::PrepareForLazyCompilation(
    Handle<WasmSharedModuleData> shared) {
  if (shared->has_lazy_compilation_orchestrator()) return;
  Isolate* isolate = shared->GetIsolate();
  auto orch_handle =
      Managed<wasm::LazyCompilationOrchestrator>::Allocate(isolate);
  shared->set_lazy_compilation_orchestrator(*orch_handle);
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

  Handle<Object> breakpoint_objects(breakpoint_info->break_point_objects(),
                                    isolate);
  return isolate->debug()->GetHitBreakPointObjects(breakpoint_objects);
}

Handle<WasmCompiledModule> WasmCompiledModule::New(
    Isolate* isolate, WasmModule* module, Handle<FixedArray> code_table,
    Handle<FixedArray> export_wrappers,
    const std::vector<GlobalHandleAddress>& function_tables,
    bool use_trap_handler) {
  Handle<FixedArray> ret =
      isolate->factory()->NewFixedArray(PropertyIndices::Count, TENURED);
  // WasmCompiledModule::cast would fail since fields are not set yet.
  Handle<WasmCompiledModule> compiled_module(
      reinterpret_cast<WasmCompiledModule*>(*ret), isolate);
  Handle<WeakCell> weak_native_context =
      isolate->factory()->NewWeakCell(isolate->native_context());
  compiled_module->set_weak_native_context(*weak_native_context);
  compiled_module->set_use_trap_handler(use_trap_handler);
  if (!FLAG_wasm_jit_to_native) {
    compiled_module->InitId();
    compiled_module->set_code_table(*code_table);
    compiled_module->set_export_wrappers(*export_wrappers);
    // TODO(mtrofin): we copy these because the order of finalization isn't
    // reliable, and we need these at Reset (which is called at
    // finalization). If the order were reliable, and top-down, we could instead
    // just get them from shared().
    compiled_module->set_num_imported_functions(module->num_imported_functions);

    int num_function_tables = static_cast<int>(function_tables.size());
    if (num_function_tables > 0) {
      Handle<FixedArray> ft =
          isolate->factory()->NewFixedArray(num_function_tables, TENURED);
      for (int i = 0; i < num_function_tables; ++i) {
        SetTableValue(isolate, ft, i, function_tables[i]);
      }
      // TODO(wasm): setting the empty tables here this way is OK under the
      // assumption that we compile and then instantiate. It needs rework if we
      // do direct instantiation. The empty tables are used as a default when
      // resetting the compiled module.
      compiled_module->set_function_tables(*ft);
      compiled_module->set_empty_function_tables(*ft);
    }
  } else {
    if (!export_wrappers.is_null()) {
      compiled_module->set_export_wrappers(*export_wrappers);
    }
    wasm::NativeModule* native_module = nullptr;
    {
      std::unique_ptr<wasm::NativeModule> native_module_ptr =
          isolate->wasm_engine()->code_manager()->NewNativeModule(*module);
      native_module = native_module_ptr.release();
      Handle<Foreign> native_module_wrapper =
          Managed<wasm::NativeModule>::From(isolate, native_module);
      compiled_module->set_native_module(*native_module_wrapper);
      Handle<WasmCompiledModule> weak_link =
          isolate->global_handles()->Create(*compiled_module);
      GlobalHandles::MakeWeak(Handle<Object>::cast(weak_link).location(),
                              Handle<Object>::cast(weak_link).location(),
                              &CompiledModuleFinalizer,
                              v8::WeakCallbackType::kFinalizer);
      compiled_module->GetNativeModule()->SetCompiledModule(weak_link);
    }
    // This is here just because it's easier for APIs that need to work with
    // either code_table or native_module. Otherwise we need to check if
    // has_code_table and pass undefined.
    compiled_module->set_code_table(*code_table);

    native_module->function_tables() = function_tables;
    native_module->empty_function_tables() = function_tables;

    int function_count = static_cast<int>(module->functions.size());
    Handle<FixedArray> handler_table =
        isolate->factory()->NewFixedArray(function_count, TENURED);
    compiled_module->set_handler_table(*handler_table);
    Handle<FixedArray> source_positions =
        isolate->factory()->NewFixedArray(function_count, TENURED);
    compiled_module->set_source_positions(*source_positions);
  }
  // TODO(mtrofin): copy the rest of the specialization parameters over.
  // We're currently OK because we're only using defaults.
  return compiled_module;
}

Handle<WasmCompiledModule> WasmCompiledModule::Clone(
    Isolate* isolate, Handle<WasmCompiledModule> module) {
  Handle<FixedArray> code_copy;
  if (!FLAG_wasm_jit_to_native) {
    code_copy = isolate->factory()->CopyFixedArray(
        handle(module->code_table(), isolate));
  }
  Handle<WasmCompiledModule> ret = Handle<WasmCompiledModule>::cast(
      isolate->factory()->CopyFixedArray(module));
  ret->reset_weak_owning_instance();
  ret->reset_next_instance();
  ret->reset_prev_instance();
  ret->reset_weak_exported_functions();
  if (!FLAG_wasm_jit_to_native) {
    ret->InitId();
    ret->set_code_table(*code_copy);
    return ret;
  }

  std::unique_ptr<wasm::NativeModule> native_module =
      module->GetNativeModule()->Clone();
  // construct the wrapper in 2 steps, because its construction may trigger GC,
  // which would shift the this pointer in set_native_module.
  Handle<Foreign> native_module_wrapper =
      Managed<wasm::NativeModule>::From(isolate, native_module.release());
  ret->set_native_module(*native_module_wrapper);
  Handle<WasmCompiledModule> weak_link =
      isolate->global_handles()->Create(*ret);
  GlobalHandles::MakeWeak(Handle<Object>::cast(weak_link).location(),
                          Handle<Object>::cast(weak_link).location(),
                          &CompiledModuleFinalizer,
                          v8::WeakCallbackType::kFinalizer);
  ret->GetNativeModule()->SetCompiledModule(weak_link);

  if (module->has_lazy_compile_data()) {
    Handle<FixedArray> lazy_comp_data = isolate->factory()->NewFixedArray(
        module->lazy_compile_data()->length(), TENURED);
    ret->set_lazy_compile_data(*lazy_comp_data);
  }
  return ret;
}

void WasmCompiledModule::SetTableValue(Isolate* isolate,
                                       Handle<FixedArray> table, int index,
                                       Address value) {
  Handle<HeapNumber> number = isolate->factory()->NewHeapNumber(
      static_cast<double>(reinterpret_cast<size_t>(value)), MUTABLE, TENURED);
  table->set(index, *number);
}

void WasmCompiledModule::UpdateTableValue(FixedArray* table, int index,
                                          Address value) {
  DisallowHeapAllocation no_gc;
  HeapNumber::cast(table->get(index))
      ->set_value(static_cast<double>(reinterpret_cast<size_t>(value)));
}

Address WasmCompiledModule::GetTableValue(FixedArray* table, int index) {
  DisallowHeapAllocation no_gc;
  double value = HeapNumber::cast(table->get(index))->value();
  return reinterpret_cast<Address>(static_cast<size_t>(value));
}

wasm::NativeModule* WasmCompiledModule::GetNativeModule() const {
  if (!has_native_module()) return nullptr;
  return Managed<wasm::NativeModule>::cast(native_module())->get();
}

void WasmCompiledModule::ResetGCModel(Isolate* isolate,
                                      WasmCompiledModule* compiled_module) {
  DisallowHeapAllocation no_gc;
  TRACE("Resetting %d\n", compiled_module->instance_id());
  Object* undefined = *isolate->factory()->undefined_value();
  Object* fct_obj = compiled_module->code_table();
  if (fct_obj != nullptr && fct_obj != undefined) {
    // Patch code to update memory references, global references, and function
    // table references.
    Zone specialization_zone(isolate->allocator(), ZONE_NAME);
    wasm::CodeSpecialization code_specialization(isolate, &specialization_zone);

    // Reset function tables.
    if (compiled_module->has_function_tables()) {
      FixedArray* function_tables = compiled_module->function_tables();
      FixedArray* empty_function_tables =
          compiled_module->empty_function_tables();
      if (function_tables != empty_function_tables) {
        DCHECK_EQ(function_tables->length(), empty_function_tables->length());
        for (int i = 0, e = function_tables->length(); i < e; ++i) {
          GlobalHandleAddress func_addr =
              WasmCompiledModule::GetTableValue(function_tables, i);
          code_specialization.RelocatePointer(
              func_addr,
              WasmCompiledModule::GetTableValue(empty_function_tables, i));
        }
        compiled_module->set_function_tables(empty_function_tables);
      }
    }

    // TODO(6792): No longer needed once WebAssembly code is off heap.
    CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
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
      bool changed = code_specialization.ApplyToWasmCode(
          WasmCodeWrapper(handle(code)), SKIP_ICACHE_FLUSH);
      // TODO(wasm): Check if this is faster than passing FLUSH_ICACHE_IF_NEEDED
      // above.
      if (changed) {
        Assembler::FlushICache(isolate, code->instruction_start(),
                               code->instruction_size());
      }
    }
  }
}

void WasmCompiledModule::InitId() {
#if DEBUG
  static uint32_t instance_id_counter = 0;
  set(kID_instance_id, Smi::FromInt(instance_id_counter++));
  TRACE("New compiled module id: %d\n", instance_id());
#endif
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
    for (uint32_t i = native_module->num_imported_functions(),
                  e = native_module->FunctionCount();
         i < e; ++i) {
      wasm::WasmCode* wasm_code = native_module->GetCode(i);
      if (wasm_code->HasTrapHandlerIndex()) {
        CHECK_LT(wasm_code->trap_handler_index(),
                 static_cast<size_t>(std::numeric_limits<int>::max()));
        trap_handler::ReleaseHandlerData(
            static_cast<int>(wasm_code->trap_handler_index()));
        wasm_code->ResetTrapHandlerIndex();
      }
    }
  }

  // Patch code to update memory references, global references, and function
  // table references.
  Zone specialization_zone(isolate->allocator(), ZONE_NAME);
  wasm::CodeSpecialization code_specialization(isolate, &specialization_zone);

  if (compiled_module->has_lazy_compile_data()) {
    for (int i = 0, e = compiled_module->lazy_compile_data()->length(); i < e;
         ++i) {
      compiled_module->lazy_compile_data()->set(
          i, isolate->heap()->undefined_value());
    }
  }
  // Reset function tables.
  if (native_module->function_tables().size() > 0) {
    std::vector<GlobalHandleAddress>& function_tables =
        native_module->function_tables();
    std::vector<GlobalHandleAddress>& empty_function_tables =
        native_module->empty_function_tables();

    if (function_tables != empty_function_tables) {
      DCHECK_EQ(function_tables.size(), empty_function_tables.size());
      for (size_t i = 0, e = function_tables.size(); i < e; ++i) {
        code_specialization.RelocatePointer(function_tables[i],
                                            empty_function_tables[i]);
      }
      native_module->function_tables() = empty_function_tables;
    }
  }

  for (uint32_t i = native_module->num_imported_functions(),
                end = native_module->FunctionCount();
       i < end; ++i) {
    wasm::WasmCode* code = native_module->GetCode(i);
    // Skip lazy compile stubs.
    if (code == nullptr || code->kind() != wasm::WasmCode::kFunction) continue;
    bool changed = code_specialization.ApplyToWasmCode(WasmCodeWrapper(code),
                                                       SKIP_ICACHE_FLUSH);
    // TODO(wasm): Check if this is faster than passing FLUSH_ICACHE_IF_NEEDED
    // above.
    if (changed) {
      Assembler::FlushICache(isolate, code->instructions().start(),
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

bool WasmCompiledModule::IsWasmCompiledModule(Object* obj) {
  if (!obj->IsFixedArray()) return false;
  FixedArray* arr = FixedArray::cast(obj);
  if (arr->length() != PropertyIndices::Count) return false;
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
  WCM_CHECK_TYPE(NAME, obj->IsFixedArray() || obj->IsUndefined(isolate))
#define WCM_CHECK_WEAK_LINK(TYPE, NAME) WCM_CHECK_OBJECT(WeakCell, NAME)
#define WCM_CHECK_SMALL_NUMBER(TYPE, NAME) \
  WCM_CHECK_TYPE(NAME, obj->IsUndefined(isolate) || obj->IsSmi())
#define WCM_CHECK(KIND, TYPE, NAME) WCM_CHECK_##KIND(TYPE, NAME)
#define WCM_CHECK_SMALL_CONST_NUMBER(TYPE, NAME) \
  WCM_CHECK_TYPE(NAME, obj->IsSmi())
#undef WCM_CHECK_TYPE
#undef WCM_CHECK_OBJECT
#undef WCM_CHECK_CONST_OBJECT
#undef WCM_CHECK_WASM_OBJECT
#undef WCM_CHECK_WEAK_LINK
#undef WCM_CHECK_SMALL_NUMBER
#undef WCM_CHECK
#undef WCM_CHECK_SMALL_CONST_NUMBER

  // All checks passed.
  return true;
}

void WasmCompiledModule::PrintInstancesChain() {
#if DEBUG
  if (!FLAG_trace_wasm_instances) return;
  for (WasmCompiledModule* current = this; current != nullptr;) {
    if (FLAG_wasm_jit_to_native) {
      PrintF("->%zu", current->GetNativeModule()->instance_id);
    } else {
      PrintF("->%d", current->instance_id());
    }
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

  Object* next = get(kID_next_instance);
  Object* prev = get(kID_prev_instance);

  if (!prev->IsUndefined(isolate)) {
    WasmCompiledModule::cast(prev)->set(kID_next_instance, next);
  }
  if (!next->IsUndefined(isolate)) {
    WasmCompiledModule::cast(next)->set(kID_prev_instance, prev);
  }
}

void WasmCompiledModule::OnWasmModuleDecodingComplete(
    Handle<WasmSharedModuleData> shared) {
  set_shared(*shared);
}

void WasmCompiledModule::ReinitializeAfterDeserialization(
    Isolate* isolate, Handle<WasmCompiledModule> compiled_module) {
  // This method must only be called immediately after deserialization.
  // At this point, no module wrapper exists, so the shared module data is
  // incomplete.
  Handle<WasmSharedModuleData> shared(
      static_cast<WasmSharedModuleData*>(compiled_module->get(kID_shared)),
      isolate);
  if (!FLAG_wasm_jit_to_native) {
    DCHECK(!WasmSharedModuleData::IsWasmSharedModuleData(*shared));
    WasmSharedModuleData::ReinitializeAfterDeserialization(isolate, shared);
  }
  size_t function_table_count =
      compiled_module->shared()->module()->function_tables.size();
  wasm::NativeModule* native_module = compiled_module->GetNativeModule();

  if (function_table_count > 0) {
    // The tables are of the right size, but contain bogus global handle
    // addresses. Produce new global handles for the empty tables, then reset,
    // which will relocate the code. We end up with a WasmCompiledModule as-if
    // it were just compiled.
    Handle<FixedArray> function_tables;
    if (!FLAG_wasm_jit_to_native) {
      DCHECK(compiled_module->has_function_tables());
      function_tables =
          handle(compiled_module->empty_function_tables(), isolate);
    } else {
      DCHECK_GT(native_module->function_tables().size(), 0);
    }
    for (size_t i = 0; i < function_table_count; ++i) {
      Handle<Object> global_func_table_handle =
          isolate->global_handles()->Create(isolate->heap()->undefined_value());
      GlobalHandleAddress new_func_table = global_func_table_handle.address();
      if (!FLAG_wasm_jit_to_native) {
        SetTableValue(isolate, function_tables, static_cast<int>(i),
                      new_func_table);
      } else {
        native_module->empty_function_tables()[i] = new_func_table;
      }
    }
  }

  // Reset, but don't delete any global handles, because their owning instance
  // may still be active.
  WasmCompiledModule::Reset(isolate, *compiled_module);
  DCHECK(WasmSharedModuleData::IsWasmSharedModuleData(*shared));
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
  WasmFunction& function = shared->module()->functions[func_index];
  if (!function.name.is_set()) return {};
  return ExtractUtf8StringFromModuleBytes(isolate, shared, function.name);
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
  WasmFunction& function = module()->functions[func_index];
  SeqOneByteString* bytes = module_bytes();
  DCHECK_GE(bytes->length(), function.name.end_offset());
  return Vector<const uint8_t>(
      bytes->GetCharsAddress() + function.name.offset(),
      function.name.length());
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
    Handle<Object> break_point_object) {
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
  WasmSharedModuleData::AddBreakpoint(shared, *position, break_point_object);

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

void AttachWasmFunctionInfo(Isolate* isolate, Handle<Code> code,
                            MaybeHandle<WeakCell> weak_instance,
                            int func_index) {
  DCHECK(weak_instance.is_null() ||
         weak_instance.ToHandleChecked()->value()->IsWasmInstanceObject());
  Handle<FixedArray> deopt_data = isolate->factory()->NewFixedArray(2, TENURED);
  if (!weak_instance.is_null()) {
    // TODO(wasm): Introduce constants for the indexes in wasm deopt data.
    deopt_data->set(0, *weak_instance.ToHandleChecked());
  }
  deopt_data->set(1, Smi::FromInt(func_index));

  code->set_deoptimization_data(*deopt_data);
}

void AttachWasmFunctionInfo(Isolate* isolate, Handle<Code> code,
                            MaybeHandle<WasmInstanceObject> instance,
                            int func_index) {
  MaybeHandle<WeakCell> weak_instance;
  if (!instance.is_null()) {
    weak_instance = isolate->factory()->NewWeakCell(instance.ToHandleChecked());
  }
  AttachWasmFunctionInfo(isolate, code, weak_instance, func_index);
}

WasmFunctionInfo GetWasmFunctionInfo(Isolate* isolate, Handle<Code> code) {
  FixedArray* deopt_data = code->deoptimization_data();
  DCHECK_LE(2, deopt_data->length());
  MaybeHandle<WasmInstanceObject> instance;
  Object* maybe_weak_instance = deopt_data->get(0);
  if (maybe_weak_instance->IsWeakCell()) {
    Object* maybe_instance = WeakCell::cast(maybe_weak_instance)->value();
    if (maybe_instance) {
      instance = handle(WasmInstanceObject::cast(maybe_instance), isolate);
    }
  }
  int func_index = Smi::ToInt(deopt_data->get(1));
  return {instance, func_index};
}

#undef TRACE

}  // namespace internal
}  // namespace v8

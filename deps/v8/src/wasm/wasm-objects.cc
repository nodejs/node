// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-objects.h"

#include "src/base/iterator.h"
#include "src/base/vector.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/code-factory.h"
#include "src/compiler/wasm-compiler.h"
#include "src/debug/debug-interface.h"
#include "src/logging/counters.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/managed-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/struct-inl.h"
#include "src/trap-handler/trap-handler.h"
#include "src/utils/utils.h"
#include "src/wasm/code-space-access.h"
#include "src/wasm/jump-table-assembler.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/module-instantiate.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-subtyping.h"
#include "src/wasm/wasm-value.h"

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
class WasmInstanceNativeAllocations {
 public:
  WasmInstanceNativeAllocations(Handle<WasmInstanceObject> instance,
                                size_t num_imported_functions,
                                size_t num_imported_mutable_globals,
                                size_t num_data_segments,
                                size_t num_elem_segments)
      : imported_function_targets_(new Address[num_imported_functions]),
        imported_mutable_globals_(new Address[num_imported_mutable_globals]),
        data_segment_starts_(new Address[num_data_segments]),
        data_segment_sizes_(new uint32_t[num_data_segments]),
        dropped_elem_segments_(new uint8_t[num_elem_segments]) {
    instance->set_imported_function_targets(imported_function_targets_.get());
    instance->set_imported_mutable_globals(imported_mutable_globals_.get());
    instance->set_data_segment_starts(data_segment_starts_.get());
    instance->set_data_segment_sizes(data_segment_sizes_.get());
    instance->set_dropped_elem_segments(dropped_elem_segments_.get());
  }

 private:
  const std::unique_ptr<Address[]> imported_function_targets_;
  const std::unique_ptr<Address[]> imported_mutable_globals_;
  const std::unique_ptr<Address[]> data_segment_starts_;
  const std::unique_ptr<uint32_t[]> data_segment_sizes_;
  const std::unique_ptr<uint8_t[]> dropped_elem_segments_;
};

size_t EstimateNativeAllocationsSize(const WasmModule* module) {
  size_t estimate =
      sizeof(WasmInstanceNativeAllocations) +
      (1 * kSystemPointerSize * module->num_imported_mutable_globals) +
      (2 * kSystemPointerSize * module->num_imported_functions) +
      ((kSystemPointerSize + sizeof(uint32_t) + sizeof(uint8_t)) *
       module->num_declared_data_segments);
  return estimate;
}

enum DispatchTableElements : int {
  kDispatchTableInstanceOffset,
  kDispatchTableIndexOffset,
  // Marker:
  kDispatchTableNumElements
};

}  // namespace

// static
Handle<WasmModuleObject> WasmModuleObject::New(
    Isolate* isolate, std::shared_ptr<wasm::NativeModule> native_module,
    Handle<Script> script) {
  Handle<FixedArray> export_wrappers = isolate->factory()->NewFixedArray(0);
  return New(isolate, std::move(native_module), script, export_wrappers);
}

// static
Handle<WasmModuleObject> WasmModuleObject::New(
    Isolate* isolate, std::shared_ptr<wasm::NativeModule> native_module,
    Handle<Script> script, Handle<FixedArray> export_wrappers) {
  Handle<Managed<wasm::NativeModule>> managed_native_module;
  if (script->type() == Script::TYPE_WASM) {
    managed_native_module = handle(
        Managed<wasm::NativeModule>::cast(script->wasm_managed_native_module()),
        isolate);
  } else {
    const WasmModule* module = native_module->module();
    size_t memory_estimate =
        native_module->committed_code_space() +
        wasm::WasmCodeManager::EstimateNativeModuleMetaDataSize(module);
    managed_native_module = Managed<wasm::NativeModule>::FromSharedPtr(
        isolate, memory_estimate, std::move(native_module));
  }
  Handle<WasmModuleObject> module_object = Handle<WasmModuleObject>::cast(
      isolate->factory()->NewJSObject(isolate->wasm_module_constructor()));
  module_object->set_export_wrappers(*export_wrappers);
  module_object->set_managed_native_module(*managed_native_module);
  module_object->set_script(*script);
  return module_object;
}

Handle<String> WasmModuleObject::ExtractUtf8StringFromModuleBytes(
    Isolate* isolate, Handle<WasmModuleObject> module_object,
    wasm::WireBytesRef ref, InternalizeString internalize) {
  base::Vector<const uint8_t> wire_bytes =
      module_object->native_module()->wire_bytes();
  return ExtractUtf8StringFromModuleBytes(isolate, wire_bytes, ref,
                                          internalize);
}

Handle<String> WasmModuleObject::ExtractUtf8StringFromModuleBytes(
    Isolate* isolate, base::Vector<const uint8_t> wire_bytes,
    wasm::WireBytesRef ref, InternalizeString internalize) {
  base::Vector<const uint8_t> name_vec =
      wire_bytes.SubVector(ref.offset(), ref.end_offset());
  // UTF8 validation happens at decode time.
  DCHECK(unibrow::Utf8::ValidateEncoding(name_vec.begin(), name_vec.length()));
  auto* factory = isolate->factory();
  return internalize
             ? factory->InternalizeUtf8String(
                   base::Vector<const char>::cast(name_vec))
             : factory
                   ->NewStringFromUtf8(base::Vector<const char>::cast(name_vec))
                   .ToHandleChecked();
}

MaybeHandle<String> WasmModuleObject::GetModuleNameOrNull(
    Isolate* isolate, Handle<WasmModuleObject> module_object) {
  const WasmModule* module = module_object->module();
  if (!module->name.is_set()) return {};
  return ExtractUtf8StringFromModuleBytes(isolate, module_object, module->name,
                                          kNoInternalize);
}

MaybeHandle<String> WasmModuleObject::GetFunctionNameOrNull(
    Isolate* isolate, Handle<WasmModuleObject> module_object,
    uint32_t func_index) {
  DCHECK_LT(func_index, module_object->module()->functions.size());
  wasm::WireBytesRef name =
      module_object->module()->lazily_generated_names.LookupFunctionName(
          wasm::ModuleWireBytes(module_object->native_module()->wire_bytes()),
          func_index);
  if (!name.is_set()) return {};
  return ExtractUtf8StringFromModuleBytes(isolate, module_object, name,
                                          kNoInternalize);
}

base::Vector<const uint8_t> WasmModuleObject::GetRawFunctionName(
    int func_index) {
  if (func_index == wasm::kAnonymousFuncIndex) {
    return base::Vector<const uint8_t>({nullptr, 0});
  }
  DCHECK_GT(module()->functions.size(), func_index);
  wasm::ModuleWireBytes wire_bytes(native_module()->wire_bytes());
  wasm::WireBytesRef name_ref =
      module()->lazily_generated_names.LookupFunctionName(wire_bytes,
                                                          func_index);
  wasm::WasmName name = wire_bytes.GetNameOrNull(name_ref);
  return base::Vector<const uint8_t>::cast(name);
}

Handle<WasmTableObject> WasmTableObject::New(
    Isolate* isolate, Handle<WasmInstanceObject> instance, wasm::ValueType type,
    uint32_t initial, bool has_maximum, uint32_t maximum,
    Handle<FixedArray>* entries, Handle<Object> initial_value) {
  // TODO(7748): Make this work with other types when spec clears up.
  {
    const WasmModule* module =
        instance.is_null() ? nullptr : instance->module();
    CHECK(wasm::WasmTable::IsValidTableType(type, module));
  }

  Handle<FixedArray> backing_store = isolate->factory()->NewFixedArray(initial);
  for (int i = 0; i < static_cast<int>(initial); ++i) {
    backing_store->set(i, *initial_value);
  }

  Handle<Object> max;
  if (has_maximum) {
    max = isolate->factory()->NewNumberFromUint(maximum);
  } else {
    max = isolate->factory()->undefined_value();
  }

  Handle<JSFunction> table_ctor(
      isolate->native_context()->wasm_table_constructor(), isolate);
  auto table_obj = Handle<WasmTableObject>::cast(
      isolate->factory()->NewJSObject(table_ctor));
  DisallowGarbageCollection no_gc;

  if (!instance.is_null()) table_obj->set_instance(*instance);
  table_obj->set_entries(*backing_store);
  table_obj->set_current_length(initial);
  table_obj->set_maximum_length(*max);
  table_obj->set_raw_type(static_cast<int>(type.raw_bit_field()));

  table_obj->set_dispatch_tables(ReadOnlyRoots(isolate).empty_fixed_array());
  if (entries != nullptr) {
    *entries = backing_store;
  }
  return Handle<WasmTableObject>::cast(table_obj);
}

void WasmTableObject::AddDispatchTable(Isolate* isolate,
                                       Handle<WasmTableObject> table_obj,
                                       Handle<WasmInstanceObject> instance,
                                       int table_index) {
  Handle<FixedArray> dispatch_tables(table_obj->dispatch_tables(), isolate);
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

int WasmTableObject::Grow(Isolate* isolate, Handle<WasmTableObject> table,
                          uint32_t count, Handle<Object> init_value) {
  uint32_t old_size = table->current_length();
  if (count == 0) return old_size;  // Degenerate case: nothing to do.

  // Check if growing by {count} is valid.
  uint32_t max_size;
  if (!table->maximum_length().ToUint32(&max_size)) {
    max_size = FLAG_wasm_max_table_size;
  }
  max_size = std::min(max_size, FLAG_wasm_max_table_size);
  DCHECK_LE(old_size, max_size);
  if (max_size - old_size < count) return -1;

  uint32_t new_size = old_size + count;
  // Even with 2x over-allocation, there should not be an integer overflow.
  STATIC_ASSERT(wasm::kV8MaxWasmTableSize <= kMaxInt / 2);
  DCHECK_GE(kMaxInt, new_size);
  int old_capacity = table->entries().length();
  if (new_size > static_cast<uint32_t>(old_capacity)) {
    int grow = static_cast<int>(new_size) - old_capacity;
    // Grow at least by the old capacity, to implement exponential growing.
    grow = std::max(grow, old_capacity);
    // Never grow larger than the max size.
    grow = std::min(grow, static_cast<int>(max_size - old_capacity));
    auto new_store = isolate->factory()->CopyFixedArrayAndGrow(
        handle(table->entries(), isolate), grow);
    table->set_entries(*new_store, WriteBarrierMode::UPDATE_WRITE_BARRIER);
  }
  table->set_current_length(new_size);

  Handle<FixedArray> dispatch_tables(table->dispatch_tables(), isolate);
  DCHECK_EQ(0, dispatch_tables->length() % kDispatchTableNumElements);
  // Tables are stored in the instance object, no code patching is
  // necessary. We simply have to grow the raw tables in each instance
  // that has imported this table.

  // TODO(titzer): replace the dispatch table with a weak list of all
  // the instances that import a given table.
  for (int i = 0; i < dispatch_tables->length();
       i += kDispatchTableNumElements) {
    int table_index =
        Smi::cast(dispatch_tables->get(i + kDispatchTableIndexOffset)).value();

    Handle<WasmInstanceObject> instance(
        WasmInstanceObject::cast(dispatch_tables->get(i)), isolate);

    DCHECK_EQ(old_size,
              instance->GetIndirectFunctionTable(isolate, table_index)->size());
    WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(
        instance, table_index, new_size);
  }

  for (uint32_t entry = old_size; entry < new_size; ++entry) {
    WasmTableObject::Set(isolate, table, entry, init_value);
  }
  return old_size;
}

bool WasmTableObject::IsInBounds(Isolate* isolate,
                                 Handle<WasmTableObject> table,
                                 uint32_t entry_index) {
  return entry_index < static_cast<uint32_t>(table->current_length());
}

bool WasmTableObject::IsValidElement(Isolate* isolate,
                                     Handle<WasmTableObject> table,
                                     Handle<Object> entry) {
  const char* error_message;
  const WasmModule* module =
      !table->instance().IsUndefined()
          ? WasmInstanceObject::cast(table->instance()).module()
          : nullptr;
  if (entry->IsWasmInternalFunction()) {
    entry =
        handle(Handle<WasmInternalFunction>::cast(entry)->external(), isolate);
  }
  return wasm::TypecheckJSObject(isolate, module, entry, table->type(),
                                 &error_message);
}

void WasmTableObject::SetFunctionTableEntry(Isolate* isolate,
                                            Handle<WasmTableObject> table,
                                            Handle<FixedArray> entries,
                                            int entry_index,
                                            Handle<Object> entry) {
  if (entry->IsNull(isolate)) {
    ClearDispatchTables(isolate, table, entry_index);  // Degenerate case.
    entries->set(entry_index, ReadOnlyRoots(isolate).null_value());
    return;
  }

  Handle<Object> external =
      handle(Handle<WasmInternalFunction>::cast(entry)->external(), isolate);

  if (WasmExportedFunction::IsWasmExportedFunction(*external)) {
    auto exported_function = Handle<WasmExportedFunction>::cast(external);
    Handle<WasmInstanceObject> target_instance(exported_function->instance(),
                                               isolate);
    int func_index = exported_function->function_index();
    auto* wasm_function = &target_instance->module()->functions[func_index];
    UpdateDispatchTables(isolate, *table, entry_index, wasm_function,
                         *target_instance);
  } else if (WasmJSFunction::IsWasmJSFunction(*external)) {
    UpdateDispatchTables(isolate, table, entry_index,
                         Handle<WasmJSFunction>::cast(external));
  } else {
    DCHECK(WasmCapiFunction::IsWasmCapiFunction(*external));
    UpdateDispatchTables(isolate, table, entry_index,
                         Handle<WasmCapiFunction>::cast(external));
  }
  entries->set(entry_index, *entry);
}

void WasmTableObject::Set(Isolate* isolate, Handle<WasmTableObject> table,
                          uint32_t index, Handle<Object> entry) {
  // Callers need to perform bounds checks, type check, and error handling.
  DCHECK(IsInBounds(isolate, table, index));
  DCHECK(IsValidElement(isolate, table, entry));

  Handle<FixedArray> entries(table->entries(), isolate);
  // The FixedArray is addressed with int's.
  int entry_index = static_cast<int>(index);

  switch (table->type().heap_representation()) {
    case wasm::HeapType::kAny:
      entries->set(entry_index, *entry);
      return;
    case wasm::HeapType::kFunc:
      SetFunctionTableEntry(isolate, table, entries, entry_index, entry);
      return;
    case wasm::HeapType::kEq:
    case wasm::HeapType::kData:
    case wasm::HeapType::kArray:
    case wasm::HeapType::kI31:
      // TODO(7748): Implement once we have struct/arrays/i31ref tables.
      UNREACHABLE();
    case wasm::HeapType::kBottom:
      UNREACHABLE();
    default:
      DCHECK(!table->instance().IsUndefined());
      // TODO(7748): Relax this once we have struct/array/i31ref tables.
      DCHECK(WasmInstanceObject::cast(table->instance())
                 .module()
                 ->has_signature(table->type().ref_index()));
      SetFunctionTableEntry(isolate, table, entries, entry_index, entry);
      return;
  }
}

Handle<Object> WasmTableObject::Get(Isolate* isolate,
                                    Handle<WasmTableObject> table,
                                    uint32_t index) {
  Handle<FixedArray> entries(table->entries(), isolate);
  // Callers need to perform bounds checks and error handling.
  DCHECK(IsInBounds(isolate, table, index));

  // The FixedArray is addressed with int's.
  int entry_index = static_cast<int>(index);

  Handle<Object> entry(entries->get(entry_index), isolate);

  if (entry->IsNull(isolate)) {
    return entry;
  }

  switch (table->type().heap_representation()) {
    case wasm::HeapType::kAny:
      return entry;
    case wasm::HeapType::kFunc:
      if (entry->IsWasmInternalFunction()) return entry;
      break;
    case wasm::HeapType::kEq:
    case wasm::HeapType::kI31:
    case wasm::HeapType::kData:
    case wasm::HeapType::kArray:
      // TODO(7748): Implement once we have a story for struct/arrays/i31ref in
      // JS.
      UNIMPLEMENTED();
    case wasm::HeapType::kBottom:
      UNREACHABLE();
    default:
      DCHECK(!table->instance().IsUndefined());
      // TODO(7748): Relax this once we have struct/array/i31ref tables.
      DCHECK(WasmInstanceObject::cast(table->instance())
                 .module()
                 ->has_signature(table->type().ref_index()));
      if (entry->IsWasmInternalFunction()) return entry;
      break;
  }

  // {entry} is not a valid entry in the table. It has to be a placeholder
  // for lazy initialization.
  Handle<Tuple2> tuple = Handle<Tuple2>::cast(entry);
  auto instance = handle(WasmInstanceObject::cast(tuple->value1()), isolate);
  int function_index = Smi::cast(tuple->value2()).value();

  // Check if we already compiled a wrapper for the function but did not store
  // it in the table slot yet.
  Handle<WasmInternalFunction> internal =
      WasmInstanceObject::GetOrCreateWasmInternalFunction(isolate, instance,
                                                          function_index);
  entries->set(entry_index, *internal);
  return internal;
}

void WasmTableObject::Fill(Isolate* isolate, Handle<WasmTableObject> table,
                           uint32_t start, Handle<Object> entry,
                           uint32_t count) {
  // Bounds checks must be done by the caller.
  DCHECK_LE(start, table->current_length());
  DCHECK_LE(count, table->current_length());
  DCHECK_LE(start + count, table->current_length());

  for (uint32_t i = 0; i < count; i++) {
    WasmTableObject::Set(isolate, table, start + i, entry);
  }
}

void WasmTableObject::UpdateDispatchTables(Isolate* isolate,
                                           WasmTableObject table,
                                           int entry_index,
                                           const wasm::WasmFunction* func,
                                           WasmInstanceObject target_instance) {
  DisallowGarbageCollection no_gc;

  // We simply need to update the IFTs for each instance that imports
  // this table.
  FixedArray dispatch_tables = table.dispatch_tables();
  DCHECK_EQ(0, dispatch_tables.length() % kDispatchTableNumElements);

  Object call_ref =
      func->imported
          // The function in the target instance was imported. Use its imports
          // table, which contains a tuple needed by the import wrapper.
          ? target_instance.imported_function_refs().get(func->func_index)
          // For wasm functions, just pass the target instance.
          : target_instance;
  Address call_target = target_instance.GetCallTarget(func->func_index);

  int original_sig_id = func->sig_index;

  for (int i = 0, len = dispatch_tables.length(); i < len;
       i += kDispatchTableNumElements) {
    int table_index =
        Smi::cast(dispatch_tables.get(i + kDispatchTableIndexOffset)).value();
    WasmInstanceObject instance = WasmInstanceObject::cast(
        dispatch_tables.get(i + kDispatchTableInstanceOffset));
    const WasmModule* module = instance.module();
    // Try to avoid the signature map lookup by checking if the signature in
    // {module} at {original_sig_id} matches {func->sig}.
    int sig_id;
    // TODO(7748): wasm-gc signatures cannot be canonicalized this way because
    // references could wrongly be detected as identical.
    if (module->has_signature(original_sig_id) &&
        *module->signature(original_sig_id) == *func->sig) {
      sig_id = module->canonicalized_type_ids[original_sig_id];
      DCHECK_EQ(sig_id, module->signature_map.Find(*func->sig));
    } else {
      // Note that {SignatureMap::Find} may return {-1} if the signature is
      // not found; it will simply never match any check.
      sig_id = module->signature_map.Find(*func->sig);
    }
    WasmIndirectFunctionTable ift = WasmIndirectFunctionTable::cast(
        instance.indirect_function_tables().get(table_index));
    ift.Set(entry_index, sig_id, call_target, call_ref);
  }
}

void WasmTableObject::UpdateDispatchTables(Isolate* isolate,
                                           Handle<WasmTableObject> table,
                                           int entry_index,
                                           Handle<WasmJSFunction> function) {
  // We simply need to update the IFTs for each instance that imports
  // this table.
  Handle<FixedArray> dispatch_tables(table->dispatch_tables(), isolate);
  DCHECK_EQ(0, dispatch_tables->length() % kDispatchTableNumElements);

  for (int i = 0; i < dispatch_tables->length();
       i += kDispatchTableNumElements) {
    int table_index =
        Smi::cast(dispatch_tables->get(i + kDispatchTableIndexOffset)).value();
    Handle<WasmInstanceObject> instance(
        WasmInstanceObject::cast(
            dispatch_tables->get(i + kDispatchTableInstanceOffset)),
        isolate);
    WasmInstanceObject::ImportWasmJSFunctionIntoTable(
        isolate, instance, table_index, entry_index, function);
  }
}

void WasmTableObject::UpdateDispatchTables(
    Isolate* isolate, Handle<WasmTableObject> table, int entry_index,
    Handle<WasmCapiFunction> capi_function) {
  // We simply need to update the IFTs for each instance that imports
  // this table.
  Handle<FixedArray> dispatch_tables(table->dispatch_tables(), isolate);
  DCHECK_EQ(0, dispatch_tables->length() % kDispatchTableNumElements);

  // Reconstruct signature.
  // TODO(jkummerow): Unify with "SignatureHelper" in c-api.cc.
  PodArray<wasm::ValueType> serialized_sig =
      capi_function->GetSerializedSignature();
  int total_count = serialized_sig.length() - 1;
  std::unique_ptr<wasm::ValueType[]> reps(new wasm::ValueType[total_count]);
  int result_count;
  static const wasm::ValueType kMarker = wasm::kWasmVoid;
  for (int i = 0, j = 0; i <= total_count; i++) {
    if (serialized_sig.get(i) == kMarker) {
      result_count = i;
      continue;
    }
    reps[j++] = serialized_sig.get(i);
  }
  int param_count = total_count - result_count;
  wasm::FunctionSig sig(result_count, param_count, reps.get());

  for (int i = 0; i < dispatch_tables->length();
       i += kDispatchTableNumElements) {
    int table_index =
        Smi::cast(dispatch_tables->get(i + kDispatchTableIndexOffset)).value();
    Handle<WasmInstanceObject> instance(
        WasmInstanceObject::cast(
            dispatch_tables->get(i + kDispatchTableInstanceOffset)),
        isolate);
    wasm::NativeModule* native_module =
        instance->module_object().native_module();
    wasm::WasmImportWrapperCache* cache = native_module->import_wrapper_cache();
    auto kind = compiler::WasmImportCallKind::kWasmToCapi;
    wasm::WasmCode* wasm_code =
        cache->MaybeGet(kind, &sig, param_count, wasm::kNoSuspend);
    if (wasm_code == nullptr) {
      wasm::WasmCodeRefScope code_ref_scope;
      wasm::WasmImportWrapperCache::ModificationScope cache_scope(cache);
      wasm_code = compiler::CompileWasmCapiCallWrapper(native_module, &sig);
      wasm::WasmImportWrapperCache::CacheKey key(kind, &sig, param_count,
                                                 wasm::kNoSuspend);
      cache_scope[key] = wasm_code;
      wasm_code->IncRef();
      isolate->counters()->wasm_generated_code_size()->Increment(
          wasm_code->instructions().length());
      isolate->counters()->wasm_reloc_size()->Increment(
          wasm_code->reloc_info().length());
    }
    // Note that {SignatureMap::Find} may return {-1} if the signature is
    // not found; it will simply never match any check.
    auto sig_id = instance->module()->signature_map.Find(sig);
    instance->GetIndirectFunctionTable(isolate, table_index)
        ->Set(entry_index, sig_id, wasm_code->instruction_start(),
              WasmCapiFunctionData::cast(
                  capi_function->shared().function_data(kAcquireLoad))
                  .internal()
                  .ref());
  }
}

void WasmTableObject::ClearDispatchTables(Isolate* isolate,
                                          Handle<WasmTableObject> table,
                                          int index) {
  Handle<FixedArray> dispatch_tables(table->dispatch_tables(), isolate);
  DCHECK_EQ(0, dispatch_tables->length() % kDispatchTableNumElements);
  for (int i = 0; i < dispatch_tables->length();
       i += kDispatchTableNumElements) {
    int table_index =
        Smi::cast(dispatch_tables->get(i + kDispatchTableIndexOffset)).value();
    Handle<WasmInstanceObject> target_instance(
        WasmInstanceObject::cast(
            dispatch_tables->get(i + kDispatchTableInstanceOffset)),
        isolate);
    Handle<WasmIndirectFunctionTable> function_table =
        target_instance->GetIndirectFunctionTable(isolate, table_index);
    DCHECK_LT(index, function_table->size());
    function_table->Clear(index);
  }
}

void WasmTableObject::SetFunctionTablePlaceholder(
    Isolate* isolate, Handle<WasmTableObject> table, int entry_index,
    Handle<WasmInstanceObject> instance, int func_index) {
  // Put (instance, func_index) as a Tuple2 into the entry_index.
  // The {WasmExportedFunction} will be created lazily.
  // Allocate directly in old space as the tuples are typically long-lived, and
  // we create many of them, which would result in lots of GC when initializing
  // large tables.
  Handle<Tuple2> tuple = isolate->factory()->NewTuple2(
      instance, Handle<Smi>(Smi::FromInt(func_index), isolate),
      AllocationType::kOld);
  table->entries().set(entry_index, *tuple);
}

void WasmTableObject::GetFunctionTableEntry(
    Isolate* isolate, const WasmModule* module, Handle<WasmTableObject> table,
    int entry_index, bool* is_valid, bool* is_null,
    MaybeHandle<WasmInstanceObject>* instance, int* function_index,
    MaybeHandle<WasmJSFunction>* maybe_js_function) {
  DCHECK(wasm::IsSubtypeOf(table->type(), wasm::kWasmFuncRef, module));
  DCHECK_LT(entry_index, table->current_length());
  // We initialize {is_valid} with {true}. We may change it later.
  *is_valid = true;
  Handle<Object> element(table->entries().get(entry_index), isolate);

  *is_null = element->IsNull(isolate);
  if (*is_null) return;

  if (element->IsWasmInternalFunction()) {
    element = handle(Handle<WasmInternalFunction>::cast(element)->external(),
                     isolate);
  }
  if (WasmExportedFunction::IsWasmExportedFunction(*element)) {
    auto target_func = Handle<WasmExportedFunction>::cast(element);
    *instance = handle(target_func->instance(), isolate);
    *function_index = target_func->function_index();
    *maybe_js_function = MaybeHandle<WasmJSFunction>();
    return;
  }
  if (WasmJSFunction::IsWasmJSFunction(*element)) {
    *instance = MaybeHandle<WasmInstanceObject>();
    *maybe_js_function = Handle<WasmJSFunction>::cast(element);
    return;
  }
  if (element->IsTuple2()) {
    auto tuple = Handle<Tuple2>::cast(element);
    *instance = handle(WasmInstanceObject::cast(tuple->value1()), isolate);
    *function_index = Smi::cast(tuple->value2()).value();
    *maybe_js_function = MaybeHandle<WasmJSFunction>();
    return;
  }
  *is_valid = false;
}

namespace {
class IftNativeAllocations {
 public:
  IftNativeAllocations(Handle<WasmIndirectFunctionTable> table, uint32_t size)
      : sig_ids_(size), targets_(size) {
    table->set_sig_ids(sig_ids_.data());
    table->set_targets(targets_.data());
  }

  static size_t SizeInMemory(uint32_t size) {
    return size * (sizeof(Address) + sizeof(uint32_t));
  }

  void resize(Handle<WasmIndirectFunctionTable> table, uint32_t new_size) {
    DCHECK_GE(new_size, sig_ids_.size());
    DCHECK_EQ(this, Managed<IftNativeAllocations>::cast(
                        table->managed_native_allocations())
                        .raw());
    sig_ids_.resize(new_size);
    targets_.resize(new_size);
    table->set_sig_ids(sig_ids_.data());
    table->set_targets(targets_.data());
  }

 private:
  std::vector<uint32_t> sig_ids_;
  std::vector<Address> targets_;
};
}  // namespace

Handle<WasmIndirectFunctionTable> WasmIndirectFunctionTable::New(
    Isolate* isolate, uint32_t size) {
  auto refs = isolate->factory()->NewFixedArray(static_cast<int>(size));
  auto table = Handle<WasmIndirectFunctionTable>::cast(
      isolate->factory()->NewStruct(WASM_INDIRECT_FUNCTION_TABLE_TYPE));
  table->set_size(size);
  table->set_refs(*refs);
  auto native_allocations = Managed<IftNativeAllocations>::Allocate(
      isolate, IftNativeAllocations::SizeInMemory(size), table, size);
  table->set_managed_native_allocations(*native_allocations);
  for (uint32_t i = 0; i < size; ++i) {
    table->Clear(i);
  }
  return table;
}
void WasmIndirectFunctionTable::Set(uint32_t index, int sig_id,
                                    Address call_target, Object ref) {
  sig_ids()[index] = sig_id;
  targets()[index] = call_target;
  refs().set(index, ref);
}

void WasmIndirectFunctionTable::Clear(uint32_t index) {
  sig_ids()[index] = -1;
  targets()[index] = 0;
  refs().set(
      index,
      ReadOnlyRoots(GetIsolateFromWritableObject(*this)).undefined_value());
}

void WasmIndirectFunctionTable::Resize(Isolate* isolate,
                                       Handle<WasmIndirectFunctionTable> table,
                                       uint32_t new_size) {
  uint32_t old_size = table->size();
  if (old_size >= new_size) return;  // Nothing to do.

  table->set_size(new_size);

  // Grow table exponentially to guarantee amortized constant allocation and gc
  // time.
  Handle<FixedArray> old_refs(table->refs(), isolate);
  // Since we might have overallocated, {old_capacity} might be different than
  // {old_size}.
  uint32_t old_capacity = old_refs->length();
  // If we have enough capacity, there is no need to reallocate.
  if (new_size <= old_capacity) return;
  uint32_t new_capacity = std::max(2 * old_capacity, new_size);

  Managed<IftNativeAllocations>::cast(table->managed_native_allocations())
      .raw()
      ->resize(table, new_capacity);

  Handle<FixedArray> new_refs = isolate->factory()->CopyFixedArrayAndGrow(
      old_refs, static_cast<int>(new_capacity - old_capacity));
  table->set_refs(*new_refs);
  for (uint32_t i = old_capacity; i < new_capacity; ++i) {
    table->Clear(i);
  }
}

namespace {

void SetInstanceMemory(Handle<WasmInstanceObject> instance,
                       Handle<JSArrayBuffer> buffer) {
  bool is_wasm_module = instance->module()->origin == wasm::kWasmOrigin;
  bool use_trap_handler =
      instance->module_object().native_module()->bounds_checks() ==
      wasm::kTrapHandler;
  // Wasm modules compiled to use the trap handler don't have bounds checks,
  // so they must have a memory that has guard regions.
  CHECK_IMPLIES(is_wasm_module && use_trap_handler,
                buffer->GetBackingStore()->has_guard_regions());

  instance->SetRawMemory(reinterpret_cast<byte*>(buffer->backing_store()),
                         buffer->byte_length());
#if DEBUG
  if (!FLAG_mock_arraybuffer_allocator) {
    // To flush out bugs earlier, in DEBUG mode, check that all pages of the
    // memory are accessible by reading and writing one byte on each page.
    // Don't do this if the mock ArrayBuffer allocator is enabled.
    byte* mem_start = instance->memory_start();
    size_t mem_size = instance->memory_size();
    for (size_t offset = 0; offset < mem_size; offset += wasm::kWasmPageSize) {
      byte val = mem_start[offset];
      USE(val);
      mem_start[offset] = val;
    }
  }
#endif
}
}  // namespace

MaybeHandle<WasmMemoryObject> WasmMemoryObject::New(
    Isolate* isolate, MaybeHandle<JSArrayBuffer> maybe_buffer, int maximum) {
  Handle<JSArrayBuffer> buffer;
  if (!maybe_buffer.ToHandle(&buffer)) {
    // If no buffer was provided, create a zero-length one.
    auto backing_store =
        BackingStore::AllocateWasmMemory(isolate, 0, 0, SharedFlag::kNotShared);
    if (!backing_store) return {};
    buffer = isolate->factory()->NewJSArrayBuffer(std::move(backing_store));
  }

  Handle<JSFunction> memory_ctor(
      isolate->native_context()->wasm_memory_constructor(), isolate);

  auto memory_object = Handle<WasmMemoryObject>::cast(
      isolate->factory()->NewJSObject(memory_ctor, AllocationType::kOld));
  memory_object->set_array_buffer(*buffer);
  memory_object->set_maximum_pages(maximum);

  if (buffer->is_shared()) {
    auto backing_store = buffer->GetBackingStore();
    backing_store->AttachSharedWasmMemoryObject(isolate, memory_object);
  }

  // For debugging purposes we memorize a link from the JSArrayBuffer
  // to it's owning WasmMemoryObject instance.
  Handle<Symbol> symbol = isolate->factory()->array_buffer_wasm_memory_symbol();
  JSObject::SetProperty(isolate, buffer, symbol, memory_object).Check();

  return memory_object;
}

MaybeHandle<WasmMemoryObject> WasmMemoryObject::New(Isolate* isolate,
                                                    int initial, int maximum,
                                                    SharedFlag shared) {
  bool has_maximum = maximum != kNoMaximum;
  int heuristic_maximum = maximum;
  if (!has_maximum) {
    heuristic_maximum = static_cast<int>(wasm::max_mem_pages());
  }

#ifdef V8_TARGET_ARCH_32_BIT
  // On 32-bit platforms we need an heuristic here to balance overall memory
  // and address space consumption.
  constexpr int kGBPages = 1024 * 1024 * 1024 / wasm::kWasmPageSize;
  if (initial > kGBPages) {
    // We always allocate at least the initial size.
    heuristic_maximum = initial;
  } else if (has_maximum) {
    // We try to reserve the maximum, but at most 1GB to avoid OOMs.
    heuristic_maximum = std::min(maximum, kGBPages);
  } else if (shared == SharedFlag::kShared) {
    // If shared memory has no maximum, we use an implicit maximum of 1GB.
    heuristic_maximum = kGBPages;
  } else {
    // If non-shared memory has no maximum, we only allocate the initial size
    // and then grow with realloc.
    heuristic_maximum = initial;
  }
#endif

  auto backing_store = BackingStore::AllocateWasmMemory(
      isolate, initial, heuristic_maximum, shared);

  if (!backing_store) return {};

  Handle<JSArrayBuffer> buffer =
      (shared == SharedFlag::kShared)
          ? isolate->factory()->NewJSSharedArrayBuffer(std::move(backing_store))
          : isolate->factory()->NewJSArrayBuffer(std::move(backing_store));

  return New(isolate, buffer, maximum);
}

void WasmMemoryObject::AddInstance(Isolate* isolate,
                                   Handle<WasmMemoryObject> memory,
                                   Handle<WasmInstanceObject> instance) {
  Handle<WeakArrayList> old_instances =
      memory->has_instances()
          ? Handle<WeakArrayList>(memory->instances(), isolate)
          : handle(ReadOnlyRoots(isolate->heap()).empty_weak_array_list(),
                   isolate);
  Handle<WeakArrayList> new_instances = WeakArrayList::Append(
      isolate, old_instances, MaybeObjectHandle::Weak(instance));
  memory->set_instances(*new_instances);
  Handle<JSArrayBuffer> buffer(memory->array_buffer(), isolate);
  SetInstanceMemory(instance, buffer);
}

void WasmMemoryObject::update_instances(Isolate* isolate,
                                        Handle<JSArrayBuffer> buffer) {
  if (has_instances()) {
    Handle<WeakArrayList> instances(this->instances(), isolate);
    for (int i = 0; i < instances->length(); i++) {
      MaybeObject elem = instances->Get(i);
      HeapObject heap_object;
      if (elem->GetHeapObjectIfWeak(&heap_object)) {
        Handle<WasmInstanceObject> instance(
            WasmInstanceObject::cast(heap_object), isolate);
        SetInstanceMemory(instance, buffer);
      } else {
        DCHECK(elem->IsCleared());
      }
    }
  }
  set_array_buffer(*buffer);
}

// static
int32_t WasmMemoryObject::Grow(Isolate* isolate,
                               Handle<WasmMemoryObject> memory_object,
                               uint32_t pages) {
  TRACE_EVENT0("v8.wasm", "wasm.GrowMemory");
  Handle<JSArrayBuffer> old_buffer(memory_object->array_buffer(), isolate);
  // Any buffer used as an asmjs memory cannot be detached, and
  // therefore this memory cannot be grown.
  if (old_buffer->is_asmjs_memory()) return -1;

  std::shared_ptr<BackingStore> backing_store = old_buffer->GetBackingStore();
  if (!backing_store) return -1;

  // Check for maximum memory size.
  // Note: The {wasm::max_mem_pages()} limit is already checked in
  // {BackingStore::CopyWasmMemory}, and is irrelevant for
  // {GrowWasmMemoryInPlace} because memory is never allocated with more
  // capacity than that limit.
  size_t old_size = old_buffer->byte_length();
  DCHECK_EQ(0, old_size % wasm::kWasmPageSize);
  size_t old_pages = old_size / wasm::kWasmPageSize;
  uint32_t max_pages = wasm::kSpecMaxMemoryPages;
  if (memory_object->has_maximum_pages()) {
    DCHECK_GE(max_pages, memory_object->maximum_pages());
    max_pages = static_cast<uint32_t>(memory_object->maximum_pages());
  }
  DCHECK_GE(max_pages, old_pages);
  if (pages > max_pages - old_pages) return -1;

  base::Optional<size_t> result_inplace =
      backing_store->GrowWasmMemoryInPlace(isolate, pages, max_pages);
  // Handle shared memory first.
  if (old_buffer->is_shared()) {
    // Shared memories can only be grown in place; no copying.
    if (!result_inplace.has_value()) {
      // There are different limits per platform, thus crash if the correctness
      // fuzzer is running.
      if (FLAG_correctness_fuzzer_suppressions) {
        FATAL("could not grow wasm memory");
      }
      return -1;
    }

    BackingStore::BroadcastSharedWasmMemoryGrow(isolate, backing_store);
    // Broadcasting the update should update this memory object too.
    CHECK_NE(*old_buffer, memory_object->array_buffer());
    size_t new_pages = result_inplace.value() + pages;
    // If the allocation succeeded, then this can't possibly overflow:
    size_t new_byte_length = new_pages * wasm::kWasmPageSize;
    // This is a less than check, as it is not guaranteed that the SAB
    // length here will be equal to the stashed length above as calls to
    // grow the same memory object can come in from different workers.
    // It is also possible that a call to Grow was in progress when
    // handling this call.
    CHECK_LE(new_byte_length, memory_object->array_buffer().byte_length());
    // As {old_pages} was read racefully, we return here the synchronized
    // value provided by {GrowWasmMemoryInPlace}, to provide the atomic
    // read-modify-write behavior required by the spec.
    return static_cast<int32_t>(result_inplace.value());  // success
  }

  // Check if the non-shared memory could grow in-place.
  if (result_inplace.has_value()) {
    // Detach old and create a new one with the grown backing store.
    old_buffer->Detach(true);
    Handle<JSArrayBuffer> new_buffer =
        isolate->factory()->NewJSArrayBuffer(std::move(backing_store));
    memory_object->update_instances(isolate, new_buffer);
    // For debugging purposes we memorize a link from the JSArrayBuffer
    // to it's owning WasmMemoryObject instance.
    Handle<Symbol> symbol =
        isolate->factory()->array_buffer_wasm_memory_symbol();
    JSObject::SetProperty(isolate, new_buffer, symbol, memory_object).Check();
    DCHECK_EQ(result_inplace.value(), old_pages);
    return static_cast<int32_t>(result_inplace.value());  // success
  }

  size_t new_pages = old_pages + pages;
  DCHECK_LT(old_pages, new_pages);
  // Try allocating a new backing store and copying.
  // To avoid overall quadratic complexity of many small grow operations, we
  // grow by at least 0.5 MB + 12.5% of the existing memory size.
  // These numbers are kept small because we must be careful about address
  // space consumption on 32-bit platforms.
  size_t min_growth = old_pages + 8 + (old_pages >> 3);
  size_t new_capacity = std::max(new_pages, min_growth);
  std::unique_ptr<BackingStore> new_backing_store =
      backing_store->CopyWasmMemory(isolate, new_pages, new_capacity);
  if (!new_backing_store) {
    // Crash on out-of-memory if the correctness fuzzer is running.
    if (FLAG_correctness_fuzzer_suppressions) {
      FATAL("could not grow wasm memory");
    }
    return -1;
  }

  // Detach old and create a new one with the new backing store.
  old_buffer->Detach(true);
  Handle<JSArrayBuffer> new_buffer =
      isolate->factory()->NewJSArrayBuffer(std::move(new_backing_store));
  memory_object->update_instances(isolate, new_buffer);
  // For debugging purposes we memorize a link from the JSArrayBuffer
  // to it's owning WasmMemoryObject instance.
  Handle<Symbol> symbol = isolate->factory()->array_buffer_wasm_memory_symbol();
  JSObject::SetProperty(isolate, new_buffer, symbol, memory_object).Check();
  return static_cast<int32_t>(old_pages);  // success
}

// static
MaybeHandle<WasmGlobalObject> WasmGlobalObject::New(
    Isolate* isolate, Handle<WasmInstanceObject> instance,
    MaybeHandle<JSArrayBuffer> maybe_untagged_buffer,
    MaybeHandle<FixedArray> maybe_tagged_buffer, wasm::ValueType type,
    int32_t offset, bool is_mutable) {
  Handle<JSFunction> global_ctor(
      isolate->native_context()->wasm_global_constructor(), isolate);
  auto global_obj = Handle<WasmGlobalObject>::cast(
      isolate->factory()->NewJSObject(global_ctor));
  {
    // Disallow GC until all fields have acceptable types.
    DisallowGarbageCollection no_gc;
    if (!instance.is_null()) global_obj->set_instance(*instance);
    global_obj->set_type(type);
    global_obj->set_offset(offset);
    global_obj->set_is_mutable(is_mutable);
  }

  if (type.is_reference()) {
    DCHECK(maybe_untagged_buffer.is_null());
    Handle<FixedArray> tagged_buffer;
    if (!maybe_tagged_buffer.ToHandle(&tagged_buffer)) {
      // If no buffer was provided, create one.
      tagged_buffer =
          isolate->factory()->NewFixedArray(1, AllocationType::kOld);
      CHECK_EQ(offset, 0);
    }
    global_obj->set_tagged_buffer(*tagged_buffer);
  } else {
    DCHECK(maybe_tagged_buffer.is_null());
    uint32_t type_size = type.value_kind_size();

    Handle<JSArrayBuffer> untagged_buffer;
    if (!maybe_untagged_buffer.ToHandle(&untagged_buffer)) {
      MaybeHandle<JSArrayBuffer> result =
          isolate->factory()->NewJSArrayBufferAndBackingStore(
              offset + type_size, InitializedFlag::kZeroInitialized);

      if (!result.ToHandle(&untagged_buffer)) return {};
    }

    // Check that the offset is in bounds.
    CHECK_LE(offset + type_size, untagged_buffer->byte_length());

    global_obj->set_untagged_buffer(*untagged_buffer);
  }

  return global_obj;
}

FunctionTargetAndRef::FunctionTargetAndRef(
    Handle<WasmInstanceObject> target_instance, int target_func_index) {
  Isolate* isolate = target_instance->native_context().GetIsolate();
  if (target_func_index <
      static_cast<int>(target_instance->module()->num_imported_functions)) {
    // The function in the target instance was imported. Use its imports table,
    // which contains a tuple needed by the import wrapper.
    ImportedFunctionEntry entry(target_instance, target_func_index);
    ref_ = handle(entry.object_ref(), isolate);
    call_target_ = entry.target();
  } else {
    // The function in the target instance was not imported.
    ref_ = target_instance;
    call_target_ = target_instance->GetCallTarget(target_func_index);
  }
}

void ImportedFunctionEntry::SetWasmToJs(
    Isolate* isolate, Handle<JSReceiver> callable,
    const wasm::WasmCode* wasm_to_js_wrapper, Handle<HeapObject> suspender) {
  TRACE_IFT("Import callable 0x%" PRIxPTR "[%d] = {callable=0x%" PRIxPTR
            ", target=%p}\n",
            instance_->ptr(), index_, callable->ptr(),
            wasm_to_js_wrapper->instructions().begin());
  DCHECK(wasm_to_js_wrapper->kind() == wasm::WasmCode::kWasmToJsWrapper ||
         wasm_to_js_wrapper->kind() == wasm::WasmCode::kWasmToCapiWrapper);
  Handle<WasmApiFunctionRef> ref =
      isolate->factory()->NewWasmApiFunctionRef(callable, suspender);
  instance_->imported_function_refs().set(index_, *ref);
  instance_->imported_function_targets()[index_] =
      wasm_to_js_wrapper->instruction_start();
}

void ImportedFunctionEntry::SetWasmToWasm(WasmInstanceObject instance,
                                          Address call_target) {
  TRACE_IFT("Import Wasm 0x%" PRIxPTR "[%d] = {instance=0x%" PRIxPTR
            ", target=0x%" PRIxPTR "}\n",
            instance_->ptr(), index_, instance.ptr(), call_target);
  instance_->imported_function_refs().set(index_, instance);
  instance_->imported_function_targets()[index_] = call_target;
}

// Returns an empty Object() if no callable is available, a JSReceiver
// otherwise.
Object ImportedFunctionEntry::maybe_callable() {
  Object value = object_ref();
  if (!value.IsWasmApiFunctionRef()) return Object();
  return JSReceiver::cast(WasmApiFunctionRef::cast(value).callable());
}

JSReceiver ImportedFunctionEntry::callable() {
  return JSReceiver::cast(WasmApiFunctionRef::cast(object_ref()).callable());
}

Object ImportedFunctionEntry::object_ref() {
  return instance_->imported_function_refs().get(index_);
}

Address ImportedFunctionEntry::target() {
  return instance_->imported_function_targets()[index_];
}

// static
constexpr uint16_t WasmInstanceObject::kTaggedFieldOffsets[];

// static
bool WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(
    Handle<WasmInstanceObject> instance, int table_index,
    uint32_t minimum_size) {
  Isolate* isolate = instance->GetIsolate();
  DCHECK_LT(table_index, instance->indirect_function_tables().length());
  Handle<WasmIndirectFunctionTable> table =
      instance->GetIndirectFunctionTable(isolate, table_index);
  WasmIndirectFunctionTable::Resize(isolate, table, minimum_size);
  if (table_index == 0) {
    instance->SetIndirectFunctionTableShortcuts(isolate);
  }
  return true;
}

void WasmInstanceObject::SetRawMemory(byte* mem_start, size_t mem_size) {
  CHECK_LE(mem_size, wasm::max_mem_bytes());
  set_memory_start(mem_start);
  set_memory_size(mem_size);
}

const WasmModule* WasmInstanceObject::module() {
  return module_object().module();
}

Handle<WasmInstanceObject> WasmInstanceObject::New(
    Isolate* isolate, Handle<WasmModuleObject> module_object) {
  Handle<JSFunction> instance_cons(
      isolate->native_context()->wasm_instance_constructor(), isolate);
  Handle<JSObject> instance_object =
      isolate->factory()->NewJSObject(instance_cons, AllocationType::kOld);

  Handle<WasmInstanceObject> instance(
      WasmInstanceObject::cast(*instance_object), isolate);
  instance->clear_padding();

  // Initialize the imported function arrays.
  auto module = module_object->module();
  auto num_imported_functions = module->num_imported_functions;
  auto num_imported_mutable_globals = module->num_imported_mutable_globals;
  auto num_data_segments = module->num_declared_data_segments;
  size_t native_allocations_size = EstimateNativeAllocationsSize(module);
  auto native_allocations = Managed<WasmInstanceNativeAllocations>::Allocate(
      isolate, native_allocations_size, instance, num_imported_functions,
      num_imported_mutable_globals, num_data_segments,
      module->elem_segments.size());
  instance->set_managed_native_allocations(*native_allocations);

  Handle<FixedArray> imported_function_refs =
      isolate->factory()->NewFixedArray(num_imported_functions);
  instance->set_imported_function_refs(*imported_function_refs);

  instance->SetRawMemory(reinterpret_cast<byte*>(EmptyBackingStoreBuffer()), 0);
  instance->set_isolate_root(isolate->isolate_root());
  instance->set_stack_limit_address(
      isolate->stack_guard()->address_of_jslimit());
  instance->set_real_stack_limit_address(
      isolate->stack_guard()->address_of_real_jslimit());
  instance->set_new_allocation_limit_address(
      isolate->heap()->NewSpaceAllocationLimitAddress());
  instance->set_new_allocation_top_address(
      isolate->heap()->NewSpaceAllocationTopAddress());
  instance->set_old_allocation_limit_address(
      isolate->heap()->OldSpaceAllocationLimitAddress());
  instance->set_old_allocation_top_address(
      isolate->heap()->OldSpaceAllocationTopAddress());
  instance->set_globals_start(nullptr);
  instance->set_indirect_function_table_size(0);
  instance->set_indirect_function_table_refs(
      ReadOnlyRoots(isolate).empty_fixed_array());
  instance->set_indirect_function_table_sig_ids(nullptr);
  instance->set_indirect_function_table_targets(nullptr);
  instance->set_native_context(*isolate->native_context());
  instance->set_module_object(*module_object);
  instance->set_jump_table_start(
      module_object->native_module()->jump_table_start());
  instance->set_hook_on_function_call_address(
      isolate->debug()->hook_on_function_call_address());
  instance->set_managed_object_maps(*isolate->factory()->empty_fixed_array());
  instance->set_feedback_vectors(*isolate->factory()->empty_fixed_array());
  instance->set_tiering_budget_array(
      module_object->native_module()->tiering_budget_array());
  instance->set_break_on_entry(module_object->script().break_on_entry());

  // Insert the new instance into the scripts weak list of instances. This list
  // is used for breakpoints affecting all instances belonging to the script.
  if (module_object->script().type() == Script::TYPE_WASM) {
    Handle<WeakArrayList> weak_instance_list(
        module_object->script().wasm_weak_instance_list(), isolate);
    weak_instance_list = WeakArrayList::Append(
        isolate, weak_instance_list, MaybeObjectHandle::Weak(instance));
    module_object->script().set_wasm_weak_instance_list(*weak_instance_list);
  }

  InitDataSegmentArrays(instance, module_object);
  InitElemSegmentArrays(instance, module_object);

  return instance;
}

// static
void WasmInstanceObject::InitDataSegmentArrays(
    Handle<WasmInstanceObject> instance,
    Handle<WasmModuleObject> module_object) {
  auto module = module_object->module();
  auto wire_bytes = module_object->native_module()->wire_bytes();
  auto num_data_segments = module->num_declared_data_segments;
  // The number of declared data segments will be zero if there is no DataCount
  // section. These arrays will not be allocated nor initialized in that case,
  // since they cannot be used (since the validator checks that number of
  // declared data segments when validating the memory.init and memory.drop
  // instructions).
  DCHECK(num_data_segments == 0 ||
         num_data_segments == module->data_segments.size());
  for (size_t i = 0; i < num_data_segments; ++i) {
    const wasm::WasmDataSegment& segment = module->data_segments[i];
    // Initialize the pointer and size of passive segments.
    auto source_bytes = wire_bytes.SubVector(segment.source.offset(),
                                             segment.source.end_offset());
    instance->data_segment_starts()[i] =
        reinterpret_cast<Address>(source_bytes.begin());
    // Set the active segments to being already dropped, since memory.init on
    // a dropped passive segment and an active segment have the same
    // behavior.
    instance->data_segment_sizes()[i] =
        segment.active ? 0 : source_bytes.length();
  }
}

void WasmInstanceObject::InitElemSegmentArrays(
    Handle<WasmInstanceObject> instance,
    Handle<WasmModuleObject> module_object) {
  auto module = module_object->module();
  auto num_elem_segments = module->elem_segments.size();
  for (size_t i = 0; i < num_elem_segments; ++i) {
    instance->dropped_elem_segments()[i] =
        module->elem_segments[i].status ==
                wasm::WasmElemSegment::kStatusDeclarative
            ? 1
            : 0;
  }
}

Address WasmInstanceObject::GetCallTarget(uint32_t func_index) {
  wasm::NativeModule* native_module = module_object().native_module();
  if (func_index < native_module->num_imported_functions()) {
    return imported_function_targets()[func_index];
  }
  return native_module->GetCallTargetForFunction(func_index);
}

Handle<WasmIndirectFunctionTable> WasmInstanceObject::GetIndirectFunctionTable(
    Isolate* isolate, uint32_t table_index) {
  DCHECK_LT(table_index, indirect_function_tables().length());
  return handle(WasmIndirectFunctionTable::cast(
                    indirect_function_tables().get(table_index)),
                isolate);
}

void WasmInstanceObject::SetIndirectFunctionTableShortcuts(Isolate* isolate) {
  if (indirect_function_tables().length() > 0 &&
      indirect_function_tables().get(0).IsWasmIndirectFunctionTable()) {
    HandleScope scope(isolate);
    Handle<WasmIndirectFunctionTable> table0 =
        GetIndirectFunctionTable(isolate, 0);
    set_indirect_function_table_size(table0->size());
    set_indirect_function_table_refs(table0->refs());
    set_indirect_function_table_sig_ids(table0->sig_ids());
    set_indirect_function_table_targets(table0->targets());
  }
}

// static
bool WasmInstanceObject::CopyTableEntries(Isolate* isolate,
                                          Handle<WasmInstanceObject> instance,
                                          uint32_t table_dst_index,
                                          uint32_t table_src_index,
                                          uint32_t dst, uint32_t src,
                                          uint32_t count) {
  CHECK_LT(table_dst_index, instance->tables().length());
  CHECK_LT(table_src_index, instance->tables().length());
  auto table_dst = handle(
      WasmTableObject::cast(instance->tables().get(table_dst_index)), isolate);
  auto table_src = handle(
      WasmTableObject::cast(instance->tables().get(table_src_index)), isolate);
  uint32_t max_dst = table_dst->current_length();
  uint32_t max_src = table_src->current_length();
  bool copy_backward = src < dst;
  if (!base::IsInBounds(dst, count, max_dst) ||
      !base::IsInBounds(src, count, max_src)) {
    return false;
  }

  // no-op
  if ((dst == src && table_dst_index == table_src_index) || count == 0) {
    return true;
  }

  for (uint32_t i = 0; i < count; ++i) {
    uint32_t src_index = copy_backward ? (src + count - i - 1) : src + i;
    uint32_t dst_index = copy_backward ? (dst + count - i - 1) : dst + i;
    auto value = WasmTableObject::Get(isolate, table_src, src_index);
    WasmTableObject::Set(isolate, table_dst, dst_index, value);
  }
  return true;
}

// static
bool WasmInstanceObject::InitTableEntries(Isolate* isolate,
                                          Handle<WasmInstanceObject> instance,
                                          uint32_t table_index,
                                          uint32_t segment_index, uint32_t dst,
                                          uint32_t src, uint32_t count) {
  // Note that this implementation just calls through to module instantiation.
  // This is intentional, so that the runtime only depends on the object
  // methods, and not the module instantiation logic.
  return wasm::LoadElemSegment(isolate, instance, table_index, segment_index,
                               dst, src, count);
}

MaybeHandle<WasmInternalFunction> WasmInstanceObject::GetWasmInternalFunction(
    Isolate* isolate, Handle<WasmInstanceObject> instance, int index) {
  MaybeHandle<WasmInternalFunction> result;
  if (instance->has_wasm_internal_functions()) {
    Object val = instance->wasm_internal_functions().get(index);
    if (!val.IsUndefined(isolate)) {
      result = Handle<WasmInternalFunction>(WasmInternalFunction::cast(val),
                                            isolate);
    }
  }
  return result;
}

Handle<WasmInternalFunction>
WasmInstanceObject::GetOrCreateWasmInternalFunction(
    Isolate* isolate, Handle<WasmInstanceObject> instance, int function_index) {
  MaybeHandle<WasmInternalFunction> maybe_result =
      WasmInstanceObject::GetWasmInternalFunction(isolate, instance,
                                                  function_index);

  Handle<WasmInternalFunction> result;
  if (maybe_result.ToHandle(&result)) {
    return result;
  }

  Handle<WasmModuleObject> module_object(instance->module_object(), isolate);
  const WasmModule* module = module_object->module();
  const WasmFunction& function = module->functions[function_index];
  int wrapper_index =
      GetExportWrapperIndex(module, function.sig_index, function.imported);
  DCHECK_EQ(wrapper_index,
            GetExportWrapperIndex(module, function.sig, function.imported));

  Handle<Object> entry =
      FixedArray::get(module_object->export_wrappers(), wrapper_index, isolate);

  Handle<CodeT> wrapper;
  if (entry->IsCodeT()) {
    wrapper = Handle<CodeT>::cast(entry);
  } else {
    // The wrapper may not exist yet if no function in the exports section has
    // this signature. We compile it and store the wrapper in the module for
    // later use.
    wrapper = ToCodeT(
        wasm::JSToWasmWrapperCompilationUnit::CompileJSToWasmWrapper(
            isolate, function.sig, instance->module(), function.imported),
        isolate);
    module_object->export_wrappers().set(wrapper_index, *wrapper);
  }
  auto external = Handle<WasmExternalFunction>::cast(WasmExportedFunction::New(
      isolate, instance, function_index,
      static_cast<int>(function.sig->parameter_count()), wrapper));
  result =
      WasmInternalFunction::FromExternal(external, isolate).ToHandleChecked();

  WasmInstanceObject::SetWasmInternalFunction(isolate, instance, function_index,
                                              result);
  return result;
}

void WasmInstanceObject::SetWasmInternalFunction(
    Isolate* isolate, Handle<WasmInstanceObject> instance, int index,
    Handle<WasmInternalFunction> val) {
  Handle<FixedArray> functions;
  if (!instance->has_wasm_internal_functions()) {
    // Lazily allocate the wasm external functions array.
    functions = isolate->factory()->NewFixedArray(
        static_cast<int>(instance->module()->functions.size()));
    instance->set_wasm_internal_functions(*functions);
  } else {
    functions =
        Handle<FixedArray>(instance->wasm_internal_functions(), isolate);
  }
  functions->set(index, *val);
}

// static
void WasmInstanceObject::ImportWasmJSFunctionIntoTable(
    Isolate* isolate, Handle<WasmInstanceObject> instance, int table_index,
    int entry_index, Handle<WasmJSFunction> js_function) {
  // Deserialize the signature encapsulated with the {WasmJSFunction}.
  // Note that {SignatureMap::Find} may return {-1} if the signature is
  // not found; it will simply never match any check.
  Zone zone(isolate->allocator(), ZONE_NAME);
  const wasm::FunctionSig* sig = js_function->GetSignature(&zone);
  auto sig_id = instance->module()->signature_map.Find(*sig);

  // Compile a wrapper for the target callable.
  Handle<JSReceiver> callable(js_function->GetCallable(), isolate);
  wasm::WasmCodeRefScope code_ref_scope;
  Address call_target = kNullAddress;
  if (sig_id >= 0) {
    wasm::NativeModule* native_module =
        instance->module_object().native_module();
    // TODO(wasm): Cache and reuse wrapper code, to avoid repeated compilation
    // and permissions switching.
    const wasm::WasmFeatures enabled = native_module->enabled_features();
    auto resolved = compiler::ResolveWasmImportCall(
        callable, sig, instance->module(), enabled);
    compiler::WasmImportCallKind kind = resolved.kind;
    callable = resolved.callable;  // Update to ultimate target.
    DCHECK_NE(compiler::WasmImportCallKind::kLinkError, kind);
    wasm::CompilationEnv env = native_module->CreateCompilationEnv();
    // {expected_arity} should only be used if kind != kJSFunctionArityMismatch.
    int expected_arity = -1;
    if (kind == compiler::WasmImportCallKind ::kJSFunctionArityMismatch) {
      expected_arity = Handle<JSFunction>::cast(callable)
                           ->shared()
                           .internal_formal_parameter_count_without_receiver();
    }
    wasm::Suspend suspend =
        resolved.suspender.is_null() || resolved.suspender->IsUndefined()
            ? wasm::kNoSuspend
            : wasm::kSuspend;
    // TODO(manoskouk): Reuse js_function->wasm_to_js_wrapper_code().
    wasm::WasmCompilationResult result = compiler::CompileWasmImportCallWrapper(
        &env, kind, sig, false, expected_arity, suspend);
    wasm::CodeSpaceWriteScope write_scope(native_module);
    std::unique_ptr<wasm::WasmCode> wasm_code = native_module->AddCode(
        result.func_index, result.code_desc, result.frame_slot_count,
        result.tagged_parameter_slots,
        result.protected_instructions_data.as_vector(),
        result.source_positions.as_vector(), GetCodeKind(result),
        wasm::ExecutionTier::kNone, wasm::kNoDebugging);
    wasm::WasmCode* published_code =
        native_module->PublishCode(std::move(wasm_code));
    isolate->counters()->wasm_generated_code_size()->Increment(
        published_code->instructions().length());
    isolate->counters()->wasm_reloc_size()->Increment(
        published_code->reloc_info().length());
    call_target = published_code->instruction_start();
  }

  // Update the dispatch table.
  Handle<HeapObject> suspender = handle(js_function->GetSuspender(), isolate);
  Handle<WasmApiFunctionRef> ref =
      isolate->factory()->NewWasmApiFunctionRef(callable, suspender);
  WasmIndirectFunctionTable::cast(
      instance->indirect_function_tables().get(table_index))
      .Set(entry_index, sig_id, call_target, *ref);
}

// static
uint8_t* WasmInstanceObject::GetGlobalStorage(
    Handle<WasmInstanceObject> instance, const wasm::WasmGlobal& global) {
  DCHECK(!global.type.is_reference());
  if (global.mutability && global.imported) {
    return reinterpret_cast<byte*>(
        instance->imported_mutable_globals()[global.index]);
  } else {
    return instance->globals_start() + global.offset;
  }
}

// static
std::pair<Handle<FixedArray>, uint32_t>
WasmInstanceObject::GetGlobalBufferAndIndex(Handle<WasmInstanceObject> instance,
                                            const wasm::WasmGlobal& global) {
  DCHECK(global.type.is_reference());
  Isolate* isolate = instance->GetIsolate();
  if (global.mutability && global.imported) {
    Handle<FixedArray> buffer(
        FixedArray::cast(
            instance->imported_mutable_globals_buffers().get(global.index)),
        isolate);
    Address idx = instance->imported_mutable_globals()[global.index];
    DCHECK_LE(idx, std::numeric_limits<uint32_t>::max());
    return {buffer, static_cast<uint32_t>(idx)};
  }
  return {handle(instance->tagged_globals_buffer(), isolate), global.offset};
}

// static
wasm::WasmValue WasmInstanceObject::GetGlobalValue(
    Handle<WasmInstanceObject> instance, const wasm::WasmGlobal& global) {
  Isolate* isolate = instance->GetIsolate();
  if (global.type.is_reference()) {
    Handle<FixedArray> global_buffer;  // The buffer of the global.
    uint32_t global_index = 0;         // The index into the buffer.
    std::tie(global_buffer, global_index) =
        GetGlobalBufferAndIndex(instance, global);
    return wasm::WasmValue(handle(global_buffer->get(global_index), isolate),
                           global.type);
  }
  Address ptr = reinterpret_cast<Address>(GetGlobalStorage(instance, global));
  using wasm::Simd128;
  switch (global.type.kind()) {
#define CASE_TYPE(valuetype, ctype) \
  case wasm::valuetype:             \
    return wasm::WasmValue(base::ReadUnalignedValue<ctype>(ptr));
    FOREACH_WASMVALUE_CTYPES(CASE_TYPE)
#undef CASE_TYPE
    default:
      UNREACHABLE();
  }
}

wasm::WasmValue WasmStruct::GetFieldValue(uint32_t index) {
  wasm::ValueType field_type = type()->field(index);
  int field_offset = WasmStruct::kHeaderSize + type()->field_offset(index);
  Address field_address = GetFieldAddress(field_offset);
  using wasm::Simd128;
  switch (field_type.kind()) {
#define CASE_TYPE(valuetype, ctype) \
  case wasm::valuetype:             \
    return wasm::WasmValue(base::ReadUnalignedValue<ctype>(field_address));
    CASE_TYPE(kI8, int8_t)
    CASE_TYPE(kI16, int16_t)
    FOREACH_WASMVALUE_CTYPES(CASE_TYPE)
#undef CASE_TYPE
    case wasm::kRef:
    case wasm::kOptRef: {
      Handle<Object> ref(TaggedField<Object>::load(*this, field_offset),
                         GetIsolateFromWritableObject(*this));
      return wasm::WasmValue(ref, field_type);
    }
    case wasm::kRtt:
      // TODO(7748): Expose RTTs to DevTools.
      UNIMPLEMENTED();
    case wasm::kVoid:
    case wasm::kBottom:
      UNREACHABLE();
  }
}

wasm::WasmValue WasmArray::GetElement(uint32_t index) {
  wasm::ValueType element_type = type()->element_type();
  int element_offset =
      WasmArray::kHeaderSize + index * element_type.value_kind_size();
  Address element_address = GetFieldAddress(element_offset);
  using wasm::Simd128;
  switch (element_type.kind()) {
#define CASE_TYPE(value_type, ctype) \
  case wasm::value_type:             \
    return wasm::WasmValue(base::ReadUnalignedValue<ctype>(element_address));
    CASE_TYPE(kI8, int8_t)
    CASE_TYPE(kI16, int16_t)
    FOREACH_WASMVALUE_CTYPES(CASE_TYPE)
#undef CASE_TYPE
    case wasm::kRef:
    case wasm::kOptRef: {
      Handle<Object> ref(TaggedField<Object>::load(*this, element_offset),
                         GetIsolateFromWritableObject(*this));
      return wasm::WasmValue(ref, element_type);
    }
    case wasm::kRtt:
      // TODO(7748): Expose RTTs to DevTools.
      UNIMPLEMENTED();
    case wasm::kVoid:
    case wasm::kBottom:
      UNREACHABLE();
  }
}

// static
Handle<WasmTagObject> WasmTagObject::New(Isolate* isolate,
                                         const wasm::FunctionSig* sig,
                                         Handle<HeapObject> tag) {
  Handle<JSFunction> tag_cons(isolate->native_context()->wasm_tag_constructor(),
                              isolate);

  // Serialize the signature.
  DCHECK_EQ(0, sig->return_count());
  DCHECK_LE(sig->parameter_count(), std::numeric_limits<int>::max());
  int sig_size = static_cast<int>(sig->parameter_count());
  Handle<PodArray<wasm::ValueType>> serialized_sig =
      PodArray<wasm::ValueType>::New(isolate, sig_size, AllocationType::kOld);
  int index = 0;  // Index into the {PodArray} above.
  for (wasm::ValueType param : sig->parameters()) {
    serialized_sig->set(index++, param);
  }

  Handle<JSObject> tag_object =
      isolate->factory()->NewJSObject(tag_cons, AllocationType::kOld);
  Handle<WasmTagObject> tag_wrapper = Handle<WasmTagObject>::cast(tag_object);
  tag_wrapper->set_serialized_signature(*serialized_sig);
  tag_wrapper->set_tag(*tag);

  return tag_wrapper;
}

// TODO(9495): Update this if function type variance is introduced.
bool WasmTagObject::MatchesSignature(const wasm::FunctionSig* sig) {
  DCHECK_EQ(0, sig->return_count());
  DCHECK_LE(sig->parameter_count(), std::numeric_limits<int>::max());
  int sig_size = static_cast<int>(sig->parameter_count());
  if (sig_size != serialized_signature().length()) return false;
  for (int index = 0; index < sig_size; ++index) {
    if (sig->GetParam(index) != serialized_signature().get(index)) {
      return false;
    }
  }
  return true;
}

// TODO(9495): Update this if function type variance is introduced.
bool WasmCapiFunction::MatchesSignature(const wasm::FunctionSig* sig) const {
  // TODO(jkummerow): Unify with "SignatureHelper" in c-api.cc.
  int param_count = static_cast<int>(sig->parameter_count());
  int result_count = static_cast<int>(sig->return_count());
  PodArray<wasm::ValueType> serialized_sig =
      shared().wasm_capi_function_data().serialized_signature();
  if (param_count + result_count + 1 != serialized_sig.length()) return false;
  int serialized_index = 0;
  for (int i = 0; i < result_count; i++, serialized_index++) {
    if (sig->GetReturn(i) != serialized_sig.get(serialized_index)) {
      return false;
    }
  }
  if (serialized_sig.get(serialized_index) != wasm::kWasmVoid) return false;
  serialized_index++;
  for (int i = 0; i < param_count; i++, serialized_index++) {
    if (sig->GetParam(i) != serialized_sig.get(serialized_index)) return false;
  }
  return true;
}

// static
Handle<WasmExceptionPackage> WasmExceptionPackage::New(
    Isolate* isolate, Handle<WasmExceptionTag> exception_tag, int size) {
  Handle<FixedArray> values = isolate->factory()->NewFixedArray(size);
  return New(isolate, exception_tag, values);
}

Handle<WasmExceptionPackage> WasmExceptionPackage::New(
    Isolate* isolate, Handle<WasmExceptionTag> exception_tag,
    Handle<FixedArray> values) {
  Handle<JSObject> exception = isolate->factory()->NewWasmExceptionError(
      MessageTemplate::kWasmExceptionError);
  CHECK(!Object::SetProperty(isolate, exception,
                             isolate->factory()->wasm_exception_tag_symbol(),
                             exception_tag, StoreOrigin::kMaybeKeyed,
                             Just(ShouldThrow::kThrowOnError))
             .is_null());
  CHECK(!Object::SetProperty(isolate, exception,
                             isolate->factory()->wasm_exception_values_symbol(),
                             values, StoreOrigin::kMaybeKeyed,
                             Just(ShouldThrow::kThrowOnError))
             .is_null());
  return Handle<WasmExceptionPackage>::cast(exception);
}

// static
Handle<Object> WasmExceptionPackage::GetExceptionTag(
    Isolate* isolate, Handle<WasmExceptionPackage> exception_package) {
  Handle<Object> tag;
  if (JSReceiver::GetProperty(isolate, exception_package,
                              isolate->factory()->wasm_exception_tag_symbol())
          .ToHandle(&tag)) {
    return tag;
  }
  return ReadOnlyRoots(isolate).undefined_value_handle();
}

// static
Handle<Object> WasmExceptionPackage::GetExceptionValues(
    Isolate* isolate, Handle<WasmExceptionPackage> exception_package) {
  Handle<Object> values;
  if (JSReceiver::GetProperty(
          isolate, exception_package,
          isolate->factory()->wasm_exception_values_symbol())
          .ToHandle(&values)) {
    DCHECK_IMPLIES(!values->IsUndefined(), values->IsFixedArray());
    return values;
  }
  return ReadOnlyRoots(isolate).undefined_value_handle();
}

void EncodeI32ExceptionValue(Handle<FixedArray> encoded_values,
                             uint32_t* encoded_index, uint32_t value) {
  encoded_values->set((*encoded_index)++, Smi::FromInt(value >> 16));
  encoded_values->set((*encoded_index)++, Smi::FromInt(value & 0xffff));
}

void EncodeI64ExceptionValue(Handle<FixedArray> encoded_values,
                             uint32_t* encoded_index, uint64_t value) {
  EncodeI32ExceptionValue(encoded_values, encoded_index,
                          static_cast<uint32_t>(value >> 32));
  EncodeI32ExceptionValue(encoded_values, encoded_index,
                          static_cast<uint32_t>(value));
}

void DecodeI32ExceptionValue(Handle<FixedArray> encoded_values,
                             uint32_t* encoded_index, uint32_t* value) {
  uint32_t msb = Smi::cast(encoded_values->get((*encoded_index)++)).value();
  uint32_t lsb = Smi::cast(encoded_values->get((*encoded_index)++)).value();
  *value = (msb << 16) | (lsb & 0xffff);
}

void DecodeI64ExceptionValue(Handle<FixedArray> encoded_values,
                             uint32_t* encoded_index, uint64_t* value) {
  uint32_t lsb = 0, msb = 0;
  DecodeI32ExceptionValue(encoded_values, encoded_index, &msb);
  DecodeI32ExceptionValue(encoded_values, encoded_index, &lsb);
  *value = (static_cast<uint64_t>(msb) << 32) | static_cast<uint64_t>(lsb);
}

// static
Handle<WasmContinuationObject> WasmContinuationObject::New(
    Isolate* isolate, std::unique_ptr<wasm::StackMemory> stack,
    Handle<HeapObject> parent) {
  stack->jmpbuf()->stack_limit = stack->jslimit();
  stack->jmpbuf()->sp = stack->base();
  stack->jmpbuf()->fp = kNullAddress;
  wasm::JumpBuffer* jmpbuf = stack->jmpbuf();
  size_t external_size = stack->owned_size();
  Handle<Foreign> managed_stack = Managed<wasm::StackMemory>::FromUniquePtr(
      isolate, external_size, std::move(stack));
  Handle<Foreign> foreign_jmpbuf =
      isolate->factory()->NewForeign(reinterpret_cast<Address>(jmpbuf));
  Handle<WasmContinuationObject> result = Handle<WasmContinuationObject>::cast(
      isolate->factory()->NewStruct(WASM_CONTINUATION_OBJECT_TYPE));
  result->set_jmpbuf(*foreign_jmpbuf);
  result->set_stack(*managed_stack);
  result->set_parent(*parent);
  return result;
}

// static
Handle<WasmContinuationObject> WasmContinuationObject::New(
    Isolate* isolate, std::unique_ptr<wasm::StackMemory> stack) {
  auto parent = ReadOnlyRoots(isolate).undefined_value();
  return New(isolate, std::move(stack), handle(parent, isolate));
}

// static
Handle<WasmContinuationObject> WasmContinuationObject::New(
    Isolate* isolate, Handle<WasmContinuationObject> parent) {
  auto stack =
      std::unique_ptr<wasm::StackMemory>(wasm::StackMemory::New(isolate));
  return New(isolate, std::move(stack), parent);
}

// static
Handle<WasmSuspenderObject> WasmSuspenderObject::New(Isolate* isolate) {
  Handle<JSFunction> suspender_cons(
      isolate->native_context()->wasm_suspender_constructor(), isolate);
  // Suspender objects should be at least as long-lived as the instances of
  // which it will wrap the imports/exports, allocate in old space too.
  auto suspender = Handle<WasmSuspenderObject>::cast(
      isolate->factory()->NewJSObject(suspender_cons, AllocationType::kOld));
  suspender->set_state(Inactive);
  // Instantiate the callable object which resumes this Suspender. This will be
  // used implicitly as the onFulfilled callback of the returned JS promise.
  Handle<WasmOnFulfilledData> function_data =
      isolate->factory()->NewWasmOnFulfilledData(suspender);
  Handle<SharedFunctionInfo> shared =
      isolate->factory()->NewSharedFunctionInfoForWasmOnFulfilled(
          function_data);
  Handle<Context> context(isolate->native_context());
  Handle<JSObject> resume =
      Factory::JSFunctionBuilder{isolate, shared, context}.Build();
  suspender->set_resume(*resume);
  return suspender;
}

#ifdef DEBUG

namespace {

constexpr uint32_t kBytesPerExceptionValuesArrayElement = 2;

size_t ComputeEncodedElementSize(wasm::ValueType type) {
  size_t byte_size = type.value_kind_size();
  DCHECK_EQ(byte_size % kBytesPerExceptionValuesArrayElement, 0);
  DCHECK_LE(1, byte_size / kBytesPerExceptionValuesArrayElement);
  return byte_size / kBytesPerExceptionValuesArrayElement;
}

}  // namespace

#endif  // DEBUG

// static
uint32_t WasmExceptionPackage::GetEncodedSize(const wasm::WasmTag* tag) {
  const wasm::WasmTagSig* sig = tag->sig;
  uint32_t encoded_size = 0;
  for (size_t i = 0; i < sig->parameter_count(); ++i) {
    switch (sig->GetParam(i).kind()) {
      case wasm::kI32:
      case wasm::kF32:
        DCHECK_EQ(2, ComputeEncodedElementSize(sig->GetParam(i)));
        encoded_size += 2;
        break;
      case wasm::kI64:
      case wasm::kF64:
        DCHECK_EQ(4, ComputeEncodedElementSize(sig->GetParam(i)));
        encoded_size += 4;
        break;
      case wasm::kS128:
        DCHECK_EQ(8, ComputeEncodedElementSize(sig->GetParam(i)));
        encoded_size += 8;
        break;
      case wasm::kRef:
      case wasm::kOptRef:
        encoded_size += 1;
        break;
      case wasm::kRtt:
      case wasm::kVoid:
      case wasm::kBottom:
      case wasm::kI8:
      case wasm::kI16:
        UNREACHABLE();
    }
  }
  return encoded_size;
}

bool WasmExportedFunction::IsWasmExportedFunction(Object object) {
  if (!object.IsJSFunction()) return false;
  JSFunction js_function = JSFunction::cast(object);
  CodeT code = js_function.code();
  if (CodeKind::JS_TO_WASM_FUNCTION != code.kind() &&
      code.builtin_id() != Builtin::kGenericJSToWasmWrapper &&
      code.builtin_id() != Builtin::kWasmReturnPromiseOnSuspend) {
    return false;
  }
  DCHECK(js_function.shared().HasWasmExportedFunctionData());
  return true;
}

bool WasmCapiFunction::IsWasmCapiFunction(Object object) {
  if (!object.IsJSFunction()) return false;
  JSFunction js_function = JSFunction::cast(object);
  // TODO(jkummerow): Enable this when there is a JavaScript wrapper
  // able to call this function.
  // if (js_function->code()->kind() != CodeKind::WASM_TO_CAPI_FUNCTION) {
  //   return false;
  // }
  // DCHECK(js_function->shared()->HasWasmCapiFunctionData());
  // return true;
  return js_function.shared().HasWasmCapiFunctionData();
}

Handle<WasmCapiFunction> WasmCapiFunction::New(
    Isolate* isolate, Address call_target, Handle<Foreign> embedder_data,
    Handle<PodArray<wasm::ValueType>> serialized_signature) {
  // TODO(jkummerow): Install a JavaScript wrapper. For now, calling
  // these functions directly is unsupported; they can only be called
  // from Wasm code.

  // To support simulator builds, we potentially have to redirect the
  // call target (which is an address pointing into the C++ binary).
  call_target = ExternalReference::Create(call_target).address();

  // TODO(7748): Support proper typing for external functions. That requires
  // global (cross-module) canonicalization of signatures/RTTs.
  Handle<Map> rtt = isolate->factory()->wasm_internal_function_map();
  Handle<WasmCapiFunctionData> fun_data =
      isolate->factory()->NewWasmCapiFunctionData(
          call_target, embedder_data, BUILTIN_CODE(isolate, Illegal), rtt,
          serialized_signature);
  Handle<SharedFunctionInfo> shared =
      isolate->factory()->NewSharedFunctionInfoForWasmCapiFunction(fun_data);
  Handle<JSFunction> result =
      Factory::JSFunctionBuilder{isolate, shared, isolate->native_context()}
          .Build();
  fun_data->internal().set_external(*result);
  return Handle<WasmCapiFunction>::cast(result);
}

WasmInstanceObject WasmExportedFunction::instance() {
  return shared().wasm_exported_function_data().instance();
}

int WasmExportedFunction::function_index() {
  return shared().wasm_exported_function_data().function_index();
}

Handle<WasmExportedFunction> WasmExportedFunction::New(
    Isolate* isolate, Handle<WasmInstanceObject> instance, int func_index,
    int arity, Handle<CodeT> export_wrapper) {
  DCHECK(
      CodeKind::JS_TO_WASM_FUNCTION == export_wrapper->kind() ||
      (export_wrapper->is_builtin() &&
       (export_wrapper->builtin_id() == Builtin::kGenericJSToWasmWrapper ||
        export_wrapper->builtin_id() == Builtin::kWasmReturnPromiseOnSuspend)));
  int num_imported_functions = instance->module()->num_imported_functions;
  Handle<Object> ref =
      func_index >= num_imported_functions
          ? instance
          : handle(instance->imported_function_refs().get(func_index), isolate);

  Factory* factory = isolate->factory();
  const wasm::FunctionSig* sig = instance->module()->functions[func_index].sig;
  Address call_target = instance->GetCallTarget(func_index);
  Handle<Map> rtt;
  bool has_gc =
      instance->module_object().native_module()->enabled_features().has_gc();
  if (has_gc) {
    int sig_index = instance->module()->functions[func_index].sig_index;
    // TODO(7748): Create funcref RTTs lazily?
    rtt = handle(Map::cast(instance->managed_object_maps().get(sig_index)),
                 isolate);
  } else {
    rtt = factory->wasm_internal_function_map();
  }
  Handle<WasmExportedFunctionData> function_data =
      factory->NewWasmExportedFunctionData(
          export_wrapper, instance, call_target, ref, func_index,
          reinterpret_cast<Address>(sig), wasm::kGenericWrapperBudget, rtt);

  MaybeHandle<String> maybe_name;
  bool is_asm_js_module = instance->module_object().is_asm_js();
  if (is_asm_js_module) {
    // We can use the function name only for asm.js. For WebAssembly, the
    // function name is specified as the function_index.toString().
    maybe_name = WasmModuleObject::GetFunctionNameOrNull(
        isolate, handle(instance->module_object(), isolate), func_index);
  }
  Handle<String> name;
  if (!maybe_name.ToHandle(&name)) {
    base::EmbeddedVector<char, 16> buffer;
    int length = SNPrintF(buffer, "%d", func_index);
    name = factory
               ->NewStringFromOneByte(
                   base::Vector<uint8_t>::cast(buffer.SubVector(0, length)))
               .ToHandleChecked();
  }
  Handle<Map> function_map;
  switch (instance->module()->origin) {
    case wasm::kWasmOrigin:
      function_map = isolate->wasm_exported_function_map();
      break;
    case wasm::kAsmJsSloppyOrigin:
      function_map = isolate->sloppy_function_map();
      break;
    case wasm::kAsmJsStrictOrigin:
      function_map = isolate->strict_function_map();
      break;
  }

  Handle<NativeContext> context(isolate->native_context());
  Handle<SharedFunctionInfo> shared =
      factory->NewSharedFunctionInfoForWasmExportedFunction(name,
                                                            function_data);
  Handle<JSFunction> js_function =
      Factory::JSFunctionBuilder{isolate, shared, context}
          .set_map(function_map)
          .Build();

  // According to the spec, exported functions should not have a [[Construct]]
  // method. This does not apply to functions exported from asm.js however.
  DCHECK_EQ(is_asm_js_module, js_function->IsConstructor());
  shared->set_length(arity);
  shared->set_internal_formal_parameter_count(JSParameterCount(arity));
  shared->set_script(instance->module_object().script());
  function_data->internal().set_external(*js_function);
  return Handle<WasmExportedFunction>::cast(js_function);
}

Address WasmExportedFunction::GetWasmCallTarget() {
  return instance().GetCallTarget(function_index());
}

const wasm::FunctionSig* WasmExportedFunction::sig() {
  return instance().module()->functions[function_index()].sig;
}

bool WasmExportedFunction::MatchesSignature(
    const WasmModule* other_module, const wasm::FunctionSig* other_sig) {
  const wasm::FunctionSig* sig = this->sig();
  if (sig->parameter_count() != other_sig->parameter_count() ||
      sig->return_count() != other_sig->return_count()) {
    return false;
  }

  for (int i = 0; i < sig->all().size(); i++) {
    if (!wasm::EquivalentTypes(sig->all()[i], other_sig->all()[i],
                               this->instance().module(), other_module)) {
      return false;
    }
  }
  return true;
}

// static
std::unique_ptr<char[]> WasmExportedFunction::GetDebugName(
    const wasm::FunctionSig* sig) {
  constexpr const char kPrefix[] = "js-to-wasm:";
  // prefix + parameters + delimiter + returns + zero byte
  size_t len = strlen(kPrefix) + sig->all().size() + 2;
  auto buffer = base::OwnedVector<char>::New(len);
  memcpy(buffer.start(), kPrefix, strlen(kPrefix));
  PrintSignature(buffer.as_vector() + strlen(kPrefix), sig);
  return buffer.ReleaseData();
}

// static
bool WasmJSFunction::IsWasmJSFunction(Object object) {
  if (!object.IsJSFunction()) return false;
  JSFunction js_function = JSFunction::cast(object);
  return js_function.shared().HasWasmJSFunctionData();
}

Handle<WasmJSFunction> WasmJSFunction::New(Isolate* isolate,
                                           const wasm::FunctionSig* sig,
                                           Handle<JSReceiver> callable,
                                           Handle<HeapObject> suspender) {
  DCHECK_LE(sig->all().size(), kMaxInt);
  int sig_size = static_cast<int>(sig->all().size());
  int return_count = static_cast<int>(sig->return_count());
  int parameter_count = static_cast<int>(sig->parameter_count());
  Handle<PodArray<wasm::ValueType>> serialized_sig =
      PodArray<wasm::ValueType>::New(isolate, sig_size, AllocationType::kOld);
  if (sig_size > 0) {
    serialized_sig->copy_in(0, sig->all().begin(), sig_size);
  }
  // TODO(wasm): Think about caching and sharing the JS-to-JS wrappers per
  // signature instead of compiling a new one for every instantiation.
  Handle<CodeT> wrapper_code = ToCodeT(
      compiler::CompileJSToJSWrapper(isolate, sig, nullptr).ToHandleChecked(),
      isolate);

  // WasmJSFunctions use on-heap Code objects as call targets, so we can't
  // cache the target address, unless the WasmJSFunction wraps a
  // WasmExportedFunction.
  Address call_target = kNullAddress;
  if (WasmExportedFunction::IsWasmExportedFunction(*callable)) {
    call_target = WasmExportedFunction::cast(*callable).GetWasmCallTarget();
  }

  Factory* factory = isolate->factory();
  // TODO(7748): Support proper typing for external functions. That requires
  // global (cross-module) canonicalization of signatures/RTTs.
  Handle<Map> rtt = factory->wasm_internal_function_map();
  Handle<WasmJSFunctionData> function_data = factory->NewWasmJSFunctionData(
      call_target, callable, return_count, parameter_count, serialized_sig,
      wrapper_code, rtt, suspender);

  if (wasm::WasmFeatures::FromIsolate(isolate).has_typed_funcref()) {
    using CK = compiler::WasmImportCallKind;
    int expected_arity = parameter_count;
    CK kind = compiler::kDefaultImportCallKind;
    if (callable->IsJSFunction()) {
      SharedFunctionInfo shared = Handle<JSFunction>::cast(callable)->shared();
      expected_arity =
          shared.internal_formal_parameter_count_without_receiver();
      if (expected_arity != parameter_count) {
        kind = CK::kJSFunctionArityMismatch;
      }
    }
    // TODO(wasm): Think about caching and sharing the wasm-to-JS wrappers per
    // signature instead of compiling a new one for every instantiation.
    wasm::Suspend suspend =
        suspender.is_null() ? wasm::kNoSuspend : wasm::kSuspend;
    DCHECK_IMPLIES(!suspender.is_null(), !suspender->IsUndefined());
    Handle<CodeT> wasm_to_js_wrapper_code =
        ToCodeT(compiler::CompileWasmToJSWrapper(isolate, sig, kind,
                                                 expected_arity, suspend)
                    .ToHandleChecked(),
                isolate);
    function_data->internal().set_code(*wasm_to_js_wrapper_code);
  }

  Handle<String> name = factory->Function_string();
  if (callable->IsJSFunction()) {
    name = JSFunction::GetDebugName(Handle<JSFunction>::cast(callable));
    name = String::Flatten(isolate, name);
  }
  Handle<NativeContext> context(isolate->native_context());
  Handle<SharedFunctionInfo> shared =
      factory->NewSharedFunctionInfoForWasmJSFunction(name, function_data);
  Handle<JSFunction> js_function =
      Factory::JSFunctionBuilder{isolate, shared, context}
          .set_map(isolate->wasm_exported_function_map())
          .Build();
  js_function->shared().set_internal_formal_parameter_count(
      JSParameterCount(parameter_count));
  function_data->internal().set_external(*js_function);
  return Handle<WasmJSFunction>::cast(js_function);
}

JSReceiver WasmJSFunction::GetCallable() const {
  return JSReceiver::cast(WasmApiFunctionRef::cast(
                              shared().wasm_js_function_data().internal().ref())
                              .callable());
}

HeapObject WasmJSFunction::GetSuspender() const {
  return WasmApiFunctionRef::cast(
             shared().wasm_js_function_data().internal().ref())
      .suspender();
}

const wasm::FunctionSig* WasmJSFunction::GetSignature(Zone* zone) {
  WasmJSFunctionData function_data = shared().wasm_js_function_data();
  int sig_size = function_data.serialized_signature().length();
  wasm::ValueType* types = zone->NewArray<wasm::ValueType>(sig_size);
  if (sig_size > 0) {
    function_data.serialized_signature().copy_out(0, types, sig_size);
  }
  int return_count = function_data.serialized_return_count();
  int parameter_count = function_data.serialized_parameter_count();
  return zone->New<wasm::FunctionSig>(return_count, parameter_count, types);
}

bool WasmJSFunction::MatchesSignatureForSuspend(const wasm::FunctionSig* sig) {
  DCHECK_LE(sig->all().size(), kMaxInt);
  int sig_size = static_cast<int>(sig->all().size());
  int parameter_count = static_cast<int>(sig->parameter_count());
  int return_count = static_cast<int>(sig->return_count());
  DisallowHeapAllocation no_alloc;
  WasmJSFunctionData function_data = shared().wasm_js_function_data();
  if (parameter_count != function_data.serialized_parameter_count()) {
    return false;
  }
  if (sig_size == 0) return true;  // Prevent undefined behavior.
  // This function is only called for functions wrapped by a
  // WebAssembly.Suspender object, so the return type has to be externref.
  CHECK_EQ(function_data.serialized_return_count(), 1);
  CHECK_EQ(function_data.serialized_signature().get(0), wasm::kWasmAnyRef);
  const wasm::ValueType* expected = sig->all().begin();
  return function_data.serialized_signature().matches(
      1, expected + return_count, parameter_count);
}

// TODO(9495): Update this if function type variance is introduced.
bool WasmJSFunction::MatchesSignature(const wasm::FunctionSig* sig) {
  DCHECK_LE(sig->all().size(), kMaxInt);
  int sig_size = static_cast<int>(sig->all().size());
  int return_count = static_cast<int>(sig->return_count());
  int parameter_count = static_cast<int>(sig->parameter_count());
  DisallowHeapAllocation no_alloc;
  WasmJSFunctionData function_data = shared().wasm_js_function_data();
  if (return_count != function_data.serialized_return_count() ||
      parameter_count != function_data.serialized_parameter_count()) {
    return false;
  }
  if (sig_size == 0) return true;  // Prevent undefined behavior.
  const wasm::ValueType* expected = sig->all().begin();
  return function_data.serialized_signature().matches(expected, sig_size);
}

PodArray<wasm::ValueType> WasmCapiFunction::GetSerializedSignature() const {
  return shared().wasm_capi_function_data().serialized_signature();
}

bool WasmExternalFunction::IsWasmExternalFunction(Object object) {
  return WasmExportedFunction::IsWasmExportedFunction(object) ||
         WasmJSFunction::IsWasmJSFunction(object);
}

// static
MaybeHandle<WasmInternalFunction> WasmInternalFunction::FromExternal(
    Handle<Object> external, Isolate* isolate) {
  if (WasmExportedFunction::IsWasmExportedFunction(*external) ||
      WasmJSFunction::IsWasmJSFunction(*external) ||
      WasmCapiFunction::IsWasmCapiFunction(*external)) {
    WasmFunctionData data = WasmFunctionData::cast(
        Handle<JSFunction>::cast(external)->shared().function_data(
            kAcquireLoad));
    return handle(data.internal(), isolate);
  }
  return MaybeHandle<WasmInternalFunction>();
}

Handle<WasmExceptionTag> WasmExceptionTag::New(Isolate* isolate, int index) {
  Handle<WasmExceptionTag> result =
      Handle<WasmExceptionTag>::cast(isolate->factory()->NewStruct(
          WASM_EXCEPTION_TAG_TYPE, AllocationType::kOld));
  result->set_index(index);
  return result;
}

Handle<AsmWasmData> AsmWasmData::New(
    Isolate* isolate, std::shared_ptr<wasm::NativeModule> native_module,
    Handle<FixedArray> export_wrappers, Handle<HeapNumber> uses_bitset) {
  const WasmModule* module = native_module->module();
  const bool kUsesLiftoff = false;
  size_t memory_estimate =
      wasm::WasmCodeManager::EstimateNativeModuleCodeSize(
          module, kUsesLiftoff, wasm::kNoDynamicTiering) +
      wasm::WasmCodeManager::EstimateNativeModuleMetaDataSize(module);
  Handle<Managed<wasm::NativeModule>> managed_native_module =
      Managed<wasm::NativeModule>::FromSharedPtr(isolate, memory_estimate,
                                                 std::move(native_module));
  Handle<AsmWasmData> result = Handle<AsmWasmData>::cast(
      isolate->factory()->NewStruct(ASM_WASM_DATA_TYPE, AllocationType::kOld));
  result->set_managed_native_module(*managed_native_module);
  result->set_export_wrappers(*export_wrappers);
  result->set_uses_bitset(*uses_bitset);
  return result;
}

namespace wasm {

bool TypecheckJSObject(Isolate* isolate, const WasmModule* module,
                       Handle<Object> value, ValueType expected,
                       const char** error_message) {
  DCHECK(expected.is_reference());
  switch (expected.kind()) {
    case kOptRef:
      if (value->IsNull(isolate)) return true;
      V8_FALLTHROUGH;
    case kRef: {
      HeapType::Representation repr = expected.heap_representation();
      switch (repr) {
        case HeapType::kFunc: {
          if (!(WasmExternalFunction::IsWasmExternalFunction(*value) ||
                WasmCapiFunction::IsWasmCapiFunction(*value))) {
            *error_message =
                "function-typed object must be null (if nullable) or a Wasm "
                "function object";
            return false;
          }
          return true;
        }
        case HeapType::kAny:
          return true;
        case HeapType::kData:
        case HeapType::kArray:
        case HeapType::kEq:
        case HeapType::kI31: {
          // TODO(7748): Change this when we have a decision on the JS API for
          // structs/arrays.
          if (!FLAG_wasm_gc_js_interop) {
            Handle<Name> key = isolate->factory()->wasm_wrapped_object_symbol();
            LookupIterator it(isolate, value, key,
                              LookupIterator::OWN_SKIP_INTERCEPTOR);
            if (it.state() != LookupIterator::DATA) {
              *error_message =
                  "eqref/dataref/i31ref object must be null (if nullable) or "
                  "wrapped with the wasm object wrapper";
              return false;
            }
            value = it.GetDataValue();
          }

          if (repr == HeapType::kI31) {
            if (!value->IsSmi()) {
              *error_message = "i31ref-typed object cannot be a heap object";
              return false;
            }
            return true;
          }

          if (!((repr == HeapType::kEq && value->IsSmi()) ||
                (repr != HeapType::kArray && value->IsWasmStruct()) ||
                value->IsWasmArray())) {
            *error_message = "object incompatible with wasm type";
            return false;
          }
          return true;
        }
        default:
          if (module == nullptr) {
            *error_message =
                "an object defined in JavaScript cannot be compatible with a "
                "type defined in a Webassembly module";
            return false;
          }
          DCHECK(module->has_type(expected.ref_index()));
          if (module->has_signature(expected.ref_index())) {
            if (WasmExportedFunction::IsWasmExportedFunction(*value)) {
              WasmExportedFunction function =
                  WasmExportedFunction::cast(*value);
              const WasmModule* exporting_module = function.instance().module();
              ValueType real_type = ValueType::Ref(
                  exporting_module->functions[function.function_index()]
                      .sig_index,
                  kNonNullable);
              if (!IsSubtypeOf(real_type, expected, exporting_module, module)) {
                *error_message =
                    "assigned exported function has to be a subtype of the "
                    "expected type";
                return false;
              }
              return true;
            }

            if (WasmJSFunction::IsWasmJSFunction(*value)) {
              // Since a WasmJSFunction cannot refer to indexed types (definable
              // only in a module), we do not need full function subtyping.
              // TODO(manoskouk): Change this if wasm types can be exported.
              if (!WasmJSFunction::cast(*value).MatchesSignature(
                      module->signature(expected.ref_index()))) {
                *error_message =
                    "assigned WasmJSFunction has to be a subtype of the "
                    "expected type";
                return false;
              }
              return true;
            }

            if (WasmCapiFunction::IsWasmCapiFunction(*value)) {
              // Since a WasmCapiFunction cannot refer to indexed types
              // (definable only in a module), we do not need full function
              // subtyping.
              // TODO(manoskouk): Change this if wasm types can be exported.
              if (!WasmCapiFunction::cast(*value).MatchesSignature(
                      module->signature(expected.ref_index()))) {
                *error_message =
                    "assigned WasmCapiFunction has to be a subtype of the "
                    "expected type";
                return false;
              }
              return true;
            }

            *error_message =
                "function-typed object must be null (if nullable) or a Wasm "
                "function object";

            return false;
          }
          // TODO(7748): Implement when the JS API for structs/arrays is decided
          // on.
          *error_message =
              "passing struct/array-typed objects between Webassembly and "
              "Javascript is not supported yet.";
          return false;
      }
    }
    case kRtt:
      // TODO(7748): Implement when the JS API for rtts is decided on.
      *error_message =
          "passing rtts between Webassembly and Javascript is not supported "
          "yet.";
      return false;
    case kI8:
    case kI16:
    case kI32:
    case kI64:
    case kF32:
    case kF64:
    case kS128:
    case kVoid:
    case kBottom:
      UNREACHABLE();
  }
}

}  // namespace wasm

}  // namespace internal
}  // namespace v8

#undef TRACE_IFT

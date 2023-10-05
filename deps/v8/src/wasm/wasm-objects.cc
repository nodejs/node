// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-objects.h"

#include "src/base/iterator.h"
#include "src/base/vector.h"
#include "src/compiler/wasm-compiler.h"
#include "src/debug/debug.h"
#include "src/logging/counters.h"
#include "src/objects/managed-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/utils/utils.h"
#include "src/wasm/code-space-access.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/module-instantiate.h"
#include "src/wasm/serialized-signature-inl.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-subtyping.h"
#include "src/wasm/wasm-value.h"

// Needs to be last so macros do not get undefined.
#include "src/objects/object-macros.h"

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
  Handle<Managed<wasm::NativeModule>> managed_native_module;
  if (script->type() == Script::Type::kWasm) {
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
  CHECK(type.is_object_reference());

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
  if (!Object::ToUint32(table->maximum_length(), &max_size)) {
    max_size = v8_flags.wasm_max_table_size;
  }
  max_size = std::min(max_size, v8_flags.wasm_max_table_size.value());
  DCHECK_LE(old_size, max_size);
  if (max_size - old_size < count) return -1;

  uint32_t new_size = old_size + count;
  // Even with 2x over-allocation, there should not be an integer overflow.
  static_assert(wasm::kV8MaxWasmTableSize <= kMaxInt / 2);
  DCHECK_GE(kMaxInt, new_size);
  int old_capacity = table->entries()->length();
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

bool WasmTableObject::is_in_bounds(uint32_t entry_index) {
  return entry_index < static_cast<uint32_t>(current_length());
}

MaybeHandle<Object> WasmTableObject::JSToWasmElement(
    Isolate* isolate, Handle<WasmTableObject> table, Handle<Object> entry,
    const char** error_message) {
  // Any `entry` has to be in its JS representation.
  DCHECK(!IsWasmInternalFunction(*entry));
  const WasmModule* module =
      !IsUndefined(table->instance())
          ? WasmInstanceObject::cast(table->instance())->module()
          : nullptr;
  return wasm::JSToWasmObject(isolate, module, entry, table->type(),
                              error_message);
}

void WasmTableObject::SetFunctionTableEntry(Isolate* isolate,
                                            Handle<WasmTableObject> table,
                                            Handle<FixedArray> entries,
                                            int entry_index,
                                            Handle<Object> entry) {
  if (IsWasmNull(*entry, isolate)) {
    ClearDispatchTables(isolate, table, entry_index);  // Degenerate case.
    entries->set(entry_index, ReadOnlyRoots(isolate).wasm_null());
    return;
  }
  Handle<Object> external = WasmInternalFunction::GetOrCreateExternal(
      Handle<WasmInternalFunction>::cast(entry));

  if (WasmExportedFunction::IsWasmExportedFunction(*external)) {
    auto exported_function = Handle<WasmExportedFunction>::cast(external);
    Handle<WasmInstanceObject> target_instance(exported_function->instance(),
                                               isolate);
    int func_index = exported_function->function_index();
    auto* wasm_function = &target_instance->module()->functions[func_index];
    UpdateDispatchTables(isolate, table, entry_index, wasm_function,
                         target_instance);
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

// Note: This needs to be handlified because it transitively calls
// {ImportWasmJSFunctionIntoTable} which calls {NewWasmApiFunctionRef}.
void WasmTableObject::Set(Isolate* isolate, Handle<WasmTableObject> table,
                          uint32_t index, Handle<Object> entry) {
  // Callers need to perform bounds checks, type check, and error handling.
  DCHECK(table->is_in_bounds(index));

  Handle<FixedArray> entries(table->entries(), isolate);
  // The FixedArray is addressed with int's.
  int entry_index = static_cast<int>(index);

  switch (table->type().heap_representation()) {
    case wasm::HeapType::kExtern:
    case wasm::HeapType::kString:
    case wasm::HeapType::kStringViewWtf8:
    case wasm::HeapType::kStringViewWtf16:
    case wasm::HeapType::kStringViewIter:
    case wasm::HeapType::kEq:
    case wasm::HeapType::kStruct:
    case wasm::HeapType::kArray:
    case wasm::HeapType::kAny:
    case wasm::HeapType::kI31:
    case wasm::HeapType::kNone:
    case wasm::HeapType::kNoFunc:
    case wasm::HeapType::kNoExtern:
      entries->set(entry_index, *entry);
      return;
    case wasm::HeapType::kFunc:
      SetFunctionTableEntry(isolate, table, entries, entry_index, entry);
      return;
    case wasm::HeapType::kBottom:
      UNREACHABLE();
    default:
      DCHECK(!IsUndefined(table->instance()));
      if (WasmInstanceObject::cast(table->instance())
              ->module()
              ->has_signature(table->type().ref_index())) {
        SetFunctionTableEntry(isolate, table, entries, entry_index, entry);
        return;
      }
      entries->set(entry_index, *entry);
      return;
  }
}

Handle<Object> WasmTableObject::Get(Isolate* isolate,
                                    Handle<WasmTableObject> table,
                                    uint32_t index) {
  Handle<FixedArray> entries(table->entries(), isolate);
  // Callers need to perform bounds checks and error handling.
  DCHECK(table->is_in_bounds(index));

  // The FixedArray is addressed with int's.
  int entry_index = static_cast<int>(index);

  Handle<Object> entry(entries->get(entry_index), isolate);

  if (IsWasmNull(*entry, isolate)) {
    return entry;
  }

  switch (table->type().heap_representation()) {
    case wasm::HeapType::kStringViewWtf8:
    case wasm::HeapType::kStringViewWtf16:
    case wasm::HeapType::kStringViewIter:
    case wasm::HeapType::kExtern:
    case wasm::HeapType::kString:
    case wasm::HeapType::kEq:
    case wasm::HeapType::kI31:
    case wasm::HeapType::kStruct:
    case wasm::HeapType::kArray:
    case wasm::HeapType::kAny:
    case wasm::HeapType::kNone:
    case wasm::HeapType::kNoFunc:
    case wasm::HeapType::kNoExtern:
      return entry;
    case wasm::HeapType::kFunc:
      if (IsWasmInternalFunction(*entry)) return entry;
      break;
    case wasm::HeapType::kBottom:
      UNREACHABLE();
    default:
      DCHECK(!IsUndefined(table->instance()));
      const WasmModule* module =
          WasmInstanceObject::cast(table->instance())->module();
      if (module->has_array(table->type().ref_index()) ||
          module->has_struct(table->type().ref_index())) {
        return entry;
      }
      DCHECK(module->has_signature(table->type().ref_index()));
      if (IsWasmInternalFunction(*entry)) return entry;
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

// static
void WasmTableObject::UpdateDispatchTables(
    Isolate* isolate, Handle<WasmTableObject> table, int entry_index,
    const wasm::WasmFunction* func,
    Handle<WasmInstanceObject> target_instance) {
  // We simply need to update the IFTs for each instance that imports
  // this table.
  Handle<FixedArray> dispatch_tables =
      handle(table->dispatch_tables(), isolate);
  DCHECK_EQ(0, dispatch_tables->length() % kDispatchTableNumElements);

  Handle<Object> call_ref =
      func->imported
          // The function in the target instance was imported. Use its imports
          // table, which contains a tuple needed by the import wrapper.
          ? handle(target_instance->imported_function_refs()->get(
                       func->func_index),
                   isolate)
          // For wasm functions, just pass the target instance.
          : target_instance;
  Address call_target = target_instance->GetCallTarget(func->func_index);

  int original_sig_id = func->sig_index;

  for (int i = 0, len = dispatch_tables->length(); i < len;
       i += kDispatchTableNumElements) {
    int table_index =
        Smi::cast(dispatch_tables->get(i + kDispatchTableIndexOffset)).value();
    Handle<WasmInstanceObject> instance =
        handle(WasmInstanceObject::cast(
                   dispatch_tables->get(i + kDispatchTableInstanceOffset)),
               isolate);
    int sig_id = target_instance->module()
                     ->isorecursive_canonical_type_ids[original_sig_id];
    Handle<WasmIndirectFunctionTable> ift =
        handle(WasmIndirectFunctionTable::cast(
                   instance->indirect_function_tables()->get(table_index)),
               isolate);
    if (v8_flags.wasm_to_js_generic_wrapper &&
        IsWasmApiFunctionRef(*call_ref)) {
      Handle<WasmApiFunctionRef> orig_ref =
          Handle<WasmApiFunctionRef>::cast(call_ref);
      Handle<WasmApiFunctionRef> new_ref =
          isolate->factory()->NewWasmApiFunctionRef(orig_ref);
      if (new_ref->instance() == *instance) {
        WasmApiFunctionRef::SetIndexInTableAsCallOrigin(new_ref, entry_index);
      } else {
        WasmApiFunctionRef::SetCrossInstanceTableIndexAsCallOrigin(
            isolate, new_ref, instance, entry_index);
      }
      call_ref = new_ref;
    }
    ift->Set(entry_index, sig_id, call_target, *call_ref);
  }
}

// static
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

// static
void WasmTableObject::UpdateDispatchTables(
    Isolate* isolate, Handle<WasmTableObject> table, int entry_index,
    Handle<WasmCapiFunction> capi_function) {
  // We simply need to update the IFTs for each instance that imports
  // this table.
  Handle<FixedArray> dispatch_tables(table->dispatch_tables(), isolate);
  DCHECK_EQ(0, dispatch_tables->length() % kDispatchTableNumElements);

  // Reconstruct signature.
  std::unique_ptr<wasm::ValueType[]> reps;
  wasm::FunctionSig sig = wasm::SerializedSignatureHelper::DeserializeSignature(
      capi_function->GetSerializedSignature(), &reps);

  for (int i = 0; i < dispatch_tables->length();
       i += kDispatchTableNumElements) {
    int table_index =
        Smi::cast(dispatch_tables->get(i + kDispatchTableIndexOffset)).value();
    Handle<WasmInstanceObject> instance(
        WasmInstanceObject::cast(
            dispatch_tables->get(i + kDispatchTableInstanceOffset)),
        isolate);
    wasm::NativeModule* native_module =
        instance->module_object()->native_module();
    wasm::WasmImportWrapperCache* cache = native_module->import_wrapper_cache();
    auto kind = wasm::ImportCallKind::kWasmToCapi;
    uint32_t canonical_type_index =
        wasm::GetTypeCanonicalizer()->AddRecursiveGroup(&sig);
    int param_count = static_cast<int>(sig.parameter_count());
    wasm::WasmCode* wasm_code = cache->MaybeGet(kind, canonical_type_index,
                                                param_count, wasm::kNoSuspend);
    if (wasm_code == nullptr) {
      wasm::WasmCodeRefScope code_ref_scope;
      wasm::WasmImportWrapperCache::ModificationScope cache_scope(cache);
      wasm_code = compiler::CompileWasmCapiCallWrapper(native_module, &sig);
      wasm::WasmImportWrapperCache::CacheKey key(kind, canonical_type_index,
                                                 param_count, wasm::kNoSuspend);
      cache_scope[key] = wasm_code;
      wasm_code->IncRef();
      isolate->counters()->wasm_generated_code_size()->Increment(
          wasm_code->instructions().length());
      isolate->counters()->wasm_reloc_size()->Increment(
          wasm_code->reloc_info().length());
    }
    instance->GetIndirectFunctionTable(isolate, table_index)
        ->Set(entry_index, canonical_type_index, wasm_code->instruction_start(),
              WasmCapiFunctionData::cast(
                  capi_function->shared()->function_data(kAcquireLoad))
                  ->internal()
                  ->ref());
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
  table->entries()->set(entry_index, *tuple);
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
  Handle<Object> element(table->entries()->get(entry_index), isolate);

  *is_null = IsWasmNull(*element, isolate);
  if (*is_null) return;

  if (IsWasmInternalFunction(*element)) {
    element = WasmInternalFunction::GetOrCreateExternal(
        Handle<WasmInternalFunction>::cast(element));
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
  if (IsTuple2(*element)) {
    auto tuple = Handle<Tuple2>::cast(element);
    *instance = handle(WasmInstanceObject::cast(tuple->value1()), isolate);
    *function_index = Smi::cast(tuple->value2()).value();
    *maybe_js_function = MaybeHandle<WasmJSFunction>();
    return;
  }
  *is_valid = false;
}

Handle<WasmIndirectFunctionTable> WasmIndirectFunctionTable::New(
    Isolate* isolate, uint32_t size) {
  auto refs = isolate->factory()->NewFixedArray(static_cast<int>(size));
  auto sig_ids = FixedUInt32Array::New(isolate, size);
  auto targets = ExternalPointerArray::New(isolate, size);
  auto table = Handle<WasmIndirectFunctionTable>::cast(
      isolate->factory()->NewStruct(WASM_INDIRECT_FUNCTION_TABLE_TYPE));

  // Disallow GC until all fields have acceptable types.
  DisallowGarbageCollection no_gc;

  table->set_size(size);
  table->set_refs(*refs);
  table->set_sig_ids(*sig_ids);
  table->set_targets(*targets);
  for (uint32_t i = 0; i < size; ++i) {
    table->Clear(i);
  }

  return table;
}

void WasmIndirectFunctionTable::Set(uint32_t index, int sig_id,
                                    Address call_target, Tagged<Object> ref) {
  Isolate* isolate = GetIsolateFromWritableObject(*this);
  sig_ids()->set(index, sig_id);
  targets()->set<kWasmIndirectFunctionTargetTag>(index, isolate, call_target);
  refs()->set(index, ref);
}

void WasmIndirectFunctionTable::Clear(uint32_t index) {
  Isolate* isolate = GetIsolateFromWritableObject(*this);
  sig_ids()->set(index, -1);
  targets()->clear(index);
  refs()->set(index, ReadOnlyRoots(isolate).undefined_value());
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
  Handle<FixedUInt32Array> old_sig_ids(table->sig_ids(), isolate);
  Handle<ExternalPointerArray> old_targets(table->targets(), isolate);

  // Since we might have overallocated, {old_capacity} might be different than
  // {old_size}.
  uint32_t old_capacity = old_refs->length();
  // If we have enough capacity, there is no need to reallocate.
  if (new_size <= old_capacity) return;
  uint32_t new_capacity = std::max(2 * old_capacity, new_size);

  Handle<FixedUInt32Array> new_sig_ids =
      FixedUInt32Array::New(isolate, new_capacity);
  new_sig_ids->copy_in(0, old_sig_ids->GetDataStartAddress(),
                       old_capacity * kUInt32Size);
  table->set_sig_ids(*new_sig_ids);

  Handle<ExternalPointerArray> new_targets =
      isolate->factory()
          ->CopyExternalPointerArrayAndGrow<kWasmIndirectFunctionTargetTag>(
              old_targets, static_cast<int>(new_capacity - old_capacity));
  table->set_targets(*new_targets);

  Handle<FixedArray> new_refs = isolate->factory()->CopyFixedArrayAndGrow(
      old_refs, static_cast<int>(new_capacity - old_capacity));
  table->set_refs(*new_refs);

  for (uint32_t i = old_capacity; i < new_capacity; ++i) {
    table->Clear(i);
  }
}

namespace {

void SetInstanceMemory(Tagged<WasmInstanceObject> instance,
                       Tagged<JSArrayBuffer> buffer, int memory_index) {
  DisallowHeapAllocation no_gc;
  const WasmModule* module = instance->module();
  const wasm::WasmMemory& memory = module->memories[memory_index];

  bool is_wasm_module = module->origin == wasm::kWasmOrigin;
  bool use_trap_handler = memory.bounds_checks == wasm::kTrapHandler;
  // Asm.js does not use trap handling.
  CHECK_IMPLIES(use_trap_handler, is_wasm_module);
  // ArrayBuffers allocated for Wasm do always have a BackingStore.
  std::shared_ptr<BackingStore> backing_store = buffer->GetBackingStore();
  CHECK_IMPLIES(is_wasm_module, backing_store);
  CHECK_IMPLIES(is_wasm_module, backing_store->is_wasm_memory());
  // Wasm modules compiled to use the trap handler don't have bounds checks,
  // so they must have a memory that has guard regions.
  CHECK_IMPLIES(use_trap_handler, backing_store->has_guard_regions());

  instance->SetRawMemory(memory_index,
                         reinterpret_cast<uint8_t*>(buffer->backing_store()),
                         buffer->byte_length());
#if DEBUG
  if (!v8_flags.mock_arraybuffer_allocator) {
    // To flush out bugs earlier, in DEBUG mode, check that all pages of the
    // memory are accessible by reading and writing one byte on each page.
    // Don't do this if the mock ArrayBuffer allocator is enabled.
    uint8_t* mem_start = instance->memory0_start();
    size_t mem_size = instance->memory0_size();
    for (size_t offset = 0; offset < mem_size; offset += wasm::kWasmPageSize) {
      uint8_t val = mem_start[offset];
      USE(val);
      mem_start[offset] = val;
    }
  }
#endif
}

}  // namespace

Handle<WasmMemoryObject> WasmMemoryObject::New(Isolate* isolate,
                                               Handle<JSArrayBuffer> buffer,
                                               int maximum,
                                               WasmMemoryFlag memory_type) {
  Handle<JSFunction> memory_ctor(
      isolate->native_context()->wasm_memory_constructor(), isolate);

  auto memory_object = Handle<WasmMemoryObject>::cast(
      isolate->factory()->NewJSObject(memory_ctor, AllocationType::kOld));
  memory_object->set_array_buffer(*buffer);
  memory_object->set_maximum_pages(maximum);
  memory_object->set_is_memory64(memory_type == WasmMemoryFlag::kWasmMemory64);

  std::shared_ptr<BackingStore> backing_store = buffer->GetBackingStore();
  if (buffer->is_shared()) {
    // Only Wasm memory can be shared (in contrast to asm.js memory).
    CHECK(backing_store && backing_store->is_wasm_memory());
    backing_store->AttachSharedWasmMemoryObject(isolate, memory_object);
  } else if (backing_store) {
    CHECK(!backing_store->is_shared());
  }

  // For debugging purposes we memorize a link from the JSArrayBuffer
  // to it's owning WasmMemoryObject instance.
  Handle<Symbol> symbol = isolate->factory()->array_buffer_wasm_memory_symbol();
  JSObject::SetProperty(isolate, buffer, symbol, memory_object).Check();

  return memory_object;
}

MaybeHandle<WasmMemoryObject> WasmMemoryObject::New(
    Isolate* isolate, int initial, int maximum, SharedFlag shared,
    WasmMemoryFlag memory_type) {
  bool has_maximum = maximum != kNoMaximum;

  int engine_maximum = memory_type == WasmMemoryFlag::kWasmMemory64
                           ? static_cast<int>(wasm::max_mem64_pages())
                           : static_cast<int>(wasm::max_mem32_pages());

  if (initial > engine_maximum) return {};

#ifdef V8_TARGET_ARCH_32_BIT
  // On 32-bit platforms we need an heuristic here to balance overall memory
  // and address space consumption.
  constexpr int kGBPages = 1024 * 1024 * 1024 / wasm::kWasmPageSize;
  // We allocate the smallest of the following sizes, but at least the initial
  // size:
  // 1) the module-defined maximum;
  // 2) 1GB;
  // 3) the engine maximum;
  int allocation_maximum = std::min(kGBPages, engine_maximum);
  int heuristic_maximum;
  if (initial > kGBPages) {
    // We always allocate at least the initial size.
    heuristic_maximum = initial;
  } else if (has_maximum) {
    // We try to reserve the maximum, but at most the allocation_maximum to
    // avoid OOMs.
    heuristic_maximum = std::min(maximum, allocation_maximum);
  } else if (shared == SharedFlag::kShared) {
    // If shared memory has no maximum, we use the allocation_maximum as an
    // implicit maximum.
    heuristic_maximum = allocation_maximum;
  } else {
    // If non-shared memory has no maximum, we only allocate the initial size
    // and then grow with realloc.
    heuristic_maximum = initial;
  }
#else
  int heuristic_maximum =
      has_maximum ? std::min(engine_maximum, maximum) : engine_maximum;
#endif

  std::unique_ptr<BackingStore> backing_store =
      BackingStore::AllocateWasmMemory(isolate, initial, heuristic_maximum,
                                       memory_type, shared);

  if (!backing_store) return {};

  Handle<JSArrayBuffer> buffer =
      shared == SharedFlag::kShared
          ? isolate->factory()->NewJSSharedArrayBuffer(std::move(backing_store))
          : isolate->factory()->NewJSArrayBuffer(std::move(backing_store));

  return New(isolate, buffer, maximum, memory_type);
}

void WasmMemoryObject::UseInInstance(Isolate* isolate,
                                     Handle<WasmMemoryObject> memory,
                                     Handle<WasmInstanceObject> instance,
                                     int memory_index_in_instance) {
  SetInstanceMemory(*instance, memory->array_buffer(),
                    memory_index_in_instance);
  Handle<WeakArrayList> old_instances =
      memory->has_instances()
          ? Handle<WeakArrayList>(memory->instances(), isolate)
          : isolate->factory()->empty_weak_array_list();
  Handle<WeakArrayList> new_instances = WeakArrayList::Append(
      isolate, old_instances, MaybeObjectHandle::Weak(instance));
  memory->set_instances(*new_instances);
}

void WasmMemoryObject::SetNewBuffer(Tagged<JSArrayBuffer> new_buffer) {
  DisallowGarbageCollection no_gc;
  set_array_buffer(new_buffer);
  if (has_instances()) {
    Tagged<WeakArrayList> instances = this->instances();
    for (int i = 0, len = instances->length(); i < len; ++i) {
      MaybeObject elem = instances->Get(i);
      if (elem->IsCleared()) continue;
      Tagged<WasmInstanceObject> instance =
          WasmInstanceObject::cast(elem.GetHeapObjectAssumeWeak());
      // TODO(clemens): Avoid the iteration by also remembering the memory index
      // if we ever see larger numbers of memories.
      Tagged<FixedArray> memory_objects = instance->memory_objects();
      int num_memories = memory_objects->length();
      for (int mem_idx = 0; mem_idx < num_memories; ++mem_idx) {
        if (memory_objects->get(mem_idx) == *this) {
          SetInstanceMemory(instance, new_buffer, mem_idx);
        }
      }
    }
  }
}

// static
int32_t WasmMemoryObject::Grow(Isolate* isolate,
                               Handle<WasmMemoryObject> memory_object,
                               uint32_t pages) {
  TRACE_EVENT0("v8.wasm", "wasm.GrowMemory");
  Handle<JSArrayBuffer> old_buffer(memory_object->array_buffer(), isolate);

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
  size_t max_pages = memory_object->is_memory64() ? wasm::max_mem64_pages()
                                                  : wasm::max_mem32_pages();
  if (memory_object->has_maximum_pages()) {
    max_pages = std::min(max_pages,
                         static_cast<size_t>(memory_object->maximum_pages()));
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
      if (v8_flags.correctness_fuzzer_suppressions) {
        FATAL("could not grow wasm memory");
      }
      return -1;
    }

    backing_store->BroadcastSharedWasmMemoryGrow(isolate);
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
    CHECK_LE(new_byte_length, memory_object->array_buffer()->byte_length());
    // As {old_pages} was read racefully, we return here the synchronized
    // value provided by {GrowWasmMemoryInPlace}, to provide the atomic
    // read-modify-write behavior required by the spec.
    return static_cast<int32_t>(result_inplace.value());  // success
  }

  // Check if the non-shared memory could grow in-place.
  if (result_inplace.has_value()) {
    // Detach old and create a new one with the grown backing store.
    JSArrayBuffer::Detach(old_buffer, true).Check();
    Handle<JSArrayBuffer> new_buffer =
        isolate->factory()->NewJSArrayBuffer(std::move(backing_store));
    memory_object->SetNewBuffer(*new_buffer);
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
  // First apply {min_growth}, then {max_pages}. The order is important, because
  // {min_growth} can be bigger than {max_pages}, and in that case we want to
  // cap to {max_pages}.
  size_t new_capacity = std::min(max_pages, std::max(new_pages, min_growth));
  DCHECK_LT(old_pages, new_capacity);
  std::unique_ptr<BackingStore> new_backing_store =
      backing_store->CopyWasmMemory(isolate, new_pages, new_capacity,
                                    memory_object->is_memory64()
                                        ? WasmMemoryFlag::kWasmMemory64
                                        : WasmMemoryFlag::kWasmMemory32);
  if (!new_backing_store) {
    // Crash on out-of-memory if the correctness fuzzer is running.
    if (v8_flags.correctness_fuzzer_suppressions) {
      FATAL("could not grow wasm memory");
    }
    return -1;
  }

  // Detach old and create a new one with the new backing store.
  JSArrayBuffer::Detach(old_buffer, true).Check();
  Handle<JSArrayBuffer> new_buffer =
      isolate->factory()->NewJSArrayBuffer(std::move(new_backing_store));
  memory_object->SetNewBuffer(*new_buffer);
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
  Isolate* isolate = target_instance->native_context()->GetIsolate();
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

void ImportedFunctionEntry::SetWasmToJs(Isolate* isolate,
                                        Handle<JSReceiver> callable,
                                        wasm::Suspend suspend,
                                        const wasm::FunctionSig* sig) {
  DCHECK(UseGenericWasmToJSWrapper(wasm::kDefaultImportCallKind, sig, suspend));
  Address wrapper = isolate->builtins()
                        ->code(Builtin::kWasmToJsWrapperAsm)
                        ->instruction_start();
  TRACE_IFT("Import callable 0x%" PRIxPTR "[%d] = {callable=0x%" PRIxPTR
            ", target=0x%" PRIxPTR "}\n",
            instance_->ptr(), index_, callable->ptr(), wrapper);
  Handle<WasmApiFunctionRef> ref = isolate->factory()->NewWasmApiFunctionRef(
      callable, suspend, instance_,
      wasm::SerializedSignatureHelper::SerializeSignature(isolate, sig));
  WasmApiFunctionRef::SetImportIndexAsCallOrigin(ref, index_);
  instance_->imported_function_refs()->set(index_, *ref);
  instance_->imported_function_targets()->set(index_, wrapper);
}

void ImportedFunctionEntry::SetWasmToJs(
    Isolate* isolate, Handle<JSReceiver> callable,
    const wasm::WasmCode* wasm_to_js_wrapper, wasm::Suspend suspend,
    const wasm::FunctionSig* sig) {
  TRACE_IFT("Import callable 0x%" PRIxPTR "[%d] = {callable=0x%" PRIxPTR
            ", target=%p}\n",
            instance_->ptr(), index_, callable->ptr(),
            wasm_to_js_wrapper->instructions().begin());
  DCHECK(wasm_to_js_wrapper->kind() == wasm::WasmCode::kWasmToJsWrapper ||
         wasm_to_js_wrapper->kind() == wasm::WasmCode::kWasmToCapiWrapper);
  Handle<WasmApiFunctionRef> ref = isolate->factory()->NewWasmApiFunctionRef(
      callable, suspend, instance_,
      wasm::SerializedSignatureHelper::SerializeSignature(isolate, sig));
  // The wasm-to-js wrapper is already optimized, the call_origin should never
  // be accessed.
  ref->set_call_origin(Smi::FromInt(WasmApiFunctionRef::kInvalidCallOrigin));
  instance_->imported_function_refs()->set(index_, *ref);
  instance_->imported_function_targets()->set(
      index_, wasm_to_js_wrapper->instruction_start());
}

void ImportedFunctionEntry::SetWasmToWasm(Tagged<WasmInstanceObject> instance,
                                          Address call_target) {
  TRACE_IFT("Import Wasm 0x%" PRIxPTR "[%d] = {instance=0x%" PRIxPTR
            ", target=0x%" PRIxPTR "}\n",
            instance_->ptr(), index_, instance.ptr(), call_target);
  instance_->imported_function_refs()->set(index_, instance);
  instance_->imported_function_targets()->set(index_, call_target);
}

// Returns an empty Object() if no callable is available, a JSReceiver
// otherwise.
Tagged<Object> ImportedFunctionEntry::maybe_callable() {
  Tagged<Object> value = object_ref();
  if (!IsWasmApiFunctionRef(value)) return Object();
  return JSReceiver::cast(WasmApiFunctionRef::cast(value)->callable());
}

Tagged<JSReceiver> ImportedFunctionEntry::callable() {
  return JSReceiver::cast(WasmApiFunctionRef::cast(object_ref())->callable());
}

Tagged<Object> ImportedFunctionEntry::object_ref() {
  return instance_->imported_function_refs()->get(index_);
}

Address ImportedFunctionEntry::target() {
  return instance_->imported_function_targets()->get(index_);
}

void ImportedFunctionEntry::set_target(Address new_target) {
  instance_->imported_function_targets()->set(index_, new_target);
}

// static
constexpr std::array<uint16_t, 24> WasmInstanceObject::kTaggedFieldOffsets;
// static
constexpr std::array<const char*, 24> WasmInstanceObject::kTaggedFieldNames;

// static
bool WasmInstanceObject::EnsureIndirectFunctionTableWithMinimumSize(
    Handle<WasmInstanceObject> instance, int table_index,
    uint32_t minimum_size) {
  Isolate* isolate = instance->GetIsolate();
  DCHECK_LT(table_index, instance->indirect_function_tables()->length());
  Handle<WasmIndirectFunctionTable> table =
      instance->GetIndirectFunctionTable(isolate, table_index);
  WasmIndirectFunctionTable::Resize(isolate, table, minimum_size);
  if (table_index == 0) {
    instance->SetIndirectFunctionTableShortcuts(isolate);
  }
  return true;
}

void WasmInstanceObject::SetRawMemory(int memory_index, uint8_t* mem_start,
                                      size_t mem_size) {
  CHECK_LE(memory_index, module()->memories.size());

  CHECK_LE(mem_size, module()->memories[memory_index].is_memory64
                         ? wasm::max_mem64_bytes()
                         : wasm::max_mem32_bytes());
  // All memory bases and sizes are stored in a FixedAddressArray.
  Tagged<FixedAddressArray> bases_and_sizes = memory_bases_and_sizes();
  bases_and_sizes->set_sandboxed_pointer(memory_index * 2,
                                         reinterpret_cast<Address>(mem_start));
  bases_and_sizes->set(memory_index * 2 + 1, mem_size);
  // Memory 0 has fast-access fields.
  if (memory_index == 0) {
    set_memory0_start(mem_start);
    set_memory0_size(mem_size);
  }
}

const WasmModule* WasmInstanceObject::module() {
  return module_object()->module();
}

Handle<WasmInstanceObject> WasmInstanceObject::New(
    Isolate* isolate, Handle<WasmModuleObject> module_object) {
  // Do first allocate all objects that will be stored in instance fields,
  // because otherwise we would have to allocate when the instance is not fully
  // initialized yet, which can lead to heap verification errors.
  const WasmModule* module = module_object->module();

  int num_imported_functions = module->num_imported_functions;
  Handle<FixedAddressArray> imported_function_targets =
      FixedAddressArray::New(isolate, num_imported_functions);
  Handle<FixedArray> imported_function_refs =
      isolate->factory()->NewFixedArray(num_imported_functions);
  Handle<FixedArray> well_known_imports =
      isolate->factory()->NewFixedArray(num_imported_functions);

  Handle<FixedArray> functions = isolate->factory()->NewFixedArrayWithZeroes(
      static_cast<int>(module->functions.size()));

  int num_imported_mutable_globals = module->num_imported_mutable_globals;
  // The imported_mutable_globals is essentially a FixedAddressArray (storing
  // sandboxed pointers), but some entries (the indices for reference-type
  // globals) are accessed as 32-bit integers which is more convenient with a
  // raw ByteArray.
  Handle<FixedAddressArray> imported_mutable_globals =
      FixedAddressArray::New(isolate, num_imported_mutable_globals);

  int num_data_segments = module->num_declared_data_segments;
  Handle<FixedAddressArray> data_segment_starts =
      FixedAddressArray::New(isolate, num_data_segments);
  Handle<FixedUInt32Array> data_segment_sizes =
      FixedUInt32Array::New(isolate, num_data_segments);

  static_assert(wasm::kV8MaxWasmMemories < kMaxInt / 2);
  int num_memories = static_cast<int>(module->memories.size());
  Handle<FixedArray> memory_objects =
      isolate->factory()->NewFixedArray(num_memories);
  Handle<FixedAddressArray> memory_bases_and_sizes =
      FixedAddressArray::New(isolate, 2 * num_memories);

  // Now allocate the instance itself.
  Handle<JSFunction> instance_cons(
      isolate->native_context()->wasm_instance_constructor(), isolate);
  Handle<JSObject> instance_object =
      isolate->factory()->NewJSObject(instance_cons, AllocationType::kOld);

  // Initialize the instance. During this step, no more allocations should
  // happen because the instance is incomplete yet, so we should not trigger
  // heap verification at this point.
  {
    DisallowHeapAllocation no_gc;

    // Some constants:
    uint8_t* empty_backing_store_buffer =
        reinterpret_cast<uint8_t*>(EmptyBackingStoreBuffer());
    Tagged<FixedArray> empty_fixed_array =
        ReadOnlyRoots(isolate).empty_fixed_array();
    Tagged<ByteArray> empty_byte_array =
        ReadOnlyRoots(isolate).empty_byte_array();
    Tagged<ExternalPointerArray> empty_external_pointer_array =
        ReadOnlyRoots(isolate).empty_external_pointer_array();

    Tagged<WasmInstanceObject> instance =
        WasmInstanceObject::cast(*instance_object);
    instance->clear_padding();
    instance->set_imported_function_targets(*imported_function_targets);
    instance->set_imported_mutable_globals(*imported_mutable_globals);
    instance->set_data_segment_starts(*data_segment_starts);
    instance->set_data_segment_sizes(*data_segment_sizes);
    instance->set_element_segments(empty_fixed_array);
    instance->set_imported_function_refs(*imported_function_refs);
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
    instance->set_globals_start(empty_backing_store_buffer);
    instance->set_indirect_function_table_size(0);
    instance->set_indirect_function_table_refs(empty_fixed_array);
    instance->set_indirect_function_table_sig_ids(
        FixedUInt32Array::cast(empty_byte_array));
    instance->set_indirect_function_table_targets(
        ExternalPointerArray::cast(empty_external_pointer_array));
    instance->set_native_context(*isolate->native_context());
    instance->set_module_object(*module_object);
    instance->set_jump_table_start(
        module_object->native_module()->jump_table_start());
    instance->set_hook_on_function_call_address(
        isolate->debug()->hook_on_function_call_address());
    instance->set_managed_object_maps(*isolate->factory()->empty_fixed_array());
    instance->set_well_known_imports(*well_known_imports);
    instance->set_wasm_internal_functions(*functions);
    instance->set_feedback_vectors(*isolate->factory()->empty_fixed_array());
    instance->set_tiering_budget_array(
        module_object->native_module()->tiering_budget_array());
    instance->set_break_on_entry(module_object->script()->break_on_entry());
    instance->InitDataSegmentArrays(*module_object);
    instance->set_memory0_start(empty_backing_store_buffer);
    instance->set_memory0_size(0);
    instance->set_memory_objects(*memory_objects);
    instance->set_memory_bases_and_sizes(*memory_bases_and_sizes);

    for (int i = 0; i < num_memories; ++i) {
      memory_bases_and_sizes->set_sandboxed_pointer(
          2 * i, reinterpret_cast<Address>(empty_backing_store_buffer));
      memory_bases_and_sizes->set(2 * i + 1, 0);
    }
  }

  // Insert the new instance into the scripts weak list of instances. This list
  // is used for breakpoints affecting all instances belonging to the script.
  if (module_object->script()->type() == Script::Type::kWasm) {
    Handle<WeakArrayList> weak_instance_list(
        module_object->script()->wasm_weak_instance_list(), isolate);
    weak_instance_list = WeakArrayList::Append(
        isolate, weak_instance_list, MaybeObjectHandle::Weak(instance_object));
    module_object->script()->set_wasm_weak_instance_list(*weak_instance_list);
  }

  return Handle<WasmInstanceObject>::cast(instance_object);
}

void WasmInstanceObject::InitDataSegmentArrays(
    Tagged<WasmModuleObject> module_object) {
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
  for (uint32_t i = 0; i < num_data_segments; ++i) {
    const wasm::WasmDataSegment& segment = module->data_segments[i];
    // Initialize the pointer and size of passive segments.
    auto source_bytes = wire_bytes.SubVector(segment.source.offset(),
                                             segment.source.end_offset());
    data_segment_starts()->set(i,
                               reinterpret_cast<Address>(source_bytes.begin()));
    // Set the active segments to being already dropped, since memory.init on
    // a dropped passive segment and an active segment have the same
    // behavior.
    data_segment_sizes()->set(static_cast<int>(i),
                              segment.active ? 0 : source_bytes.length());
  }
}

Address WasmInstanceObject::GetCallTarget(uint32_t func_index) {
  wasm::NativeModule* native_module = module_object()->native_module();
  if (func_index < native_module->num_imported_functions()) {
    return imported_function_targets()->get(func_index);
  }
  return jump_table_start() +
         JumpTableOffset(native_module->module(), func_index);
}

Handle<WasmIndirectFunctionTable> WasmInstanceObject::GetIndirectFunctionTable(
    Isolate* isolate, uint32_t table_index) {
  DCHECK_LT(table_index, indirect_function_tables()->length());
  return handle(WasmIndirectFunctionTable::cast(
                    indirect_function_tables()->get(table_index)),
                isolate);
}

void WasmInstanceObject::SetIndirectFunctionTableShortcuts(Isolate* isolate) {
  if (indirect_function_tables()->length() > 0 &&
      IsWasmIndirectFunctionTable(indirect_function_tables()->get(0))) {
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
  CHECK_LT(table_dst_index, instance->tables()->length());
  CHECK_LT(table_src_index, instance->tables()->length());
  auto table_dst = handle(
      WasmTableObject::cast(instance->tables()->get(table_dst_index)), isolate);
  auto table_src = handle(
      WasmTableObject::cast(instance->tables()->get(table_src_index)), isolate);
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
base::Optional<MessageTemplate> WasmInstanceObject::InitTableEntries(
    Isolate* isolate, Handle<WasmInstanceObject> instance, uint32_t table_index,
    uint32_t segment_index, uint32_t dst, uint32_t src, uint32_t count) {
  AccountingAllocator allocator;
  // This {Zone} will be used only by the temporary WasmFullDecoder allocated
  // down the line from this call. Therefore it is safe to stack-allocate it
  // here.
  Zone zone(&allocator, "LoadElemSegment");

  Handle<WasmTableObject> table_object = handle(
      WasmTableObject::cast(instance->tables()->get(table_index)), isolate);

  // If needed, try to lazily initialize the element segment.
  base::Optional<MessageTemplate> opt_error =
      wasm::InitializeElementSegment(&zone, isolate, instance, segment_index);
  if (opt_error.has_value()) return opt_error;

  Handle<FixedArray> elem_segment =
      handle(FixedArray::cast(instance->element_segments()->get(segment_index)),
             isolate);
  if (!base::IsInBounds<uint64_t>(dst, count, table_object->current_length())) {
    return {MessageTemplate::kWasmTrapTableOutOfBounds};
  }
  if (!base::IsInBounds<uint64_t>(src, count, elem_segment->length())) {
    return {MessageTemplate::kWasmTrapElementSegmentOutOfBounds};
  }

  for (size_t i = 0; i < count; i++) {
    WasmTableObject::Set(
        isolate, table_object, static_cast<int>(dst + i),
        handle(elem_segment->get(static_cast<int>(src + i)), isolate));
  }

  return {};
}

MaybeHandle<WasmInternalFunction> WasmInstanceObject::GetWasmInternalFunction(
    Isolate* isolate, Handle<WasmInstanceObject> instance, int index) {
  Tagged<Object> val = instance->wasm_internal_functions()->get(index);
  if (IsSmi(val)) return {};
  return handle(WasmInternalFunction::cast(val), isolate);
}

Handle<WasmInternalFunction>
WasmInstanceObject::GetOrCreateWasmInternalFunction(
    Isolate* isolate, Handle<WasmInstanceObject> instance, int function_index) {
  MaybeHandle<WasmInternalFunction> maybe_result =
      WasmInstanceObject::GetWasmInternalFunction(isolate, instance,
                                                  function_index);
  if (!maybe_result.is_null()) {
    return maybe_result.ToHandleChecked();
  }

  Handle<HeapObject> ref =
      function_index >=
              static_cast<int>(instance->module()->num_imported_functions)
          ? instance
          : handle(HeapObject::cast(
                       instance->imported_function_refs()->get(function_index)),
                   isolate);

  if (v8_flags.wasm_to_js_generic_wrapper && IsWasmApiFunctionRef(*ref)) {
    Handle<WasmApiFunctionRef> wafr = Handle<WasmApiFunctionRef>::cast(ref);
    ref = isolate->factory()->NewWasmApiFunctionRef(
        handle(wafr->callable(), isolate),
        static_cast<wasm::Suspend>(wafr->suspend()),
        handle(wafr->instance(), isolate), handle(wafr->sig(), isolate));
  }

  Handle<Map> rtt;
  bool has_gc =
      instance->module_object()->native_module()->enabled_features().has_gc();
  if (has_gc) {
    int sig_index = instance->module()->functions[function_index].sig_index;
    // TODO(14034): Create funcref RTTs lazily?
    rtt = handle(Map::cast(instance->managed_object_maps()->get(sig_index)),
                 isolate);
  } else {
    rtt = isolate->factory()->wasm_internal_function_map();
  }

  auto result = isolate->factory()->NewWasmInternalFunction(
      instance->GetCallTarget(function_index), ref, rtt, function_index);

  if (IsWasmApiFunctionRef(*ref)) {
    Handle<WasmApiFunctionRef> wafr = Handle<WasmApiFunctionRef>::cast(ref);
    WasmApiFunctionRef::SetInternalFunctionAsCallOrigin(wafr, result);
    wafr->set_call_origin(*result);
  }
  WasmInstanceObject::SetWasmInternalFunction(instance, function_index, result);
  return result;
}

void WasmInstanceObject::SetWasmInternalFunction(
    Handle<WasmInstanceObject> instance, int index,
    Handle<WasmInternalFunction> val) {
  instance->wasm_internal_functions()->set(index, *val);
}

// static
Handle<JSFunction> WasmInternalFunction::GetOrCreateExternal(
    Handle<WasmInternalFunction> internal) {
  Isolate* isolate = GetIsolateFromWritableObject(*internal);
  if (!IsUndefined(internal->external())) {
    return handle(JSFunction::cast(internal->external()), isolate);
  }

  // {this} can either be:
  // - a declared function, i.e. {ref()} is an instance,
  // - or an imported callable, i.e. {ref()} is a WasmApiFunctionRef which
  //   refers to the imported instance.
  // It cannot be a JS/C API function as for those, the external function is set
  // at creation.
  Handle<WasmInstanceObject> instance =
      handle(IsWasmInstanceObject(internal->ref())
                 ? WasmInstanceObject::cast(internal->ref())
                 : WasmInstanceObject::cast(
                       WasmApiFunctionRef::cast(internal->ref())->instance()),
             isolate);
  const WasmModule* module = instance->module();
  const WasmFunction& function = module->functions[internal->function_index()];
  uint32_t canonical_sig_index =
      module->isorecursive_canonical_type_ids[function.sig_index];
  isolate->heap()->EnsureWasmCanonicalRttsSize(canonical_sig_index + 1);
  int wrapper_index =
      wasm::GetExportWrapperIndex(canonical_sig_index, function.imported);

  MaybeObject entry =
      isolate->heap()->js_to_wasm_wrappers()->Get(wrapper_index);

  Handle<Code> wrapper;
  // {entry} can be cleared, {undefined}, or a ready {Code}.
  if (entry.IsStrongOrWeak() && IsCode(entry.GetHeapObject())) {
    wrapper = handle(Code::cast(entry.GetHeapObject()), isolate);
  } else {
    // The wrapper may not exist yet if no function in the exports section has
    // this signature. We compile it and store the wrapper in the module for
    // later use.
    wrapper = wasm::JSToWasmWrapperCompilationUnit::CompileJSToWasmWrapper(
        isolate, function.sig, canonical_sig_index, instance->module(),
        function.imported);
  }
  if (!wrapper->is_builtin()) {
    // Store the wrapper in the isolate, or make its reference weak now that we
    // have a function referencing it.
    isolate->heap()->js_to_wasm_wrappers()->Set(
        wrapper_index, HeapObjectReference::Weak(*wrapper));
  }
  auto result = WasmExportedFunction::New(
      isolate, instance, internal, internal->function_index(),
      static_cast<int>(function.sig->parameter_count()), wrapper);

  internal->set_external(*result);
  return result;
}

Tagged<HeapObject> WasmInternalFunction::external() {
  return this->TorqueGeneratedWasmInternalFunction::external();
}

// static
void WasmApiFunctionRef::SetImportIndexAsCallOrigin(
    Handle<WasmApiFunctionRef> ref, int entry_index) {
  ref->set_call_origin(Smi::FromInt(-entry_index - 1));
}

// static
void WasmApiFunctionRef::SetIndexInTableAsCallOrigin(
    Handle<WasmApiFunctionRef> ref, int entry_index) {
  ref->set_call_origin(Smi::FromInt(entry_index + 1));
}

// static
bool WasmApiFunctionRef::CallOriginIsImportIndex(Handle<Object> call_origin) {
  return Smi::cast(*call_origin).value() < 0;
}

// static
bool WasmApiFunctionRef::CallOriginIsIndexInTable(Handle<Object> call_origin) {
  return Smi::cast(*call_origin).value() > 0;
}

// static
int WasmApiFunctionRef::CallOriginAsIndex(Handle<Object> call_origin) {
  int raw_index = Smi::cast(*call_origin).value();
  CHECK_NE(raw_index, kInvalidCallOrigin);
  if (raw_index < 0) {
    raw_index = -raw_index;
  }
  return raw_index - 1;
}

// static
void WasmApiFunctionRef::SetCrossInstanceTableIndexAsCallOrigin(
    Isolate* isolate, Handle<WasmApiFunctionRef> ref,
    Handle<WasmInstanceObject> instance, int entry_index) {
  Handle<Tuple2> tuple = isolate->factory()->NewTuple2(
      instance, handle(Smi::FromInt(entry_index + 1), isolate),
      AllocationType::kOld);
  ref->set_call_origin(*tuple);
}

// static
void WasmApiFunctionRef::SetInternalFunctionAsCallOrigin(
    Handle<WasmApiFunctionRef> ref, Handle<WasmInternalFunction> internal) {
  ref->set_call_origin(*internal);
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
  // Get the function's canonical signature index. Note that the function's
  // signature may not be present in the importing module.
  uint32_t canonical_sig_index =
      wasm::GetTypeCanonicalizer()->AddRecursiveGroup(sig);

  // Compile a wrapper for the target callable.
  Handle<JSReceiver> callable(js_function->GetCallable(), isolate);
  wasm::Suspend suspend = js_function->GetSuspend();
  wasm::WasmCodeRefScope code_ref_scope;

  const wasm::WasmModule* module = instance->module();
  auto module_canonical_ids = module->isorecursive_canonical_type_ids;
  // TODO(manoskouk): Consider adding a set of canonical indices to the module
  // to avoid this linear search.
  auto sig_in_module =
      std::find(module_canonical_ids.begin(), module_canonical_ids.end(),
                canonical_sig_index);

  if (sig_in_module != module_canonical_ids.end()) {
    wasm::NativeModule* native_module =
        instance->module_object()->native_module();
    wasm::WasmImportData resolved({}, -1, callable, sig, canonical_sig_index);
    wasm::ImportCallKind kind = resolved.kind();
    callable = resolved.callable();  // Update to ultimate target.
    DCHECK_NE(wasm::ImportCallKind::kLinkError, kind);
    // {expected_arity} should only be used if kind != kJSFunctionArityMismatch.
    int expected_arity = -1;
    if (kind == wasm::ImportCallKind ::kJSFunctionArityMismatch) {
      expected_arity = Handle<JSFunction>::cast(callable)
                           ->shared()
                           ->internal_formal_parameter_count_without_receiver();
    }

    wasm::WasmImportWrapperCache* cache = native_module->import_wrapper_cache();
    wasm::WasmCode* wasm_code =
        cache->MaybeGet(kind, canonical_sig_index, expected_arity, suspend);
    Address call_target;
    if (wasm_code) {
      call_target = wasm_code->instruction_start();
    } else if (UseGenericWasmToJSWrapper(kind, sig, resolved.suspend())) {
      call_target = isolate->builtins()
                        ->code(Builtin::kWasmToJsWrapperAsm)
                        ->instruction_start();
    } else {
      wasm::CompilationEnv env = native_module->CreateCompilationEnv();
      wasm::WasmCompilationResult result =
          compiler::CompileWasmImportCallWrapper(&env, kind, sig, false,
                                                 expected_arity, suspend);
      std::unique_ptr<wasm::WasmCode> compiled_code = native_module->AddCode(
          result.func_index, result.code_desc, result.frame_slot_count,
          result.tagged_parameter_slots,
          result.protected_instructions_data.as_vector(),
          result.source_positions.as_vector(), GetCodeKind(result),
          wasm::ExecutionTier::kNone, wasm::kNotForDebugging);
      wasm_code = native_module->PublishCode(std::move(compiled_code));
      isolate->counters()->wasm_generated_code_size()->Increment(
          wasm_code->instructions().length());
      isolate->counters()->wasm_reloc_size()->Increment(
          wasm_code->reloc_info().length());

      wasm::WasmImportWrapperCache::ModificationScope cache_scope(cache);
      wasm::WasmImportWrapperCache::CacheKey key(kind, canonical_sig_index,
                                                 expected_arity, suspend);
      cache_scope[key] = wasm_code;
      call_target = wasm_code->instruction_start();
    }

    // Update the dispatch table.
    int sig_id = static_cast<int>(
        std::distance(module_canonical_ids.begin(), sig_in_module));
    Handle<WasmApiFunctionRef> ref = isolate->factory()->NewWasmApiFunctionRef(
        callable, suspend, instance,
        wasm::SerializedSignatureHelper::SerializeSignature(
            isolate, module->signature(sig_id)));

    WasmApiFunctionRef::SetIndexInTableAsCallOrigin(ref, entry_index);
    WasmIndirectFunctionTable::cast(
        instance->indirect_function_tables()->get(table_index))
        ->Set(entry_index, canonical_sig_index, call_target, *ref);
    return;
  }
  // We pass the instance as ref-parameter as a dummy value. It will never be
  // used because `call_target` == nullptr.
  WasmIndirectFunctionTable::cast(
      instance->indirect_function_tables()->get(table_index))
      ->Set(entry_index, canonical_sig_index, kNullAddress, *instance);
}

// static
uint8_t* WasmInstanceObject::GetGlobalStorage(
    Handle<WasmInstanceObject> instance, const wasm::WasmGlobal& global) {
  DCHECK(!global.type.is_reference());
  if (global.mutability && global.imported) {
    return reinterpret_cast<uint8_t*>(
        instance->imported_mutable_globals()->get_sandboxed_pointer(
            global.index));
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
            instance->imported_mutable_globals_buffers()->get(global.index)),
        isolate);
    Address idx = instance->imported_mutable_globals()->get(global.index);
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
    case wasm::kRefNull: {
      Handle<Object> ref(TaggedField<Object>::load(*this, field_offset),
                         GetIsolateFromWritableObject(*this));
      return wasm::WasmValue(ref, field_type);
    }
    case wasm::kRtt:
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
    case wasm::kRefNull: {
      Handle<Object> ref(TaggedField<Object>::load(*this, element_offset),
                         GetIsolateFromWritableObject(*this));
      return wasm::WasmValue(ref, element_type);
    }
    case wasm::kRtt:
    case wasm::kVoid:
    case wasm::kBottom:
      UNREACHABLE();
  }
}

void WasmArray::SetTaggedElement(uint32_t index, Handle<Object> value,
                                 WriteBarrierMode mode) {
  DCHECK(type()->element_type().is_reference());
  TaggedField<Object>::store(*this, element_offset(index), *value);
  CONDITIONAL_WRITE_BARRIER(*this, element_offset(index), *value, mode);
}

// static
Handle<WasmTagObject> WasmTagObject::New(Isolate* isolate,
                                         const wasm::FunctionSig* sig,
                                         uint32_t canonical_type_index,
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
  tag_wrapper->set_canonical_type_index(canonical_type_index);
  tag_wrapper->set_tag(*tag);

  return tag_wrapper;
}

bool WasmTagObject::MatchesSignature(uint32_t expected_canonical_type_index) {
  return wasm::GetWasmEngine()->type_canonicalizer()->IsCanonicalSubtype(
      this->canonical_type_index(), expected_canonical_type_index);
}

const wasm::FunctionSig* WasmCapiFunction::GetSignature(Zone* zone) const {
  Tagged<WasmCapiFunctionData> function_data =
      shared()->wasm_capi_function_data();
  return wasm::SerializedSignatureHelper::DeserializeSignature(
      zone, function_data->serialized_signature());
}

bool WasmCapiFunction::MatchesSignature(
    uint32_t other_canonical_sig_index) const {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  const wasm::FunctionSig* sig = GetSignature(&zone);
#if DEBUG
  // TODO(14034): Change this if indexed types are allowed.
  for (wasm::ValueType type : sig->all()) CHECK(!type.has_index());
#endif
  // TODO(14034): Check for subtyping instead if C API functions can define
  // signature supertype.
  return wasm::GetWasmEngine()->type_canonicalizer()->AddRecursiveGroup(sig) ==
         other_canonical_sig_index;
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
  Handle<JSFunction> exception_cons(
      isolate->native_context()->wasm_exception_constructor(), isolate);
  Handle<JSObject> exception = isolate->factory()->NewJSObject(exception_cons);
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
    DCHECK_IMPLIES(!IsUndefined(*values), IsFixedArray(*values));
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
    wasm::JumpBuffer::StackState state, Handle<HeapObject> parent,
    AllocationType allocation_type) {
  stack->jmpbuf()->stack_limit = stack->jslimit();
  stack->jmpbuf()->sp = stack->base();
  stack->jmpbuf()->fp = kNullAddress;
  stack->jmpbuf()->state = state;
  wasm::JumpBuffer* jmpbuf = stack->jmpbuf();
  size_t external_size = stack->owned_size();
  Handle<Foreign> managed_stack = Managed<wasm::StackMemory>::FromUniquePtr(
      isolate, external_size, std::move(stack), allocation_type);
  Handle<WasmContinuationObject> result =
      isolate->factory()->NewWasmContinuationObject(
          reinterpret_cast<Address>(jmpbuf), managed_stack, parent,
          allocation_type);
  return result;
}

bool UseGenericWasmToJSWrapper(wasm::ImportCallKind kind,
                               const wasm::FunctionSig* sig,
                               wasm::Suspend suspend) {
  if (kind != wasm::ImportCallKind::kJSFunctionArityMatch &&
      kind != wasm::ImportCallKind::kJSFunctionArityMismatch) {
    return false;
  }
#if !V8_TARGET_ARCH_X64 && !V8_TARGET_ARCH_ARM64 && !V8_TARGET_ARCH_ARM && \
    !V8_TARGET_ARCH_IA32
  return false;
#else
  if (suspend == wasm::kSuspend) return false;

  return v8_flags.wasm_to_js_generic_wrapper;
#endif
}

// static
Handle<WasmContinuationObject> WasmContinuationObject::New(
    Isolate* isolate, std::unique_ptr<wasm::StackMemory> stack,
    wasm::JumpBuffer::StackState state, AllocationType allocation_type) {
  auto parent = ReadOnlyRoots(isolate).undefined_value();
  return New(isolate, std::move(stack), state, handle(parent, isolate),
             allocation_type);
}

// static
Handle<WasmContinuationObject> WasmContinuationObject::New(
    Isolate* isolate, wasm::JumpBuffer::StackState state,
    Handle<WasmContinuationObject> parent) {
  auto stack =
      std::unique_ptr<wasm::StackMemory>(wasm::StackMemory::New(isolate));
  return New(isolate, std::move(stack), state, parent);
}

// static
Handle<WasmSuspenderObject> WasmSuspenderObject::New(Isolate* isolate) {
  Handle<JSFunction> suspender_cons(
      isolate->native_context()->wasm_suspender_constructor(), isolate);
  Handle<JSPromise> promise = isolate->factory()->NewJSPromise();
  auto suspender = Handle<WasmSuspenderObject>::cast(
      isolate->factory()->NewJSObject(suspender_cons));
  suspender->set_promise(*promise);
  suspender->set_state(kInactive);
  // Instantiate the callable object which resumes this Suspender. This will be
  // used implicitly as the onFulfilled callback of the returned JS promise.
  Handle<WasmResumeData> resume_data = isolate->factory()->NewWasmResumeData(
      suspender, wasm::OnResume::kContinue);
  Handle<SharedFunctionInfo> resume_sfi =
      isolate->factory()->NewSharedFunctionInfoForWasmResume(resume_data);
  Handle<Context> context(isolate->native_context());
  Handle<JSObject> resume =
      Factory::JSFunctionBuilder{isolate, resume_sfi, context}.Build();

  Handle<WasmResumeData> reject_data =
      isolate->factory()->NewWasmResumeData(suspender, wasm::OnResume::kThrow);
  Handle<SharedFunctionInfo> reject_sfi =
      isolate->factory()->NewSharedFunctionInfoForWasmResume(reject_data);
  Handle<JSObject> reject =
      Factory::JSFunctionBuilder{isolate, reject_sfi, context}.Build();
  suspender->set_resume(*resume);
  suspender->set_reject(*reject);
  suspender->set_wasm_to_js_counter(0);
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
  return GetEncodedSize(tag->sig);
}

// static
uint32_t WasmExceptionPackage::GetEncodedSize(const wasm::WasmTagSig* sig) {
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
      case wasm::kRefNull:
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

bool WasmExportedFunction::IsWasmExportedFunction(Tagged<Object> object) {
  if (!IsJSFunction(object)) return false;
  Tagged<JSFunction> js_function = JSFunction::cast(object);
  Tagged<Code> code = js_function->code();
  if (CodeKind::JS_TO_WASM_FUNCTION != code->kind() &&
      code->builtin_id() != Builtin::kJSToWasmWrapper &&
      code->builtin_id() != Builtin::kWasmReturnPromiseOnSuspend) {
    return false;
  }
  DCHECK(js_function->shared()->HasWasmExportedFunctionData());
  return true;
}

bool WasmCapiFunction::IsWasmCapiFunction(Tagged<Object> object) {
  if (!IsJSFunction(object)) return false;
  Tagged<JSFunction> js_function = JSFunction::cast(object);
  // TODO(jkummerow): Enable this when there is a JavaScript wrapper
  // able to call this function.
  // if (js_function->code()->kind() != CodeKind::WASM_TO_CAPI_FUNCTION) {
  //   return false;
  // }
  // DCHECK(js_function->shared()->HasWasmCapiFunctionData());
  // return true;
  return js_function->shared()->HasWasmCapiFunctionData();
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
  fun_data->internal()->set_external(*result);
  return Handle<WasmCapiFunction>::cast(result);
}

Tagged<WasmInstanceObject> WasmExportedFunction::instance() {
  return shared()->wasm_exported_function_data()->instance();
}

int WasmExportedFunction::function_index() {
  return shared()->wasm_exported_function_data()->function_index();
}

Handle<WasmExportedFunction> WasmExportedFunction::New(
    Isolate* isolate, Handle<WasmInstanceObject> instance,
    Handle<WasmInternalFunction> internal, int func_index, int arity,
    Handle<Code> export_wrapper) {
  DCHECK(
      CodeKind::JS_TO_WASM_FUNCTION == export_wrapper->kind() ||
      (export_wrapper->is_builtin() &&
       (export_wrapper->builtin_id() == Builtin::kJSToWasmWrapper ||
        export_wrapper->builtin_id() == Builtin::kWasmReturnPromiseOnSuspend)));
  Factory* factory = isolate->factory();
  const wasm::FunctionSig* sig = instance->module()->functions[func_index].sig;
  Handle<Map> rtt;
  wasm::Promise promise =
      export_wrapper->builtin_id() == Builtin::kWasmReturnPromiseOnSuspend
          ? wasm::kPromise
          : wasm::kNoPromise;
  uint32_t sig_index = instance->module()->functions[func_index].sig_index;
  uint32_t canonical_type_index =
      instance->module()->isorecursive_canonical_type_ids[sig_index];
  Handle<WasmExportedFunctionData> function_data =
      factory->NewWasmExportedFunctionData(
          export_wrapper, instance, internal, func_index, sig,
          canonical_type_index, v8_flags.wasm_wrapper_tiering_budget, promise);

  MaybeHandle<String> maybe_name;
  bool is_asm_js_module = instance->module_object()->is_asm_js();
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
  DCHECK_EQ(is_asm_js_module, IsConstructor(*js_function));
  shared->set_length(arity);
  shared->set_internal_formal_parameter_count(JSParameterCount(arity));
  shared->set_script(instance->module_object()->script(), kReleaseStore);
  function_data->internal()->set_external(*js_function);
  return Handle<WasmExportedFunction>::cast(js_function);
}

Address WasmExportedFunction::GetWasmCallTarget() {
  return instance()->GetCallTarget(function_index());
}

const wasm::FunctionSig* WasmExportedFunction::sig() {
  return instance()->module()->functions[function_index()].sig;
}

bool WasmExportedFunction::MatchesSignature(
    uint32_t other_canonical_type_index) {
  return wasm::GetWasmEngine()->type_canonicalizer()->IsCanonicalSubtype(
      this->shared()->wasm_exported_function_data()->canonical_type_index(),
      other_canonical_type_index);
}

// static
std::unique_ptr<char[]> WasmExportedFunction::GetDebugName(
    const wasm::FunctionSig* sig) {
  constexpr const char kPrefix[] = "js-to-wasm:";
  // prefix + parameters + delimiter + returns + zero byte
  size_t len = strlen(kPrefix) + sig->all().size() + 2;
  auto buffer = base::OwnedVector<char>::New(len);
  memcpy(buffer.begin(), kPrefix, strlen(kPrefix));
  PrintSignature(buffer.as_vector() + strlen(kPrefix), sig);
  return buffer.ReleaseData();
}

// static
bool WasmJSFunction::IsWasmJSFunction(Tagged<Object> object) {
  if (!IsJSFunction(object)) return false;
  Tagged<JSFunction> js_function = JSFunction::cast(object);
  return js_function->shared()->HasWasmJSFunctionData();
}

Handle<Map> CreateFuncRefMap(Isolate* isolate, Handle<Map> opt_rtt_parent,
                             Handle<WasmInstanceObject> opt_instance) {
  const int inobject_properties = 0;
  const int instance_size =
      Map::cast(isolate->root(RootIndex::kWasmInternalFunctionMap))
          ->instance_size();
  const InstanceType instance_type = WASM_INTERNAL_FUNCTION_TYPE;
  const ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND;
  constexpr uint32_t kNoIndex = ~0u;
  Handle<WasmTypeInfo> type_info = isolate->factory()->NewWasmTypeInfo(
      kNullAddress, opt_rtt_parent, instance_size, opt_instance, kNoIndex);
  Handle<Map> map = isolate->factory()->NewMap(
      instance_type, instance_size, elements_kind, inobject_properties);
  map->set_wasm_type_info(*type_info);
  return map;
}

Handle<WasmJSFunction> WasmJSFunction::New(Isolate* isolate,
                                           const wasm::FunctionSig* sig,
                                           Handle<JSReceiver> callable,
                                           wasm::Suspend suspend) {
  DCHECK_LE(sig->all().size(), kMaxInt);
  int parameter_count = static_cast<int>(sig->parameter_count());
  Handle<PodArray<wasm::ValueType>> serialized_sig =
      wasm::SerializedSignatureHelper::SerializeSignature(isolate, sig);
  // TODO(wasm): Think about caching and sharing the JS-to-JS wrappers per
  // signature instead of compiling a new one for every instantiation.
  Handle<Code> wrapper_code =
      compiler::CompileJSToJSWrapper(isolate, sig, nullptr).ToHandleChecked();

  // WasmJSFunctions use on-heap Code objects as call targets, so we can't
  // cache the target address, unless the WasmJSFunction wraps a
  // WasmExportedFunction.
  Address call_target = kNullAddress;
  if (WasmExportedFunction::IsWasmExportedFunction(*callable)) {
    call_target = WasmExportedFunction::cast(*callable)->GetWasmCallTarget();
  }

  Factory* factory = isolate->factory();

  Handle<Map> rtt;
  Handle<NativeContext> context(isolate->native_context());

  if (wasm::WasmFeatures::FromIsolate(isolate).has_gc()) {
    uint32_t canonical_type_index =
        wasm::GetWasmEngine()->type_canonicalizer()->AddRecursiveGroup(sig);

    isolate->heap()->EnsureWasmCanonicalRttsSize(canonical_type_index + 1);

    Handle<WeakArrayList> canonical_rtts =
        handle(isolate->heap()->wasm_canonical_rtts(), isolate);

    MaybeObject maybe_canonical_map = canonical_rtts->Get(canonical_type_index);

    if (maybe_canonical_map.IsStrongOrWeak() &&
        IsMap(maybe_canonical_map.GetHeapObject())) {
      rtt = handle(Map::cast(maybe_canonical_map.GetHeapObject()), isolate);
    } else {
      rtt = CreateFuncRefMap(isolate, Handle<Map>(),
                             Handle<WasmInstanceObject>());
      canonical_rtts->Set(canonical_type_index,
                          HeapObjectReference::Weak(*rtt));
    }
  } else {
    rtt = factory->wasm_internal_function_map();
  }

  Handle<WasmJSFunctionData> function_data = factory->NewWasmJSFunctionData(
      call_target, callable, serialized_sig, wrapper_code, rtt, suspend,
      wasm::kNoPromise);

  if (wasm::WasmFeatures::FromIsolate(isolate).has_typed_funcref()) {
    Handle<Code> wasm_to_js_wrapper_code;
    if (UseGenericWasmToJSWrapper(wasm::kDefaultImportCallKind, sig, suspend)) {
      wasm_to_js_wrapper_code =
          isolate->builtins()->code_handle(Builtin::kWasmToJsWrapperAsm);
    } else {
      int expected_arity = parameter_count;
      wasm::ImportCallKind kind = wasm::kDefaultImportCallKind;
      if (IsJSFunction(*callable)) {
        Tagged<SharedFunctionInfo> shared =
            Handle<JSFunction>::cast(callable)->shared();
        expected_arity =
            shared->internal_formal_parameter_count_without_receiver();
        if (expected_arity != parameter_count) {
          kind = wasm::ImportCallKind::kJSFunctionArityMismatch;
        }
      }
      // TODO(wasm): Think about caching and sharing the wasm-to-JS wrappers per
      // signature instead of compiling a new one for every instantiation.
      wasm_to_js_wrapper_code = compiler::CompileWasmToJSWrapper(
                                    isolate, sig, kind, expected_arity, suspend)
                                    .ToHandleChecked();
    }
    function_data->internal()->set_code(*wasm_to_js_wrapper_code);
  }

  Handle<String> name = factory->Function_string();
  if (IsJSFunction(*callable)) {
    name = JSFunction::GetDebugName(Handle<JSFunction>::cast(callable));
    name = String::Flatten(isolate, name);
  }
  Handle<SharedFunctionInfo> shared =
      factory->NewSharedFunctionInfoForWasmJSFunction(name, function_data);
  Handle<JSFunction> js_function =
      Factory::JSFunctionBuilder{isolate, shared, context}
          .set_map(isolate->wasm_exported_function_map())
          .Build();
  js_function->shared()->set_internal_formal_parameter_count(
      JSParameterCount(parameter_count));
  function_data->internal()->set_external(*js_function);
  return Handle<WasmJSFunction>::cast(js_function);
}

Tagged<JSReceiver> WasmJSFunction::GetCallable() const {
  return JSReceiver::cast(
      WasmApiFunctionRef::cast(
          shared()->wasm_js_function_data()->internal()->ref())
          ->callable());
}

wasm::Suspend WasmJSFunction::GetSuspend() const {
  return static_cast<wasm::Suspend>(
      WasmApiFunctionRef::cast(
          shared()->wasm_js_function_data()->internal()->ref())
          ->suspend());
}

const wasm::FunctionSig* WasmJSFunction::GetSignature(Zone* zone) const {
  Tagged<WasmJSFunctionData> function_data = shared()->wasm_js_function_data();
  return wasm::SerializedSignatureHelper::DeserializeSignature(
      zone, function_data->serialized_signature());
}

bool WasmJSFunction::MatchesSignature(
    uint32_t other_canonical_sig_index) const {
  AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);
  const wasm::FunctionSig* sig = GetSignature(&zone);
#if DEBUG
  // TODO(14034): Change this if indexed types are allowed.
  for (wasm::ValueType type : sig->all()) CHECK(!type.has_index());
#endif
  // TODO(14034): Check for subtyping instead if WebAssembly.Function can define
  // signature supertype.
  return wasm::GetWasmEngine()->type_canonicalizer()->AddRecursiveGroup(sig) ==
         other_canonical_sig_index;
}

Tagged<PodArray<wasm::ValueType>> WasmCapiFunction::GetSerializedSignature()
    const {
  return shared()->wasm_capi_function_data()->serialized_signature();
}

bool WasmExternalFunction::IsWasmExternalFunction(Tagged<Object> object) {
  return WasmExportedFunction::IsWasmExportedFunction(object) ||
         WasmJSFunction::IsWasmJSFunction(object);
}

// static
MaybeHandle<WasmInternalFunction> WasmInternalFunction::FromExternal(
    Handle<Object> external, Isolate* isolate) {
  if (WasmExportedFunction::IsWasmExportedFunction(*external) ||
      WasmJSFunction::IsWasmJSFunction(*external) ||
      WasmCapiFunction::IsWasmCapiFunction(*external)) {
    Tagged<WasmFunctionData> data = WasmFunctionData::cast(
        Handle<JSFunction>::cast(external)->shared()->function_data(
            kAcquireLoad));
    return handle(data->internal(), isolate);
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
    Handle<HeapNumber> uses_bitset) {
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
  result->set_uses_bitset(*uses_bitset);
  return result;
}

namespace {
constexpr int32_t kInt31MaxValue = 0x3fffffff;
constexpr int32_t kInt31MinValue = -kInt31MaxValue - 1;

// Tries to canonicalize a HeapNumber to an i31ref Smi. Returns the original
// HeapNumber if it fails.
Handle<Object> CanonicalizeHeapNumber(Handle<Object> number, Isolate* isolate) {
  double double_value = Handle<HeapNumber>::cast(number)->value();
  if (double_value >= kInt31MinValue && double_value <= kInt31MaxValue &&
      !IsMinusZero(double_value) &&
      double_value == FastI2D(FastD2I(double_value))) {
    return handle(Smi::FromInt(FastD2I(double_value)), isolate);
  }
  return number;
}

// Tries to canonicalize a Smi into an i31 Smi. Returns a HeapNumber if it
// fails.
Handle<Object> CanonicalizeSmi(Handle<Object> smi, Isolate* isolate) {
  if constexpr (SmiValuesAre31Bits()) return smi;

  int32_t value = Smi::cast(*smi).value();

  if (value <= kInt31MaxValue && value >= kInt31MinValue) {
    return smi;
  } else {
    return isolate->factory()->NewHeapNumber(value);
  }
}
}  // namespace

namespace wasm {
MaybeHandle<Object> JSToWasmObject(Isolate* isolate, Handle<Object> value,
                                   ValueType expected_canonical,
                                   const char** error_message) {
  DCHECK(expected_canonical.is_object_reference());
  if (expected_canonical.kind() == kRefNull && IsNull(*value, isolate)) {
    switch (expected_canonical.heap_representation()) {
      case HeapType::kStringViewWtf8:
        *error_message = "stringview_wtf8 has no JS representation";
        return {};
      case HeapType::kStringViewWtf16:
        *error_message = "stringview_wtf16 has no JS representation";
        return {};
      case HeapType::kStringViewIter:
        *error_message = "stringview_iter has no JS representation";
        return {};
      default:
        bool is_extern_subtype =
            expected_canonical.heap_representation() == HeapType::kExtern ||
            expected_canonical.heap_representation() == HeapType::kNoExtern;
        return is_extern_subtype ? value : isolate->factory()->wasm_null();
    }
  }

  switch (expected_canonical.heap_representation()) {
    case HeapType::kFunc: {
      if (!(WasmExternalFunction::IsWasmExternalFunction(*value) ||
            WasmCapiFunction::IsWasmCapiFunction(*value))) {
        *error_message =
            "function-typed object must be null (if nullable) or a Wasm "
            "function object";
        return {};
      }
      return MaybeHandle<Object>(Handle<JSFunction>::cast(value)
                                     ->shared()
                                     ->wasm_function_data()
                                     ->internal(),
                                 isolate);
    }
    case HeapType::kExtern: {
      if (!IsNull(*value, isolate)) return value;
      *error_message = "null is not allowed for (ref extern)";
      return {};
    }
    case HeapType::kAny: {
      if (IsSmi(*value)) return CanonicalizeSmi(value, isolate);
      if (IsHeapNumber(*value)) {
        return CanonicalizeHeapNumber(value, isolate);
      }
      if (!IsNull(*value, isolate)) return value;
      *error_message = "null is not allowed for (ref any)";
      return {};
    }
    case HeapType::kStruct: {
      if (IsWasmStruct(*value)) {
        return value;
      }
      *error_message =
          "structref object must be null (if nullable) or a wasm struct";
      return {};
    }
    case HeapType::kArray: {
      if (IsWasmArray(*value)) {
        return value;
      }
      *error_message =
          "arrayref object must be null (if nullable) or a wasm array";
      return {};
    }
    case HeapType::kEq: {
      if (IsSmi(*value)) {
        Handle<Object> truncated = CanonicalizeSmi(value, isolate);
        if (IsSmi(*truncated)) return truncated;
      } else if (IsHeapNumber(*value)) {
        Handle<Object> truncated = CanonicalizeHeapNumber(value, isolate);
        if (IsSmi(*truncated)) return truncated;
      } else if (IsWasmStruct(*value) || IsWasmArray(*value)) {
        return value;
      }
      *error_message =
          "eqref object must be null (if nullable), or a wasm "
          "struct/array, or a Number that fits in i31ref range";
      return {};
    }
    case HeapType::kI31: {
      if (IsSmi(*value)) {
        Handle<Object> truncated = CanonicalizeSmi(value, isolate);
        if (IsSmi(*truncated)) return truncated;
      } else if (IsHeapNumber(*value)) {
        Handle<Object> truncated = CanonicalizeHeapNumber(value, isolate);
        if (IsSmi(*truncated)) return truncated;
      }
      *error_message =
          "i31ref object must be null (if nullable) or a Number that fits "
          "in i31ref range";
      return {};
    }
    case HeapType::kString:
      if (IsString(*value)) return value;
      *error_message = "wrong type (expected a string)";
      return {};
    case HeapType::kStringViewWtf8:
      *error_message = "stringview_wtf8 has no JS representation";
      return {};
    case HeapType::kStringViewWtf16:
      *error_message = "stringview_wtf16 has no JS representation";
      return {};
    case HeapType::kStringViewIter:
      *error_message = "stringview_iter has no JS representation";
      return {};
    case HeapType::kNoFunc:
    case HeapType::kNoExtern:
    case HeapType::kNone: {
      *error_message = "only null allowed for null types";
      return {};
    }
    default: {
      auto type_canonicalizer = GetWasmEngine()->type_canonicalizer();

      if (WasmExportedFunction::IsWasmExportedFunction(*value)) {
        Tagged<WasmExportedFunction> function =
            WasmExportedFunction::cast(*value);
        uint32_t real_type_index = function->shared()
                                       ->wasm_exported_function_data()
                                       ->canonical_type_index();
        if (!type_canonicalizer->IsCanonicalSubtype(
                real_type_index, expected_canonical.ref_index())) {
          *error_message =
              "assigned exported function has to be a subtype of the "
              "expected type";
          return {};
        }
        return WasmInternalFunction::FromExternal(value, isolate);
      } else if (WasmJSFunction::IsWasmJSFunction(*value)) {
        if (!WasmJSFunction::cast(*value)->MatchesSignature(
                expected_canonical.ref_index())) {
          *error_message =
              "assigned WebAssembly.Function has to be a subtype of the "
              "expected type";
          return {};
        }
        return WasmInternalFunction::FromExternal(value, isolate);
      } else if (WasmCapiFunction::IsWasmCapiFunction(*value)) {
        if (!WasmCapiFunction::cast(*value)->MatchesSignature(
                expected_canonical.ref_index())) {
          *error_message =
              "assigned C API function has to be a subtype of the expected "
              "type";
          return {};
        }
        return WasmInternalFunction::FromExternal(value, isolate);
      } else if (IsWasmStruct(*value) || IsWasmArray(*value)) {
        auto wasm_obj = Handle<WasmObject>::cast(value);
        Tagged<WasmTypeInfo> type_info = wasm_obj->map()->wasm_type_info();
        uint32_t real_idx = type_info->type_index();
        const WasmModule* real_module =
            WasmInstanceObject::cast(type_info->instance())->module();
        uint32_t real_canonical_index =
            real_module->isorecursive_canonical_type_ids[real_idx];
        if (!type_canonicalizer->IsCanonicalSubtype(
                real_canonical_index, expected_canonical.ref_index())) {
          *error_message = "object is not a subtype of expected type";
          return {};
        }
        return value;
      } else {
        *error_message = "JS object does not match expected wasm type";
        return {};
      }
    }
  }
}

// Utility which canonicalizes {expected} in addition.
MaybeHandle<Object> JSToWasmObject(Isolate* isolate, const WasmModule* module,
                                   Handle<Object> value, ValueType expected,
                                   const char** error_message) {
  ValueType expected_canonical = expected;
  if (expected_canonical.has_index()) {
    uint32_t canonical_index =
        module->isorecursive_canonical_type_ids[expected_canonical.ref_index()];
    expected_canonical = ValueType::RefMaybeNull(
        canonical_index, expected_canonical.nullability());
  }
  return JSToWasmObject(isolate, value, expected_canonical, error_message);
}

Handle<Object> WasmToJSObject(Isolate* isolate, Handle<Object> value) {
  if (IsWasmNull(*value)) {
    return isolate->factory()->null_value();
  } else if (IsWasmInternalFunction(*value)) {
    return i::WasmInternalFunction::GetOrCreateExternal(
        i::Handle<i::WasmInternalFunction>::cast(value));
  } else {
    return value;
  }
}

}  // namespace wasm

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"
#undef TRACE_IFT

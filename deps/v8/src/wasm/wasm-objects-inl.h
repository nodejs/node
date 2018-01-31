// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_OBJECTS_INL_H_
#define V8_WASM_OBJECTS_INL_H_

#include "src/heap/heap-inl.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {

// Has to be the last include (doesn't have include guards)
#include "src/objects/object-macros.h"

CAST_ACCESSOR(WasmInstanceObject)
CAST_ACCESSOR(WasmMemoryObject)
CAST_ACCESSOR(WasmModuleObject)
CAST_ACCESSOR(WasmTableObject)

#define OPTIONAL_ACCESSORS(holder, name, type, offset)           \
  bool holder::has_##name() {                                    \
    return !READ_FIELD(this, offset)->IsUndefined(GetIsolate()); \
  }                                                              \
  ACCESSORS(holder, name, type, offset)

// WasmModuleObject
ACCESSORS(WasmModuleObject, compiled_module, WasmCompiledModule,
          kCompiledModuleOffset)

// WasmTableObject
ACCESSORS(WasmTableObject, functions, FixedArray, kFunctionsOffset)
ACCESSORS(WasmTableObject, maximum_length, Object, kMaximumLengthOffset)
ACCESSORS(WasmTableObject, dispatch_tables, FixedArray, kDispatchTablesOffset)

// WasmMemoryObject
ACCESSORS(WasmMemoryObject, array_buffer, JSArrayBuffer, kArrayBufferOffset)
SMI_ACCESSORS(WasmMemoryObject, maximum_pages, kMaximumPagesOffset)
OPTIONAL_ACCESSORS(WasmMemoryObject, instances, WeakFixedArray,
                   kInstancesOffset)

// WasmInstanceObject
ACCESSORS(WasmInstanceObject, wasm_context, Managed<WasmContext>,
          kWasmContextOffset)
ACCESSORS(WasmInstanceObject, compiled_module, WasmCompiledModule,
          kCompiledModuleOffset)
ACCESSORS(WasmInstanceObject, exports_object, JSObject, kExportsObjectOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, memory_object, WasmMemoryObject,
                   kMemoryObjectOffset)
ACCESSORS(WasmInstanceObject, globals_buffer, JSArrayBuffer,
          kGlobalsBufferOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, debug_info, WasmDebugInfo,
                   kDebugInfoOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, table_object, WasmTableObject,
                   kTableObjectOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, function_tables, FixedArray,
                   kFunctionTablesOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, signature_tables, FixedArray,
                   kSignatureTablesOffset)
ACCESSORS(WasmInstanceObject, directly_called_instances, FixedArray,
          kDirectlyCalledInstancesOffset)
ACCESSORS(WasmInstanceObject, js_imports_table, FixedArray,
          kJsImportsTableOffset)

// WasmSharedModuleData
ACCESSORS(WasmSharedModuleData, module_bytes, SeqOneByteString,
          kModuleBytesOffset)
ACCESSORS(WasmSharedModuleData, script, Script, kScriptOffset)
OPTIONAL_ACCESSORS(WasmSharedModuleData, asm_js_offset_table, ByteArray,
                   kAsmJsOffsetTableOffset)
OPTIONAL_ACCESSORS(WasmSharedModuleData, breakpoint_infos, FixedArray,
                   kBreakPointInfosOffset)

OPTIONAL_ACCESSORS(WasmSharedModuleData, lazy_compilation_orchestrator, Foreign,
                   kLazyCompilationOrchestratorOffset)

OPTIONAL_ACCESSORS(WasmDebugInfo, locals_names, FixedArray, kLocalsNamesOffset)
OPTIONAL_ACCESSORS(WasmDebugInfo, c_wasm_entries, FixedArray,
                   kCWasmEntriesOffset)
OPTIONAL_ACCESSORS(WasmDebugInfo, c_wasm_entry_map, Managed<wasm::SignatureMap>,
                   kCWasmEntryMapOffset)

#undef OPTIONAL_ACCESSORS

#define FORWARD_SHARED(type, name) \
  type WasmCompiledModule::name() { return shared()->name(); }
FORWARD_SHARED(SeqOneByteString*, module_bytes)
FORWARD_SHARED(wasm::WasmModule*, module)
FORWARD_SHARED(Script*, script)
FORWARD_SHARED(bool, is_asm_js)
#undef FORWARD_SHARED

#define WCM_OBJECT_OR_WEAK(TYPE, NAME, ID, TYPE_CHECK, SETTER_MODIFIER) \
  Handle<TYPE> WasmCompiledModule::NAME() const {                       \
    return handle(ptr_to_##NAME());                                     \
  }                                                                     \
                                                                        \
  MaybeHandle<TYPE> WasmCompiledModule::maybe_##NAME() const {          \
    if (has_##NAME()) return NAME();                                    \
    return MaybeHandle<TYPE>();                                         \
  }                                                                     \
                                                                        \
  TYPE* WasmCompiledModule::maybe_ptr_to_##NAME() const {               \
    Object* obj = get(ID);                                              \
    if (!(TYPE_CHECK)) return nullptr;                                  \
    return TYPE::cast(obj);                                             \
  }                                                                     \
                                                                        \
  TYPE* WasmCompiledModule::ptr_to_##NAME() const {                     \
    Object* obj = get(ID);                                              \
    DCHECK(TYPE_CHECK);                                                 \
    return TYPE::cast(obj);                                             \
  }                                                                     \
                                                                        \
  bool WasmCompiledModule::has_##NAME() const {                         \
    Object* obj = get(ID);                                              \
    return TYPE_CHECK;                                                  \
  }                                                                     \
                                                                        \
  void WasmCompiledModule::reset_##NAME() { set_undefined(ID); }        \
                                                                        \
  void WasmCompiledModule::set_##NAME(Handle<TYPE> value) {             \
    set_ptr_to_##NAME(*value);                                          \
  }                                                                     \
  void WasmCompiledModule::set_ptr_to_##NAME(TYPE* value) { set(ID, value); }

#define WCM_OBJECT(TYPE, NAME) \
  WCM_OBJECT_OR_WEAK(TYPE, NAME, kID_##NAME, obj->Is##TYPE(), public)

#define WCM_CONST_OBJECT(TYPE, NAME) \
  WCM_OBJECT_OR_WEAK(TYPE, NAME, kID_##NAME, obj->Is##TYPE(), private)

#define WCM_WASM_OBJECT(TYPE, NAME) \
  WCM_OBJECT_OR_WEAK(TYPE, NAME, kID_##NAME, TYPE::Is##TYPE(obj), private)

#define WCM_SMALL_CONST_NUMBER(TYPE, NAME)                 \
  TYPE WasmCompiledModule::NAME() const {                  \
    return static_cast<TYPE>(Smi::ToInt(get(kID_##NAME))); \
  }                                                        \
                                                           \
  void WasmCompiledModule::set_##NAME(TYPE value) {        \
    set(kID_##NAME, Smi::FromInt(value));                  \
  }

#define WCM_WEAK_LINK(TYPE, NAME)                                          \
  WCM_OBJECT_OR_WEAK(WeakCell, weak_##NAME, kID_##NAME, obj->IsWeakCell(), \
                     public)                                               \
                                                                           \
  Handle<TYPE> WasmCompiledModule::NAME() const {                          \
    return handle(TYPE::cast(weak_##NAME()->value()));                     \
  }

#define DEFINITION(KIND, TYPE, NAME) WCM_##KIND(TYPE, NAME)
WCM_PROPERTY_TABLE(DEFINITION)
#undef DECLARATION

#undef WCM_CONST_OBJECT
#undef WCM_LARGE_NUMBER
#undef WCM_OBJECT_OR_WEAK
#undef WCM_SMALL_CONST_NUMBER
#undef WCM_WEAK_LINK

uint32_t WasmTableObject::current_length() { return functions()->length(); }

bool WasmTableObject::has_maximum_length() {
  return maximum_length()->Number() >= 0;
}

bool WasmMemoryObject::has_maximum_pages() { return maximum_pages() >= 0; }

void WasmCompiledModule::ReplaceCodeTableForTesting(
    Handle<FixedArray> testing_table) {
  set_code_table(testing_table);
}

#include "src/objects/object-macros-undef.h"

}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_OBJECTS_INL_H_

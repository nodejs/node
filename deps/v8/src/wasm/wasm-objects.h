// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_OBJECTS_H_
#define V8_WASM_OBJECTS_H_

#include "src/debug/interface-types.h"
#include "src/objects-inl.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/managed.h"
#include "src/wasm/wasm-limits.h"

namespace v8 {
namespace internal {
namespace wasm {
struct WasmModule;
}

class WasmCompiledModule;
class WasmDebugInfo;
class WasmInstanceObject;
class WasmInstanceWrapper;

#define DECLARE_CASTS(name)             \
  static bool Is##name(Object* object); \
  static name* cast(Object* object)

#define DECLARE_GETTER(name, type) type* name()

#define DECLARE_ACCESSORS(name, type) \
  void set_##name(type* value);       \
  DECLARE_GETTER(name, type)

#define DECLARE_OPTIONAL_ACCESSORS(name, type) \
  bool has_##name();                           \
  DECLARE_ACCESSORS(name, type)

// Representation of a WebAssembly.Module JavaScript-level object.
class WasmModuleObject : public JSObject {
 public:
  // TODO(titzer): add the brand as an internal field instead of a property.
  enum Fields { kCompiledModule, kFieldCount };

  DECLARE_CASTS(WasmModuleObject);

  WasmCompiledModule* compiled_module();

  static Handle<WasmModuleObject> New(
      Isolate* isolate, Handle<WasmCompiledModule> compiled_module);
};

// Representation of a WebAssembly.Table JavaScript-level object.
class WasmTableObject : public JSObject {
 public:
  // TODO(titzer): add the brand as an internal field instead of a property.
  enum Fields { kFunctions, kMaximum, kDispatchTables, kFieldCount };

  DECLARE_CASTS(WasmTableObject);
  DECLARE_ACCESSORS(functions, FixedArray);

  FixedArray* dispatch_tables();
  uint32_t current_length();
  bool has_maximum_length();
  int64_t maximum_length();  // Returns < 0 if no maximum.

  static Handle<WasmTableObject> New(Isolate* isolate, uint32_t initial,
                                     int64_t maximum,
                                     Handle<FixedArray>* js_functions);
  static void Grow(Isolate* isolate, Handle<WasmTableObject> table,
                   uint32_t count);
  static Handle<FixedArray> AddDispatchTable(
      Isolate* isolate, Handle<WasmTableObject> table,
      Handle<WasmInstanceObject> instance, int table_index,
      Handle<FixedArray> function_table, Handle<FixedArray> signature_table);
};

// Representation of a WebAssembly.Memory JavaScript-level object.
class WasmMemoryObject : public JSObject {
 public:
  // TODO(titzer): add the brand as an internal field instead of a property.
  enum Fields : uint8_t { kArrayBuffer, kMaximum, kInstancesLink, kFieldCount };

  DECLARE_CASTS(WasmMemoryObject);
  DECLARE_ACCESSORS(buffer, JSArrayBuffer);
  DECLARE_OPTIONAL_ACCESSORS(instances_link, WasmInstanceWrapper);

  void AddInstance(Isolate* isolate, Handle<WasmInstanceObject> object);
  void ResetInstancesLink(Isolate* isolate);
  uint32_t current_pages();
  bool has_maximum_pages();
  int32_t maximum_pages();  // Returns < 0 if there is no maximum.

  static Handle<WasmMemoryObject> New(Isolate* isolate,
                                      Handle<JSArrayBuffer> buffer,
                                      int32_t maximum);

  static bool Grow(Isolate* isolate, Handle<WasmMemoryObject> memory,
                   uint32_t count);
};

// Representation of a WebAssembly.Instance JavaScript-level object.
class WasmInstanceObject : public JSObject {
 public:
  // TODO(titzer): add the brand as an internal field instead of a property.
  enum Fields {
    kCompiledModule,
    kMemoryObject,
    kMemoryArrayBuffer,
    kGlobalsArrayBuffer,
    kDebugInfo,
    kWasmMemInstanceWrapper,
    kFieldCount
  };

  DECLARE_CASTS(WasmInstanceObject);

  DECLARE_ACCESSORS(compiled_module, WasmCompiledModule);
  DECLARE_OPTIONAL_ACCESSORS(globals_buffer, JSArrayBuffer);
  DECLARE_OPTIONAL_ACCESSORS(memory_buffer, JSArrayBuffer);
  DECLARE_OPTIONAL_ACCESSORS(memory_object, WasmMemoryObject);
  DECLARE_OPTIONAL_ACCESSORS(debug_info, WasmDebugInfo);
  DECLARE_OPTIONAL_ACCESSORS(instance_wrapper, WasmInstanceWrapper);

  WasmModuleObject* module_object();
  wasm::WasmModule* module();

  // Get the debug info associated with the given wasm object.
  // If no debug info exists yet, it is created automatically.
  static Handle<WasmDebugInfo> GetOrCreateDebugInfo(
      Handle<WasmInstanceObject> instance);

  static Handle<WasmInstanceObject> New(
      Isolate* isolate, Handle<WasmCompiledModule> compiled_module);
};

// Representation of an exported WASM function.
class WasmExportedFunction : public JSFunction {
 public:
  enum Fields { kInstance, kIndex, kFieldCount };

  DECLARE_CASTS(WasmExportedFunction);

  WasmInstanceObject* instance();
  int function_index();

  static Handle<WasmExportedFunction> New(Isolate* isolate,
                                          Handle<WasmInstanceObject> instance,
                                          MaybeHandle<String> maybe_name,
                                          int func_index, int arity,
                                          Handle<Code> export_wrapper);
};

// Information shared by all WasmCompiledModule objects for the same module.
class WasmSharedModuleData : public FixedArray {
  enum Fields {
    kModuleWrapper,
    kModuleBytes,
    kScript,
    kAsmJsOffsetTable,
    kFieldCount
  };

 public:
  DECLARE_CASTS(WasmSharedModuleData);

  DECLARE_GETTER(module, wasm::WasmModule);
  DECLARE_OPTIONAL_ACCESSORS(module_bytes, SeqOneByteString);
  DECLARE_GETTER(script, Script);
  DECLARE_OPTIONAL_ACCESSORS(asm_js_offset_table, ByteArray);

  static Handle<WasmSharedModuleData> New(
      Isolate* isolate, Handle<Foreign> module_wrapper,
      Handle<SeqOneByteString> module_bytes, Handle<Script> script,
      Handle<ByteArray> asm_js_offset_table);

  // Check whether this module was generated from asm.js source.
  bool is_asm_js();

  // Recreate the ModuleWrapper from the module bytes after deserialization.
  static void RecreateModuleWrapper(Isolate*, Handle<WasmSharedModuleData>);
};

class WasmCompiledModule : public FixedArray {
 public:
  enum Fields { kFieldCount };

  static WasmCompiledModule* cast(Object* fixed_array) {
    SLOW_DCHECK(IsWasmCompiledModule(fixed_array));
    return reinterpret_cast<WasmCompiledModule*>(fixed_array);
  }

#define WCM_OBJECT_OR_WEAK(TYPE, NAME, ID, TYPE_CHECK)               \
  Handle<TYPE> NAME() const { return handle(ptr_to_##NAME()); }      \
                                                                     \
  MaybeHandle<TYPE> maybe_##NAME() const {                           \
    if (has_##NAME()) return NAME();                                 \
    return MaybeHandle<TYPE>();                                      \
  }                                                                  \
                                                                     \
  TYPE* maybe_ptr_to_##NAME() const {                                \
    Object* obj = get(ID);                                           \
    if (!(TYPE_CHECK)) return nullptr;                               \
    return TYPE::cast(obj);                                          \
  }                                                                  \
                                                                     \
  TYPE* ptr_to_##NAME() const {                                      \
    Object* obj = get(ID);                                           \
    DCHECK(TYPE_CHECK);                                              \
    return TYPE::cast(obj);                                          \
  }                                                                  \
                                                                     \
  void set_##NAME(Handle<TYPE> value) { set_ptr_to_##NAME(*value); } \
                                                                     \
  void set_ptr_to_##NAME(TYPE* value) { set(ID, value); }            \
                                                                     \
  bool has_##NAME() const {                                          \
    Object* obj = get(ID);                                           \
    return TYPE_CHECK;                                               \
  }                                                                  \
                                                                     \
  void reset_##NAME() { set_undefined(ID); }

#define WCM_OBJECT(TYPE, NAME) \
  WCM_OBJECT_OR_WEAK(TYPE, NAME, kID_##NAME, obj->Is##TYPE())

#define WCM_WASM_OBJECT(TYPE, NAME) \
  WCM_OBJECT_OR_WEAK(TYPE, NAME, kID_##NAME, TYPE::Is##TYPE(obj))

#define WCM_SMALL_NUMBER(TYPE, NAME)                               \
  TYPE NAME() const {                                              \
    return static_cast<TYPE>(Smi::cast(get(kID_##NAME))->value()); \
  }                                                                \
  void set_##NAME(TYPE value) { set(kID_##NAME, Smi::FromInt(value)); }

#define WCM_WEAK_LINK(TYPE, NAME)                                           \
  WCM_OBJECT_OR_WEAK(WeakCell, weak_##NAME, kID_##NAME, obj->IsWeakCell()); \
                                                                            \
  Handle<TYPE> NAME() const {                                               \
    return handle(TYPE::cast(weak_##NAME()->value()));                      \
  }

#define CORE_WCM_PROPERTY_TABLE(MACRO)                  \
  MACRO(WASM_OBJECT, WasmSharedModuleData, shared)      \
  MACRO(OBJECT, Context, native_context)                \
  MACRO(SMALL_NUMBER, uint32_t, num_imported_functions) \
  MACRO(OBJECT, FixedArray, code_table)                 \
  MACRO(OBJECT, FixedArray, weak_exported_functions)    \
  MACRO(OBJECT, FixedArray, function_tables)            \
  MACRO(OBJECT, FixedArray, signature_tables)           \
  MACRO(OBJECT, FixedArray, empty_function_tables)      \
  MACRO(OBJECT, JSArrayBuffer, memory)                  \
  MACRO(SMALL_NUMBER, uint32_t, min_mem_pages)          \
  MACRO(SMALL_NUMBER, uint32_t, max_mem_pages)          \
  MACRO(WEAK_LINK, WasmCompiledModule, next_instance)   \
  MACRO(WEAK_LINK, WasmCompiledModule, prev_instance)   \
  MACRO(WEAK_LINK, JSObject, owning_instance)           \
  MACRO(WEAK_LINK, WasmModuleObject, wasm_module)

#if DEBUG
#define DEBUG_ONLY_TABLE(MACRO) MACRO(SMALL_NUMBER, uint32_t, instance_id)
#else
#define DEBUG_ONLY_TABLE(IGNORE)
  uint32_t instance_id() const { return -1; }
#endif

#define WCM_PROPERTY_TABLE(MACRO) \
  CORE_WCM_PROPERTY_TABLE(MACRO)  \
  DEBUG_ONLY_TABLE(MACRO)

 private:
  enum PropertyIndices {
#define INDICES(IGNORE1, IGNORE2, NAME) kID_##NAME,
    WCM_PROPERTY_TABLE(INDICES) Count
#undef INDICES
  };

 public:
  static Handle<WasmCompiledModule> New(Isolate* isolate,
                                        Handle<WasmSharedModuleData> shared);

  static Handle<WasmCompiledModule> Clone(Isolate* isolate,
                                          Handle<WasmCompiledModule> module) {
    Handle<WasmCompiledModule> ret = Handle<WasmCompiledModule>::cast(
        isolate->factory()->CopyFixedArray(module));
    ret->InitId();
    ret->reset_weak_owning_instance();
    ret->reset_weak_next_instance();
    ret->reset_weak_prev_instance();
    ret->reset_weak_exported_functions();
    return ret;
  }

  uint32_t mem_size() const;
  uint32_t default_mem_size() const;

#define DECLARATION(KIND, TYPE, NAME) WCM_##KIND(TYPE, NAME)
  WCM_PROPERTY_TABLE(DECLARATION)
#undef DECLARATION

// Allow to call method on WasmSharedModuleData also on this object.
#define FORWARD_SHARED(type, name) \
  type name() { return shared()->name(); }
  FORWARD_SHARED(SeqOneByteString*, module_bytes)
  FORWARD_SHARED(wasm::WasmModule*, module)
  FORWARD_SHARED(Script*, script)
  FORWARD_SHARED(bool, is_asm_js)
#undef FORWARD_SHARED

  static bool IsWasmCompiledModule(Object* obj);

  void PrintInstancesChain();

  // Recreate the ModuleWrapper from the module bytes after deserialization.
  static void RecreateModuleWrapper(Isolate* isolate,
                                    Handle<WasmCompiledModule> compiled_module);

  // Get the function name of the function identified by the given index.
  // Returns a null handle if the function is unnamed or the name is not a valid
  // UTF-8 string.
  static MaybeHandle<String> GetFunctionNameOrNull(
      Isolate* isolate, Handle<WasmCompiledModule> compiled_module,
      uint32_t func_index);

  // Get the function name of the function identified by the given index.
  // Returns "<WASM UNNAMED>" if the function is unnamed or the name is not a
  // valid UTF-8 string.
  static Handle<String> GetFunctionName(
      Isolate* isolate, Handle<WasmCompiledModule> compiled_module,
      uint32_t func_index);

  // Get the raw bytes of the function name of the function identified by the
  // given index.
  // Meant to be used for debugging or frame printing.
  // Does not allocate, hence gc-safe.
  Vector<const uint8_t> GetRawFunctionName(uint32_t func_index);

  // Return the byte offset of the function identified by the given index.
  // The offset will be relative to the start of the module bytes.
  // Returns -1 if the function index is invalid.
  int GetFunctionOffset(uint32_t func_index);

  // Returns the function containing the given byte offset.
  // Returns -1 if the byte offset is not contained in any function of this
  // module.
  int GetContainingFunction(uint32_t byte_offset);

  // Translate from byte offset in the module to function number and byte offset
  // within that function, encoded as line and column in the position info.
  // Returns true if the position is valid inside this module, false otherwise.
  bool GetPositionInfo(uint32_t position, Script::PositionInfo* info);

  // Get the asm.js source position from a byte offset.
  // Must only be called if the associated wasm object was created from asm.js.
  static int GetAsmJsSourcePosition(Handle<WasmCompiledModule> compiled_module,
                                    uint32_t func_index, uint32_t byte_offset,
                                    bool is_at_number_conversion);

  // Compute the disassembly of a wasm function.
  // Returns the disassembly string and a list of <byte_offset, line, column>
  // entries, mapping wasm byte offsets to line and column in the disassembly.
  // The list is guaranteed to be ordered by the byte_offset.
  // Returns an empty string and empty vector if the function index is invalid.
  debug::WasmDisassembly DisassembleFunction(int func_index);

  // Extract a portion of the wire bytes as UTF-8 string.
  // Returns a null handle if the respective bytes do not form a valid UTF-8
  // string.
  static MaybeHandle<String> ExtractUtf8StringFromModuleBytes(
      Isolate* isolate, Handle<WasmCompiledModule> compiled_module,
      uint32_t offset, uint32_t size);

  // Get a list of all possible breakpoints within a given range of this module.
  bool GetPossibleBreakpoints(const debug::Location& start,
                              const debug::Location& end,
                              std::vector<debug::Location>* locations);

 private:
  void InitId();

  DISALLOW_IMPLICIT_CONSTRUCTORS(WasmCompiledModule);
};

class WasmDebugInfo : public FixedArray {
 public:
  enum Fields {
    kInstance,
    kInterpreterHandle,
    kInterpretedFunctions,
    kFieldCount
  };

  static Handle<WasmDebugInfo> New(Handle<WasmInstanceObject>);

  static bool IsDebugInfo(Object*);
  static WasmDebugInfo* cast(Object*);

  static void SetBreakpoint(Handle<WasmDebugInfo>, int func_index, int offset);

  static void RunInterpreter(Handle<WasmDebugInfo>, int func_index,
                             uint8_t* arg_buffer);

  DECLARE_GETTER(wasm_instance, WasmInstanceObject);
};

class WasmInstanceWrapper : public FixedArray {
 public:
  static Handle<WasmInstanceWrapper> New(Isolate* isolate,
                                         Handle<WasmInstanceObject> instance);
  static WasmInstanceWrapper* cast(Object* fixed_array) {
    SLOW_DCHECK(IsWasmInstanceWrapper(fixed_array));
    return reinterpret_cast<WasmInstanceWrapper*>(fixed_array);
  }
  static bool IsWasmInstanceWrapper(Object* obj);
  bool has_instance() { return get(kWrapperInstanceObject)->IsWeakCell(); }
  Handle<WasmInstanceObject> instance_object() {
    Object* obj = get(kWrapperInstanceObject);
    DCHECK(obj->IsWeakCell());
    WeakCell* cell = WeakCell::cast(obj);
    DCHECK(cell->value()->IsJSObject());
    return handle(WasmInstanceObject::cast(cell->value()));
  }
  bool has_next() { return IsWasmInstanceWrapper(get(kNextInstanceWrapper)); }
  bool has_previous() {
    return IsWasmInstanceWrapper(get(kPreviousInstanceWrapper));
  }
  void set_instance_object(Handle<JSObject> instance, Isolate* isolate);
  void set_next_wrapper(Object* obj) {
    DCHECK(IsWasmInstanceWrapper(obj));
    set(kNextInstanceWrapper, obj);
  }
  void set_previous_wrapper(Object* obj) {
    DCHECK(IsWasmInstanceWrapper(obj));
    set(kPreviousInstanceWrapper, obj);
  }
  Handle<WasmInstanceWrapper> next_wrapper() {
    Object* obj = get(kNextInstanceWrapper);
    DCHECK(IsWasmInstanceWrapper(obj));
    return handle(WasmInstanceWrapper::cast(obj));
  }
  Handle<WasmInstanceWrapper> previous_wrapper() {
    Object* obj = get(kPreviousInstanceWrapper);
    DCHECK(IsWasmInstanceWrapper(obj));
    return handle(WasmInstanceWrapper::cast(obj));
  }
  void reset_next_wrapper() { set_undefined(kNextInstanceWrapper); }
  void reset_previous_wrapper() { set_undefined(kPreviousInstanceWrapper); }
  void reset() {
    for (int kID = 0; kID < kWrapperPropertyCount; kID++) set_undefined(kID);
  }

 private:
  enum {
    kWrapperInstanceObject,
    kNextInstanceWrapper,
    kPreviousInstanceWrapper,
    kWrapperPropertyCount
  };
};

#undef DECLARE_ACCESSORS
#undef DECLARE_OPTIONAL_ACCESSORS

}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_OBJECTS_H_

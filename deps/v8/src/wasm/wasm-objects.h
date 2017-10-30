// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_OBJECTS_H_
#define V8_WASM_OBJECTS_H_

#include "src/debug/debug.h"
#include "src/debug/interface-types.h"
#include "src/objects.h"
#include "src/objects/script.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"

#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"

// Has to be the last include (doesn't have include guards)
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {
namespace wasm {
class InterpretedFrame;
class WasmInterpreter;

typedef Address GlobalHandleAddress;

}  // namespace wasm

class WasmCompiledModule;
class WasmDebugInfo;
class WasmInstanceObject;

#define DECL_OOL_QUERY(type) static bool Is##type(Object* object);
#define DECL_OOL_CAST(type) static type* cast(Object* object);

#define DECL_GETTER(name, type) type* name();

#define DECL_OPTIONAL_ACCESSORS(name, type) \
  INLINE(bool has_##name());                \
  DECL_ACCESSORS(name, type)

#define DEF_SIZE(parent)                                                     \
  static const int kSize = parent::kHeaderSize + kFieldCount * kPointerSize; \
  static const int kParentSize = parent::kHeaderSize;                        \
  static const int kHeaderSize = kSize;
#define DEF_OFFSET(name)             \
  static const int k##name##Offset = \
      kSize + (k##name##Index - kFieldCount) * kPointerSize;

// Representation of a WebAssembly.Module JavaScript-level object.
class WasmModuleObject : public JSObject {
 public:
  DECL_CAST(WasmModuleObject)

  // Shared compiled code between multiple WebAssembly.Module objects.
  DECL_ACCESSORS(compiled_module, WasmCompiledModule)

  enum {  // --
    kCompiledModuleIndex,
    kFieldCount
  };

  DEF_SIZE(JSObject)
  DEF_OFFSET(CompiledModule)

  static Handle<WasmModuleObject> New(
      Isolate* isolate, Handle<WasmCompiledModule> compiled_module);
};

// Representation of a WebAssembly.Table JavaScript-level object.
class WasmTableObject : public JSObject {
 public:
  DECL_CAST(WasmTableObject)

  DECL_ACCESSORS(functions, FixedArray)
  // TODO(titzer): introduce DECL_I64_ACCESSORS macro
  DECL_ACCESSORS(maximum_length, Object)
  DECL_ACCESSORS(dispatch_tables, FixedArray)

  enum {  // --
    kFunctionsIndex,
    kMaximumLengthIndex,
    kDispatchTablesIndex,
    kFieldCount
  };

  DEF_SIZE(JSObject)
  DEF_OFFSET(Functions)
  DEF_OFFSET(MaximumLength)
  DEF_OFFSET(DispatchTables)

  inline uint32_t current_length() { return functions()->length(); }
  inline bool has_maximum_length() { return maximum_length()->Number() >= 0; }
  void grow(Isolate* isolate, uint32_t count);

  static Handle<WasmTableObject> New(Isolate* isolate, uint32_t initial,
                                     int64_t maximum,
                                     Handle<FixedArray>* js_functions);
  static Handle<FixedArray> AddDispatchTable(
      Isolate* isolate, Handle<WasmTableObject> table,
      Handle<WasmInstanceObject> instance, int table_index,
      Handle<FixedArray> function_table, Handle<FixedArray> signature_table);
};

// Representation of a WebAssembly.Memory JavaScript-level object.
class WasmMemoryObject : public JSObject {
 public:
  DECL_CAST(WasmMemoryObject)

  DECL_ACCESSORS(array_buffer, JSArrayBuffer)
  DECL_INT_ACCESSORS(maximum_pages)
  DECL_OPTIONAL_ACCESSORS(instances, WeakFixedArray)

  enum {  // --
    kArrayBufferIndex,
    kMaximumPagesIndex,
    kInstancesIndex,
    kFieldCount
  };

  DEF_SIZE(JSObject)
  DEF_OFFSET(ArrayBuffer)
  DEF_OFFSET(MaximumPages)
  DEF_OFFSET(Instances)

  // Add an instance to the internal (weak) list. amortized O(n).
  static void AddInstance(Isolate* isolate, Handle<WasmMemoryObject> memory,
                          Handle<WasmInstanceObject> object);
  // Remove an instance from the internal (weak) list. O(n).
  static void RemoveInstance(Isolate* isolate, Handle<WasmMemoryObject> memory,
                             Handle<WasmInstanceObject> object);
  uint32_t current_pages();
  inline bool has_maximum_pages() { return maximum_pages() >= 0; }

  static Handle<WasmMemoryObject> New(Isolate* isolate,
                                      Handle<JSArrayBuffer> buffer,
                                      int32_t maximum);

  static int32_t Grow(Isolate*, Handle<WasmMemoryObject>, uint32_t pages);
};

// A WebAssembly.Instance JavaScript-level object.
class WasmInstanceObject : public JSObject {
 public:
  DECL_CAST(WasmInstanceObject)

  DECL_ACCESSORS(compiled_module, WasmCompiledModule)
  DECL_ACCESSORS(exports_object, JSObject)
  DECL_OPTIONAL_ACCESSORS(memory_object, WasmMemoryObject)
  DECL_OPTIONAL_ACCESSORS(memory_buffer, JSArrayBuffer)
  DECL_OPTIONAL_ACCESSORS(globals_buffer, JSArrayBuffer)
  DECL_OPTIONAL_ACCESSORS(debug_info, WasmDebugInfo)
  DECL_OPTIONAL_ACCESSORS(function_tables, FixedArray)
  DECL_OPTIONAL_ACCESSORS(signature_tables, FixedArray)

  // FixedArray of all instances whose code was imported
  DECL_OPTIONAL_ACCESSORS(directly_called_instances, FixedArray)

  enum {  // --
    kCompiledModuleIndex,
    kExportsObjectIndex,
    kMemoryObjectIndex,
    kMemoryBufferIndex,
    kGlobalsBufferIndex,
    kDebugInfoIndex,
    kFunctionTablesIndex,
    kSignatureTablesIndex,
    kDirectlyCalledInstancesIndex,
    kFieldCount
  };

  DEF_SIZE(JSObject)
  DEF_OFFSET(CompiledModule)
  DEF_OFFSET(ExportsObject)
  DEF_OFFSET(MemoryObject)
  DEF_OFFSET(MemoryBuffer)
  DEF_OFFSET(GlobalsBuffer)
  DEF_OFFSET(DebugInfo)
  DEF_OFFSET(FunctionTables)
  DEF_OFFSET(SignatureTables)
  DEF_OFFSET(DirectlyCalledInstances)

  WasmModuleObject* module_object();
  V8_EXPORT_PRIVATE wasm::WasmModule* module();

  // Get the debug info associated with the given wasm object.
  // If no debug info exists yet, it is created automatically.
  static Handle<WasmDebugInfo> GetOrCreateDebugInfo(Handle<WasmInstanceObject>);

  static Handle<WasmInstanceObject> New(Isolate*, Handle<WasmCompiledModule>);

  int32_t GetMemorySize();

  static int32_t GrowMemory(Isolate*, Handle<WasmInstanceObject>,
                            uint32_t pages);

  uint32_t GetMaxMemoryPages();
};

// A WASM function that is wrapped and exported to JavaScript.
class WasmExportedFunction : public JSFunction {
 public:
  WasmInstanceObject* instance();
  int function_index();

  static WasmExportedFunction* cast(Object* object);
  static bool IsWasmExportedFunction(Object* object);

  static Handle<WasmExportedFunction> New(Isolate* isolate,
                                          Handle<WasmInstanceObject> instance,
                                          MaybeHandle<String> maybe_name,
                                          int func_index, int arity,
                                          Handle<Code> export_wrapper);
};

// Information shared by all WasmCompiledModule objects for the same module.
class WasmSharedModuleData : public FixedArray {
 public:
  DECL_OOL_QUERY(WasmSharedModuleData)
  DECL_OOL_CAST(WasmSharedModuleData)

  DECL_GETTER(module, wasm::WasmModule)
  DECL_OPTIONAL_ACCESSORS(module_bytes, SeqOneByteString)
  DECL_ACCESSORS(script, Script)
  DECL_OPTIONAL_ACCESSORS(asm_js_offset_table, ByteArray)
  DECL_OPTIONAL_ACCESSORS(breakpoint_infos, FixedArray)

  enum {  // --
    kModuleWrapperIndex,
    kModuleBytesIndex,
    kScriptIndex,
    kAsmJsOffsetTableIndex,
    kBreakPointInfosIndex,
    kLazyCompilationOrchestratorIndex,
    kFieldCount
  };

  DEF_SIZE(FixedArray)
  DEF_OFFSET(ModuleWrapper)
  DEF_OFFSET(ModuleBytes)
  DEF_OFFSET(Script)
  DEF_OFFSET(AsmJsOffsetTable)
  DEF_OFFSET(BreakPointInfos)
  DEF_OFFSET(LazyCompilationOrchestrator)

  // Check whether this module was generated from asm.js source.
  bool is_asm_js();

  static void ReinitializeAfterDeserialization(Isolate*,
                                               Handle<WasmSharedModuleData>);

  static void AddBreakpoint(Handle<WasmSharedModuleData>, int position,
                            Handle<Object> break_point_object);

  static void SetBreakpointsOnNewInstance(Handle<WasmSharedModuleData>,
                                          Handle<WasmInstanceObject>);

  static void PrepareForLazyCompilation(Handle<WasmSharedModuleData>);

  static Handle<WasmSharedModuleData> New(
      Isolate* isolate, Handle<Foreign> module_wrapper,
      Handle<SeqOneByteString> module_bytes, Handle<Script> script,
      Handle<ByteArray> asm_js_offset_table);

 private:
  DECL_OPTIONAL_ACCESSORS(lazy_compilation_orchestrator, Foreign)
  friend class WasmCompiledModule;
};

// This represents the set of wasm compiled functions, together
// with all the information necessary for re-specializing them.
//
// We specialize wasm functions to their instance by embedding:
//   - raw interior pointers into the backing store of the array buffer
//     used as memory of a particular WebAssembly.Instance object.
//   - bounds check limits, computed at compile time, relative to the
//     size of the memory.
//   - the objects representing the function tables and signature tables
//   - raw pointer to the globals buffer.
//
// Even without instantiating, we need values for all of these parameters.
// We need to track these values to be able to create new instances and
// to be able to serialize/deserialize.
// The design decisions for how we track these values is not too immediate,
// and it deserves a summary. The "tricky" ones are: memory, globals, and
// the tables (signature and functions).
// The first 2 (memory & globals) are embedded as raw pointers to native
// buffers. All we need to track them is the start addresses and, in the
// case of memory, the size. We model all of them as HeapNumbers, because
// we need to store size_t values (for addresses), and potentially full
// 32 bit unsigned values for the size. Smis are 31 bits.
// For tables, we need to hold a reference to the JS Heap object, because
// we embed them as objects, and they may move.
class WasmCompiledModule : public FixedArray {
 public:
  enum {  // --
    kFieldCount
  };

  static WasmCompiledModule* cast(Object* fixed_array) {
    SLOW_DCHECK(IsWasmCompiledModule(fixed_array));
    return reinterpret_cast<WasmCompiledModule*>(fixed_array);
  }

#define WCM_OBJECT_OR_WEAK(TYPE, NAME, ID, TYPE_CHECK, SETTER_MODIFIER) \
 public:                                                                \
  Handle<TYPE> NAME() const { return handle(ptr_to_##NAME()); }         \
                                                                        \
  MaybeHandle<TYPE> maybe_##NAME() const {                              \
    if (has_##NAME()) return NAME();                                    \
    return MaybeHandle<TYPE>();                                         \
  }                                                                     \
                                                                        \
  TYPE* maybe_ptr_to_##NAME() const {                                   \
    Object* obj = get(ID);                                              \
    if (!(TYPE_CHECK)) return nullptr;                                  \
    return TYPE::cast(obj);                                             \
  }                                                                     \
                                                                        \
  TYPE* ptr_to_##NAME() const {                                         \
    Object* obj = get(ID);                                              \
    DCHECK(TYPE_CHECK);                                                 \
    return TYPE::cast(obj);                                             \
  }                                                                     \
                                                                        \
  bool has_##NAME() const {                                             \
    Object* obj = get(ID);                                              \
    return TYPE_CHECK;                                                  \
  }                                                                     \
                                                                        \
  void reset_##NAME() { set_undefined(ID); }                            \
                                                                        \
  SETTER_MODIFIER:                                                      \
  void set_##NAME(Handle<TYPE> value) { set_ptr_to_##NAME(*value); }    \
  void set_ptr_to_##NAME(TYPE* value) { set(ID, value); }

#define WCM_OBJECT(TYPE, NAME) \
  WCM_OBJECT_OR_WEAK(TYPE, NAME, kID_##NAME, obj->Is##TYPE(), public)

#define WCM_CONST_OBJECT(TYPE, NAME) \
  WCM_OBJECT_OR_WEAK(TYPE, NAME, kID_##NAME, obj->Is##TYPE(), private)

#define WCM_WASM_OBJECT(TYPE, NAME) \
  WCM_OBJECT_OR_WEAK(TYPE, NAME, kID_##NAME, TYPE::Is##TYPE(obj), private)

#define WCM_SMALL_CONST_NUMBER(TYPE, NAME)                                     \
 public:                                                                       \
  TYPE NAME() const { return static_cast<TYPE>(Smi::ToInt(get(kID_##NAME))); } \
                                                                               \
 private:                                                                      \
  void set_##NAME(TYPE value) { set(kID_##NAME, Smi::FromInt(value)); }

#define WCM_WEAK_LINK(TYPE, NAME)                                          \
  WCM_OBJECT_OR_WEAK(WeakCell, weak_##NAME, kID_##NAME, obj->IsWeakCell(), \
                     public)                                               \
                                                                           \
 public:                                                                   \
  Handle<TYPE> NAME() const {                                              \
    return handle(TYPE::cast(weak_##NAME()->value()));                     \
  }

#define WCM_LARGE_NUMBER(TYPE, NAME)                                   \
 public:                                                               \
  TYPE NAME() const {                                                  \
    Object* value = get(kID_##NAME);                                   \
    DCHECK(value->IsMutableHeapNumber());                              \
    return static_cast<TYPE>(HeapNumber::cast(value)->value());        \
  }                                                                    \
                                                                       \
  void set_##NAME(TYPE value) {                                        \
    Object* number = get(kID_##NAME);                                  \
    DCHECK(number->IsMutableHeapNumber());                             \
    HeapNumber::cast(number)->set_value(static_cast<double>(value));   \
  }                                                                    \
                                                                       \
  static void recreate_##NAME(Handle<WasmCompiledModule> obj,          \
                              Factory* factory, TYPE init_val) {       \
    Handle<HeapNumber> number = factory->NewHeapNumber(                \
        static_cast<double>(init_val), MutableMode::MUTABLE, TENURED); \
    obj->set(kID_##NAME, *number);                                     \
  }                                                                    \
  bool has_##NAME() const { return get(kID_##NAME)->IsMutableHeapNumber(); }

// Add values here if they are required for creating new instances or
// for deserialization, and if they are serializable.
// By default, instance values go to WasmInstanceObject, however, if
// we embed the generated code with a value, then we track that value here.
#define CORE_WCM_PROPERTY_TABLE(MACRO)                        \
  MACRO(WASM_OBJECT, WasmSharedModuleData, shared)            \
  MACRO(OBJECT, Context, native_context)                      \
  MACRO(SMALL_CONST_NUMBER, uint32_t, num_imported_functions) \
  MACRO(CONST_OBJECT, FixedArray, code_table)                 \
  MACRO(CONST_OBJECT, FixedArray, export_wrappers)            \
  MACRO(OBJECT, FixedArray, weak_exported_functions)          \
  MACRO(OBJECT, FixedArray, function_tables)                  \
  MACRO(OBJECT, FixedArray, signature_tables)                 \
  MACRO(CONST_OBJECT, FixedArray, empty_function_tables)      \
  MACRO(CONST_OBJECT, FixedArray, empty_signature_tables)     \
  MACRO(LARGE_NUMBER, size_t, embedded_mem_start)             \
  MACRO(LARGE_NUMBER, size_t, globals_start)                  \
  MACRO(LARGE_NUMBER, uint32_t, embedded_mem_size)            \
  MACRO(SMALL_CONST_NUMBER, uint32_t, initial_pages)          \
  MACRO(WEAK_LINK, WasmCompiledModule, next_instance)         \
  MACRO(WEAK_LINK, WasmCompiledModule, prev_instance)         \
  MACRO(WEAK_LINK, JSObject, owning_instance)                 \
  MACRO(WEAK_LINK, WasmModuleObject, wasm_module)

#if DEBUG
#define DEBUG_ONLY_TABLE(MACRO) MACRO(SMALL_CONST_NUMBER, uint32_t, instance_id)
#else
#define DEBUG_ONLY_TABLE(IGNORE)

 public:
  uint32_t instance_id() const { return static_cast<uint32_t>(-1); }
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
  static Handle<WasmCompiledModule> New(
      Isolate* isolate, Handle<WasmSharedModuleData> shared,
      Handle<FixedArray> code_table, Handle<FixedArray> export_wrappers,
      const std::vector<wasm::GlobalHandleAddress>& function_tables,
      const std::vector<wasm::GlobalHandleAddress>& signature_tables);

  static Handle<WasmCompiledModule> Clone(Isolate* isolate,
                                          Handle<WasmCompiledModule> module);
  static void Reset(Isolate* isolate, WasmCompiledModule* module);

  Address GetEmbeddedMemStartOrNull() const {
    return has_embedded_mem_start()
               ? reinterpret_cast<Address>(embedded_mem_start())
               : nullptr;
  }

  Address GetGlobalsStartOrNull() const {
    return has_globals_start() ? reinterpret_cast<Address>(globals_start())
                               : nullptr;
  }

  uint32_t GetEmbeddedMemSizeOrZero() const {
    return has_embedded_mem_size() ? embedded_mem_size() : 0;
  }

  uint32_t default_mem_size() const;

  void ResetSpecializationMemInfoIfNeeded();
  static void SetSpecializationMemInfoFrom(
      Factory* factory, Handle<WasmCompiledModule> compiled_module,
      Handle<JSArrayBuffer> buffer);
  static void SetGlobalsStartAddressFrom(
      Factory* factory, Handle<WasmCompiledModule> compiled_module,
      Handle<JSArrayBuffer> buffer);

#define DECLARATION(KIND, TYPE, NAME) WCM_##KIND(TYPE, NAME)
  WCM_PROPERTY_TABLE(DECLARATION)
#undef DECLARATION

 public:
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

  static void ReinitializeAfterDeserialization(Isolate*,
                                               Handle<WasmCompiledModule>);

  // Get the module name, if set. Returns an empty handle otherwise.
  static MaybeHandle<String> GetModuleNameOrNull(
      Isolate* isolate, Handle<WasmCompiledModule> compiled_module);

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
      wasm::WireBytesRef ref);
  static MaybeHandle<String> ExtractUtf8StringFromModuleBytes(
      Isolate* isolate, Handle<SeqOneByteString> module_bytes,
      wasm::WireBytesRef ref);

  // Get a list of all possible breakpoints within a given range of this module.
  bool GetPossibleBreakpoints(const debug::Location& start,
                              const debug::Location& end,
                              std::vector<debug::BreakLocation>* locations);

  // Set a breakpoint on the given byte position inside the given module.
  // This will affect all live and future instances of the module.
  // The passed position might be modified to point to the next breakable
  // location inside the same function.
  // If it points outside a function, or behind the last breakable location,
  // this function returns false and does not set any breakpoint.
  static bool SetBreakPoint(Handle<WasmCompiledModule>, int* position,
                            Handle<Object> break_point_object);

  // Return an empty handle if no breakpoint is hit at that location, or a
  // FixedArray with all hit breakpoint objects.
  MaybeHandle<FixedArray> CheckBreakPoints(int position);

  // Compile lazily the function called in the given caller code object at the
  // given offset.
  // If the called function cannot be determined from the caller (indirect
  // call / exported function), func_index must be set. Otherwise it can be -1.
  // If patch_caller is set, then all direct calls to functions which were
  // already lazily compiled are patched (at least the given call site).
  // Returns the Code to be called at the given call site.
  static Handle<Code> CompileLazy(Isolate*, Handle<WasmInstanceObject>,
                                  Handle<Code> caller, int offset,
                                  int func_index, bool patch_caller);

  void ReplaceCodeTableForTesting(Handle<FixedArray> testing_table) {
    set_code_table(testing_table);
  }

  static void SetTableValue(Isolate* isolate, Handle<FixedArray> table,
                            int index, Address value);
  static void UpdateTableValue(FixedArray* table, int index, Address value);
  static Address GetTableValue(FixedArray* table, int index);

 private:
  void InitId();

  DISALLOW_IMPLICIT_CONSTRUCTORS(WasmCompiledModule);
};

class WasmDebugInfo : public FixedArray {
 public:
  DECL_OOL_QUERY(WasmDebugInfo)
  DECL_OOL_CAST(WasmDebugInfo)

  DECL_GETTER(wasm_instance, WasmInstanceObject)
  DECL_OPTIONAL_ACCESSORS(locals_names, FixedArray)
  DECL_OPTIONAL_ACCESSORS(c_wasm_entries, FixedArray)
  DECL_OPTIONAL_ACCESSORS(c_wasm_entry_map, Managed<wasm::SignatureMap>)

  enum {
    kInstanceIndex,              // instance object.
    kInterpreterHandleIndex,     // managed object containing the interpreter.
    kInterpretedFunctionsIndex,  // array of interpreter entry code objects.
    kLocalsNamesIndex,           // array of array of local names.
    kCWasmEntriesIndex,          // array of C_WASM_ENTRY stubs.
    kCWasmEntryMapIndex,         // maps signature to index into CWasmEntries.
    kFieldCount
  };

  DEF_SIZE(FixedArray)
  DEF_OFFSET(Instance)
  DEF_OFFSET(InterpreterHandle)
  DEF_OFFSET(InterpretedFunctions)
  DEF_OFFSET(LocalsNames)
  DEF_OFFSET(CWasmEntries)
  DEF_OFFSET(CWasmEntryMap)

  static Handle<WasmDebugInfo> New(Handle<WasmInstanceObject>);

  // Setup a WasmDebugInfo with an existing WasmInstance struct.
  // Returns a pointer to the interpreter instantiated inside this
  // WasmDebugInfo.
  // Use for testing only.
  V8_EXPORT_PRIVATE static wasm::WasmInterpreter* SetupForTesting(
      Handle<WasmInstanceObject>);

  // Set a breakpoint in the given function at the given byte offset within that
  // function. This will redirect all future calls to this function to the
  // interpreter and will always pause at the given offset.
  static void SetBreakpoint(Handle<WasmDebugInfo>, int func_index, int offset);

  // Make a set of functions always execute in the interpreter without setting
  // breakpoints.
  static void RedirectToInterpreter(Handle<WasmDebugInfo>,
                                    Vector<int> func_indexes);

  void PrepareStep(StepAction);

  // Execute the specified function in the interpreter. Read arguments from
  // arg_buffer.
  // The frame_pointer will be used to identify the new activation of the
  // interpreter for unwinding and frame inspection.
  // Returns true if exited regularly, false if a trap occurred. In the latter
  // case, a pending exception will have been set on the isolate.
  bool RunInterpreter(Address frame_pointer, int func_index,
                      uint8_t* arg_buffer);

  // Get the stack of the wasm interpreter as pairs of <function index, byte
  // offset>. The list is ordered bottom-to-top, i.e. caller before callee.
  std::vector<std::pair<uint32_t, int>> GetInterpretedStack(
      Address frame_pointer);

  std::unique_ptr<wasm::InterpretedFrame> GetInterpretedFrame(
      Address frame_pointer, int frame_index);

  // Unwind the interpreted stack belonging to the passed interpreter entry
  // frame.
  void Unwind(Address frame_pointer);

  // Returns the number of calls / function frames executed in the interpreter.
  uint64_t NumInterpretedCalls();

  // Update the memory view of the interpreter after executing GrowMemory in
  // compiled code.
  void UpdateMemory(JSArrayBuffer* new_memory);

  // Get scope details for a specific interpreted frame.
  // This returns a JSArray of length two: One entry for the global scope, one
  // for the local scope. Both elements are JSArrays of size
  // ScopeIterator::kScopeDetailsSize and layout as described in debug-scopes.h.
  // The global scope contains information about globals and the memory.
  // The local scope contains information about parameters, locals, and stack
  // values.
  static Handle<JSObject> GetScopeDetails(Handle<WasmDebugInfo>,
                                          Address frame_pointer,
                                          int frame_index);
  static Handle<JSObject> GetGlobalScopeObject(Handle<WasmDebugInfo>,
                                               Address frame_pointer,
                                               int frame_index);
  static Handle<JSObject> GetLocalScopeObject(Handle<WasmDebugInfo>,
                                              Address frame_pointer,
                                              int frame_index);

  static Handle<JSFunction> GetCWasmEntry(Handle<WasmDebugInfo>,
                                          wasm::FunctionSig*);
};

// TODO(titzer): these should be moved to wasm-objects-inl.h
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
ACCESSORS(WasmInstanceObject, compiled_module, WasmCompiledModule,
          kCompiledModuleOffset)
ACCESSORS(WasmInstanceObject, exports_object, JSObject,
          kExportsObjectOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, memory_object, WasmMemoryObject,
                   kMemoryObjectOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, memory_buffer, JSArrayBuffer,
                   kMemoryBufferOffset)
ACCESSORS(WasmInstanceObject, globals_buffer, JSArrayBuffer,
          kGlobalsBufferOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, debug_info, WasmDebugInfo,
                   kDebugInfoOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, function_tables, FixedArray,
                   kFunctionTablesOffset)
OPTIONAL_ACCESSORS(WasmInstanceObject, signature_tables, FixedArray,
                   kSignatureTablesOffset)
ACCESSORS(WasmInstanceObject, directly_called_instances, FixedArray,
          kDirectlyCalledInstancesOffset)

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
#undef DECL_OOL_QUERY
#undef DECL_OOL_CAST
#undef DECL_GETTER
#undef DECL_OPTIONAL_ACCESSORS

#include "src/objects/object-macros-undef.h"

}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_OBJECTS_H_

// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_OBJECTS_H_
#define V8_WASM_OBJECTS_H_

#include "src/base/bits.h"
#include "src/debug/debug.h"
#include "src/debug/interface-types.h"
#include "src/managed.h"
#include "src/objects.h"
#include "src/objects/script.h"
#include "src/wasm/decoder.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"

#include "src/heap/heap.h"

// Has to be the last include (doesn't have include guards)
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {
namespace wasm {
class InterpretedFrame;
class NativeModule;
class WasmCode;
class WasmInterpreter;
struct WasmModule;
class SignatureMap;
typedef Address GlobalHandleAddress;
using ValueType = MachineRepresentation;
using FunctionSig = Signature<ValueType>;
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

// Wasm context used to store the mem_size and mem_start address of the linear
// memory. These variables can be accessed at C++ level at graph build time
// (e.g., initialized during instance building / changed at runtime by
// grow_memory). The address of the WasmContext is provided to the wasm entry
// functions using a RelocatableIntPtrConstant, then the address is passed as
// parameter to the other wasm functions.
// Note that generated code can directly read from instances of this struct.
struct WasmContext {
  byte* mem_start = nullptr;
  uint32_t mem_size = 0;  // TODO(titzer): uintptr_t?
  uint32_t mem_mask = 0;  // TODO(titzer): uintptr_t?
  byte* globals_start = nullptr;

  inline void SetRawMemory(void* mem_start, size_t mem_size) {
    DCHECK_LE(mem_size, std::min(wasm::kV8MaxWasmMemoryPages,
                                 wasm::kSpecMaxWasmMemoryPages) *
                            wasm::WasmModule::kPageSize);
    this->mem_start = static_cast<byte*>(mem_start);
    this->mem_size = static_cast<uint32_t>(mem_size);
    this->mem_mask = base::bits::RoundUpToPowerOfTwo32(this->mem_size) - 1;
    DCHECK_LE(mem_size, this->mem_mask + 1);
  }
};

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

  static void ValidateStateForTesting(Isolate* isolate,
                                      Handle<WasmModuleObject> module);
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

  inline uint32_t current_length();
  inline bool has_maximum_length();
  void Grow(Isolate* isolate, uint32_t count);

  static Handle<WasmTableObject> New(Isolate* isolate, uint32_t initial,
                                     int64_t maximum,
                                     Handle<FixedArray>* js_functions);
  static Handle<FixedArray> AddDispatchTable(
      Isolate* isolate, Handle<WasmTableObject> table,
      Handle<WasmInstanceObject> instance, int table_index,
      Handle<FixedArray> function_table, Handle<FixedArray> signature_table);

  static void Set(Isolate* isolate, Handle<WasmTableObject> table,
                  int32_t index, Handle<JSFunction> function);
};

// Representation of a WebAssembly.Memory JavaScript-level object.
class WasmMemoryObject : public JSObject {
 public:
  DECL_CAST(WasmMemoryObject)

  DECL_ACCESSORS(array_buffer, JSArrayBuffer)
  DECL_INT_ACCESSORS(maximum_pages)
  DECL_OPTIONAL_ACCESSORS(instances, WeakFixedArray)
  DECL_ACCESSORS(wasm_context, Managed<WasmContext>)

  enum {  // --
    kArrayBufferIndex,
    kMaximumPagesIndex,
    kInstancesIndex,
    kWasmContextIndex,
    kFieldCount
  };

  DEF_SIZE(JSObject)
  DEF_OFFSET(ArrayBuffer)
  DEF_OFFSET(MaximumPages)
  DEF_OFFSET(Instances)
  DEF_OFFSET(WasmContext)

  // Add an instance to the internal (weak) list. amortized O(n).
  static void AddInstance(Isolate* isolate, Handle<WasmMemoryObject> memory,
                          Handle<WasmInstanceObject> object);
  // Remove an instance from the internal (weak) list. O(n).
  static void RemoveInstance(Isolate* isolate, Handle<WasmMemoryObject> memory,
                             Handle<WasmInstanceObject> object);
  uint32_t current_pages();
  inline bool has_maximum_pages();

  V8_EXPORT_PRIVATE static Handle<WasmMemoryObject> New(
      Isolate* isolate, MaybeHandle<JSArrayBuffer> buffer, int32_t maximum);

  static int32_t Grow(Isolate*, Handle<WasmMemoryObject>, uint32_t pages);
  static void SetupNewBufferWithSameBackingStore(
      Isolate* isolate, Handle<WasmMemoryObject> memory_object, uint32_t size);
};

// A WebAssembly.Instance JavaScript-level object.
class WasmInstanceObject : public JSObject {
 public:
  DECL_CAST(WasmInstanceObject)

  DECL_ACCESSORS(wasm_context, Managed<WasmContext>)
  DECL_ACCESSORS(compiled_module, WasmCompiledModule)
  DECL_ACCESSORS(exports_object, JSObject)
  DECL_OPTIONAL_ACCESSORS(memory_object, WasmMemoryObject)
  DECL_OPTIONAL_ACCESSORS(globals_buffer, JSArrayBuffer)
  DECL_OPTIONAL_ACCESSORS(debug_info, WasmDebugInfo)
  DECL_OPTIONAL_ACCESSORS(table_object, WasmTableObject)
  DECL_OPTIONAL_ACCESSORS(function_tables, FixedArray)
  DECL_OPTIONAL_ACCESSORS(signature_tables, FixedArray)

  // FixedArray of all instances whose code was imported
  DECL_OPTIONAL_ACCESSORS(directly_called_instances, FixedArray)
  DECL_ACCESSORS(js_imports_table, FixedArray)

  enum {  // --
    kWasmContextIndex,
    kCompiledModuleIndex,
    kExportsObjectIndex,
    kMemoryObjectIndex,
    kGlobalsBufferIndex,
    kDebugInfoIndex,
    kTableObjectIndex,
    kFunctionTablesIndex,
    kSignatureTablesIndex,
    kDirectlyCalledInstancesIndex,
    kJsImportsTableIndex,
    kFieldCount
  };

  DEF_SIZE(JSObject)
  DEF_OFFSET(WasmContext)
  DEF_OFFSET(CompiledModule)
  DEF_OFFSET(ExportsObject)
  DEF_OFFSET(MemoryObject)
  DEF_OFFSET(GlobalsBuffer)
  DEF_OFFSET(DebugInfo)
  DEF_OFFSET(TableObject)
  DEF_OFFSET(FunctionTables)
  DEF_OFFSET(SignatureTables)
  DEF_OFFSET(DirectlyCalledInstances)
  DEF_OFFSET(JsImportsTable)

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

  // Assumed to be called with a code object associated to a wasm module
  // instance. Intended to be called from runtime functions. Returns nullptr on
  // failing to get owning instance.
  static WasmInstanceObject* GetOwningInstance(const wasm::WasmCode* code);
  static WasmInstanceObject* GetOwningInstanceGC(Code* code);

  static void ValidateInstancesChainForTesting(
      Isolate* isolate, Handle<WasmModuleObject> module_obj,
      int instance_count);

  static void ValidateOrphanedInstanceForTesting(
      Isolate* isolate, Handle<WasmInstanceObject> instance);
};

// A WASM function that is wrapped and exported to JavaScript.
class WasmExportedFunction : public JSFunction {
 public:
  WasmInstanceObject* instance();
  V8_EXPORT_PRIVATE int function_index();

  V8_EXPORT_PRIVATE static WasmExportedFunction* cast(Object* object);
  static bool IsWasmExportedFunction(Object* object);

  static Handle<WasmExportedFunction> New(Isolate* isolate,
                                          Handle<WasmInstanceObject> instance,
                                          MaybeHandle<String> maybe_name,
                                          int func_index, int arity,
                                          Handle<Code> export_wrapper);

  WasmCodeWrapper GetWasmCode();
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

  DECL_OPTIONAL_ACCESSORS(lazy_compilation_orchestrator, Foreign)
};

// This represents the set of wasm compiled functions, together
// with all the information necessary for re-specializing them.
//
// We specialize wasm functions to their instance by embedding:
//   - raw pointer to the wasm_context, that contains the size of the
//     memory and the pointer to the backing store of the array buffer
//     used as memory of a particular WebAssembly.Instance object. This
//     information are then used at runtime to access memory / verify bounds
//     check limits.
//   - the objects representing the function tables and signature tables
//
// Even without instantiating, we need values for all of these parameters.
// We need to track these values to be able to create new instances and
// to be able to serialize/deserialize.
// The design decisions for how we track these values is not too immediate,
// and it deserves a summary. The "tricky" ones are: memory, globals, and
// the tables (signature and functions).
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
  inline Handle<TYPE> NAME() const;                                     \
  inline MaybeHandle<TYPE> maybe_##NAME() const;                        \
  inline TYPE* maybe_ptr_to_##NAME() const;                             \
  inline TYPE* ptr_to_##NAME() const;                                   \
  inline bool has_##NAME() const;                                       \
  inline void reset_##NAME();                                           \
                                                                        \
  SETTER_MODIFIER:                                                      \
  inline void set_##NAME(Handle<TYPE> value);                           \
  inline void set_ptr_to_##NAME(TYPE* value);

#define WCM_OBJECT(TYPE, NAME) \
  WCM_OBJECT_OR_WEAK(TYPE, NAME, kID_##NAME, obj->Is##TYPE(), public)

#define WCM_CONST_OBJECT(TYPE, NAME) \
  WCM_OBJECT_OR_WEAK(TYPE, NAME, kID_##NAME, obj->Is##TYPE(), private)

#define WCM_WASM_OBJECT(TYPE, NAME) \
  WCM_OBJECT_OR_WEAK(TYPE, NAME, kID_##NAME, TYPE::Is##TYPE(obj), private)

#define WCM_SMALL_CONST_NUMBER(TYPE, NAME) \
 public:                                   \
  inline TYPE NAME() const;                \
                                           \
 private:                                  \
  inline void set_##NAME(TYPE value);

#define WCM_WEAK_LINK(TYPE, NAME)                                          \
  WCM_OBJECT_OR_WEAK(WeakCell, weak_##NAME, kID_##NAME, obj->IsWeakCell(), \
                     public)                                               \
                                                                           \
 public:                                                                   \
  inline Handle<TYPE> NAME() const;

// Add values here if they are required for creating new instances or
// for deserialization, and if they are serializable.
// By default, instance values go to WasmInstanceObject, however, if
// we embed the generated code with a value, then we track that value here.
#define CORE_WCM_PROPERTY_TABLE(MACRO)                  \
  MACRO(WASM_OBJECT, WasmSharedModuleData, shared)      \
  MACRO(OBJECT, Context, native_context)                \
  MACRO(CONST_OBJECT, FixedArray, export_wrappers)      \
  MACRO(OBJECT, FixedArray, weak_exported_functions)    \
  MACRO(WASM_OBJECT, WasmCompiledModule, next_instance) \
  MACRO(WASM_OBJECT, WasmCompiledModule, prev_instance) \
  MACRO(WEAK_LINK, WasmInstanceObject, owning_instance) \
  MACRO(WEAK_LINK, WasmModuleObject, wasm_module)       \
  MACRO(OBJECT, FixedArray, handler_table)              \
  MACRO(OBJECT, FixedArray, source_positions)           \
  MACRO(OBJECT, Foreign, native_module)                 \
  MACRO(OBJECT, FixedArray, lazy_compile_data)

#define GC_WCM_PROPERTY_TABLE(MACRO)                          \
  MACRO(SMALL_CONST_NUMBER, uint32_t, num_imported_functions) \
  MACRO(CONST_OBJECT, FixedArray, code_table)                 \
  MACRO(OBJECT, FixedArray, function_tables)                  \
  MACRO(OBJECT, FixedArray, signature_tables)                 \
  MACRO(CONST_OBJECT, FixedArray, empty_function_tables)      \
  MACRO(CONST_OBJECT, FixedArray, empty_signature_tables)     \
  MACRO(SMALL_CONST_NUMBER, uint32_t, initial_pages)

// TODO(mtrofin): this is unnecessary when we stop needing
// FLAG_wasm_jit_to_native, because we have instance_id on NativeModule.
#if DEBUG
#define DEBUG_ONLY_TABLE(MACRO) MACRO(SMALL_CONST_NUMBER, uint32_t, instance_id)
#else
#define DEBUG_ONLY_TABLE(IGNORE)

 public:
  uint32_t instance_id() const { return static_cast<uint32_t>(-1); }
#endif

#define WCM_PROPERTY_TABLE(MACRO) \
  CORE_WCM_PROPERTY_TABLE(MACRO)  \
  GC_WCM_PROPERTY_TABLE(MACRO)    \
  DEBUG_ONLY_TABLE(MACRO)

 private:
  enum PropertyIndices {
#define INDICES(IGNORE1, IGNORE2, NAME) kID_##NAME,
    WCM_PROPERTY_TABLE(INDICES) Count
#undef INDICES
  };

 public:
  static Handle<WasmCompiledModule> New(
      Isolate* isolate, wasm::WasmModule* module, Handle<FixedArray> code_table,
      Handle<FixedArray> export_wrappers,
      const std::vector<wasm::GlobalHandleAddress>& function_tables,
      const std::vector<wasm::GlobalHandleAddress>& signature_tables);

  static Handle<WasmCompiledModule> Clone(Isolate* isolate,
                                          Handle<WasmCompiledModule> module);
  static void Reset(Isolate* isolate, WasmCompiledModule* module);

  // TODO(mtrofin): delete this when we don't need FLAG_wasm_jit_to_native
  static void ResetGCModel(Isolate* isolate, WasmCompiledModule* module);

  uint32_t default_mem_size() const;

  wasm::NativeModule* GetNativeModule() const;
  void InsertInChain(WasmModuleObject*);
  void RemoveFromChain();
  void OnWasmModuleDecodingComplete(Handle<WasmSharedModuleData>);

#define DECLARATION(KIND, TYPE, NAME) WCM_##KIND(TYPE, NAME)
  WCM_PROPERTY_TABLE(DECLARATION)
#undef DECLARATION

 public:
// Allow to call method on WasmSharedModuleData also on this object.
#define FORWARD_SHARED(type, name) inline type name();
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

  // Get the source position from a given function index and byte offset,
  // for either asm.js or pure WASM modules.
  static int GetSourcePosition(Handle<WasmCompiledModule> compiled_module,
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

  inline void ReplaceCodeTableForTesting(
      std::vector<wasm::WasmCode*>&& testing_table);

  // TODO(mtrofin): following 4 unnecessary after we're done with
  // FLAG_wasm_jit_to_native
  static void SetTableValue(Isolate* isolate, Handle<FixedArray> table,
                            int index, Address value);
  static void UpdateTableValue(FixedArray* table, int index, Address value);
  static Address GetTableValue(FixedArray* table, int index);
  inline void ReplaceCodeTableForTesting(Handle<FixedArray> testing_table);

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

// Attach function information in the form of deoptimization data to the given
// code object. This information will be used for generating stack traces,
// calling imported functions in the interpreter, knowing which function to
// compile in a lazy compile stub, and more. The deopt data will be a newly
// allocated FixedArray of length 2, where the first element is a WeakCell
// containing the WasmInstanceObject, and the second element is the function
// index.
// If calling this method repeatedly for the same instance, pass a WeakCell
// directly in order to avoid creating many cells pointing to the same instance.
void AttachWasmFunctionInfo(Isolate*, Handle<Code>,
                            MaybeHandle<WeakCell> weak_instance,
                            int func_index);
void AttachWasmFunctionInfo(Isolate*, Handle<Code>,
                            MaybeHandle<WasmInstanceObject>, int func_index);

struct WasmFunctionInfo {
  MaybeHandle<WasmInstanceObject> instance;
  int func_index;
};
WasmFunctionInfo GetWasmFunctionInfo(Isolate*, Handle<Code>);

#undef DECL_OOL_QUERY
#undef DECL_OOL_CAST
#undef DECL_GETTER
#undef DECL_OPTIONAL_ACCESSORS
#undef WCM_CONST_OBJECT
#undef WCM_LARGE_NUMBER
#undef WCM_OBJECT_OR_WEAK
#undef WCM_SMALL_CONST_NUMBER
#undef WCM_WEAK_LINK

#include "src/objects/object-macros-undef.h"

}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_OBJECTS_H_

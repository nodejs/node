// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_MODULE_H_
#define V8_WASM_MODULE_H_

#include <memory>

#include "src/api.h"
#include "src/debug/debug-interface.h"
#include "src/globals.h"
#include "src/handles.h"
#include "src/managed.h"
#include "src/parsing/preparse-data.h"

#include "src/wasm/signature-map.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {

class WasmCompiledModule;
class WasmDebugInfo;
class WasmModuleObject;
class WasmInstanceObject;
class WasmTableObject;
class WasmMemoryObject;

namespace compiler {
class CallDescriptor;
}

namespace wasm {
class ErrorThrower;

enum WasmExternalKind {
  kExternalFunction = 0,
  kExternalTable = 1,
  kExternalMemory = 2,
  kExternalGlobal = 3
};

// Representation of an initializer expression.
struct WasmInitExpr {
  enum WasmInitKind {
    kNone,
    kGlobalIndex,
    kI32Const,
    kI64Const,
    kF32Const,
    kF64Const
  } kind;

  union {
    int32_t i32_const;
    int64_t i64_const;
    float f32_const;
    double f64_const;
    uint32_t global_index;
  } val;

  WasmInitExpr() : kind(kNone) {}
  explicit WasmInitExpr(int32_t v) : kind(kI32Const) { val.i32_const = v; }
  explicit WasmInitExpr(int64_t v) : kind(kI64Const) { val.i64_const = v; }
  explicit WasmInitExpr(float v) : kind(kF32Const) { val.f32_const = v; }
  explicit WasmInitExpr(double v) : kind(kF64Const) { val.f64_const = v; }
  WasmInitExpr(WasmInitKind kind, uint32_t global_index) : kind(kGlobalIndex) {
    val.global_index = global_index;
  }
};

// Reference to a string in the wire bytes.
class WireBytesRef {
 public:
  WireBytesRef() : WireBytesRef(0, 0) {}
  WireBytesRef(uint32_t offset, uint32_t length)
      : offset_(offset), length_(length) {
    DCHECK_IMPLIES(offset_ == 0, length_ == 0);
    DCHECK_LE(offset_, offset_ + length_);  // no uint32_t overflow.
  }

  uint32_t offset() const { return offset_; }
  uint32_t length() const { return length_; }
  uint32_t end_offset() const { return offset_ + length_; }
  bool is_empty() const { return length_ == 0; }
  bool is_set() const { return offset_ != 0; }

 private:
  uint32_t offset_;
  uint32_t length_;
};

// Static representation of a wasm function.
struct WasmFunction {
  FunctionSig* sig;      // signature of the function.
  uint32_t func_index;   // index into the function table.
  uint32_t sig_index;    // index into the signature table.
  WireBytesRef name;     // function name, if any.
  WireBytesRef code;     // code of this function.
  bool imported;
  bool exported;
};

// Static representation of a wasm global variable.
struct WasmGlobal {
  ValueType type;        // type of the global.
  bool mutability;       // {true} if mutable.
  WasmInitExpr init;     // the initialization expression of the global.
  uint32_t offset;       // offset into global memory.
  bool imported;         // true if imported.
  bool exported;         // true if exported.
};

// Note: An exception signature only uses the params portion of a
// function signature.
typedef FunctionSig WasmExceptionSig;

struct WasmException {
  explicit WasmException(const WasmExceptionSig* sig = &empty_sig_)
      : sig(sig) {}

  const WasmExceptionSig* sig;  // type signature of the exception.

 private:
  static const WasmExceptionSig empty_sig_;
};

// Static representation of a wasm data segment.
struct WasmDataSegment {
  WasmInitExpr dest_addr;  // destination memory address of the data.
  WireBytesRef source;     // start offset in the module bytes.
};

// Static representation of a wasm indirect call table.
struct WasmIndirectFunctionTable {
  uint32_t min_size = 0;  // minimum table size.
  uint32_t max_size = 0;  // maximum table size.
  bool has_max = false;   // true if there is a maximum size.
  // TODO(titzer): Move this to WasmInstance. Needed by interpreter only.
  std::vector<int32_t> values;  // function table, -1 indicating invalid.
  bool imported = false;        // true if imported.
  bool exported = false;        // true if exported.
  SignatureMap map;             // canonicalizing map for sig indexes.
};

// Static representation of how to initialize a table.
struct WasmTableInit {
  uint32_t table_index;
  WasmInitExpr offset;
  std::vector<uint32_t> entries;
};

// Static representation of a wasm import.
struct WasmImport {
  WireBytesRef module_name;  // module name.
  WireBytesRef field_name;   // import name.
  WasmExternalKind kind;     // kind of the import.
  uint32_t index;            // index into the respective space.
};

// Static representation of a wasm export.
struct WasmExport {
  WireBytesRef name;      // exported name.
  WasmExternalKind kind;  // kind of the export.
  uint32_t index;         // index into the respective space.
};

enum ModuleOrigin : uint8_t { kWasmOrigin, kAsmJsOrigin };

struct ModuleWireBytes;

// Static representation of a module.
struct V8_EXPORT_PRIVATE WasmModule {
  static const uint32_t kPageSize = 0x10000;    // Page size, 64kb.
  static const uint32_t kMinMemPages = 1;       // Minimum memory size = 64kb

  std::unique_ptr<Zone> signature_zone;
  uint32_t min_mem_pages = 0;     // minimum size of the memory in 64k pages
  uint32_t max_mem_pages = 0;     // maximum size of the memory in 64k pages
  bool has_max_mem = false;       // try if a maximum memory size exists
  bool has_memory = false;        // true if the memory was defined or imported
  bool mem_export = false;        // true if the memory is exported
  int start_function_index = -1;  // start function, >= 0 if any

  std::vector<WasmGlobal> globals;
  uint32_t globals_size = 0;
  uint32_t num_imported_functions = 0;
  uint32_t num_declared_functions = 0;
  uint32_t num_exported_functions = 0;
  WireBytesRef name = {0, 0};
  // TODO(wasm): Add url here, for spec'ed location information.
  std::vector<FunctionSig*> signatures;
  std::vector<WasmFunction> functions;
  std::vector<WasmDataSegment> data_segments;
  std::vector<WasmIndirectFunctionTable> function_tables;
  std::vector<WasmImport> import_table;
  std::vector<WasmExport> export_table;
  std::vector<WasmException> exceptions;
  std::vector<WasmTableInit> table_inits;

  WasmModule() : WasmModule(nullptr) {}
  WasmModule(std::unique_ptr<Zone> owned);

  ModuleOrigin origin() const { return origin_; }
  void set_origin(ModuleOrigin new_value) { origin_ = new_value; }
  bool is_wasm() const { return origin_ == kWasmOrigin; }
  bool is_asm_js() const { return origin_ == kAsmJsOrigin; }

 private:
  // TODO(kschimpf) - Encapsulate more fields.
  ModuleOrigin origin_ = kWasmOrigin;  // origin of the module
};

typedef Managed<WasmModule> WasmModuleWrapper;

// An instantiated wasm module, including memory, function table, etc.
struct WasmInstance {
  const WasmModule* module;  // static representation of the module.
  // -- Heap allocated --------------------------------------------------------
  Handle<Context> context;               // JavaScript native context.
  std::vector<Handle<FixedArray>> function_tables;  // indirect function tables.
  std::vector<Handle<FixedArray>>
      signature_tables;                    // indirect signature tables.
  // TODO(wasm): Remove this vector, since it is only used for testing.
  std::vector<Handle<Code>> function_code;  // code objects for each function.
  // -- raw memory ------------------------------------------------------------
  byte* mem_start = nullptr;  // start of linear memory.
  uint32_t mem_size = 0;      // size of the linear memory.
  // -- raw globals -----------------------------------------------------------
  byte* globals_start = nullptr;  // start of the globals area.

  explicit WasmInstance(const WasmModule* m)
      : module(m),
        function_tables(m->function_tables.size()),
        signature_tables(m->function_tables.size()),
        function_code(m->functions.size()) {}

  void ReopenHandles(Isolate* isolate) {
    context = handle(*context, isolate);

    for (auto& table : function_tables) {
      table = handle(*table, isolate);
    }

    for (auto& table : signature_tables) {
      table = handle(*table, isolate);
    }

    for (auto& code : function_code) {
      code = handle(*code, isolate);
    }
  }
};

// Interface to the storage (wire bytes) of a wasm module.
// It is illegal for anyone receiving a ModuleWireBytes to store pointers based
// on module_bytes, as this storage is only guaranteed to be alive as long as
// this struct is alive.
struct V8_EXPORT_PRIVATE ModuleWireBytes {
  ModuleWireBytes(Vector<const byte> module_bytes)
      : module_bytes_(module_bytes) {}
  ModuleWireBytes(const byte* start, const byte* end)
      : module_bytes_(start, static_cast<int>(end - start)) {
    DCHECK_GE(kMaxInt, end - start);
  }

  // Get a string stored in the module bytes representing a name.
  WasmName GetName(WireBytesRef ref) const {
    if (ref.is_empty()) return {"<?>", 3};  // no name.
    CHECK(BoundsCheck(ref.offset(), ref.length()));
    return Vector<const char>::cast(
        module_bytes_.SubVector(ref.offset(), ref.end_offset()));
  }

  // Get a string stored in the module bytes representing a function name.
  WasmName GetName(const WasmFunction* function) const {
    return GetName(function->name);
  }

  // Get a string stored in the module bytes representing a name.
  WasmName GetNameOrNull(WireBytesRef ref) const {
    if (!ref.is_set()) return {NULL, 0};  // no name.
    CHECK(BoundsCheck(ref.offset(), ref.length()));
    return Vector<const char>::cast(
        module_bytes_.SubVector(ref.offset(), ref.end_offset()));
  }

  // Get a string stored in the module bytes representing a function name.
  WasmName GetNameOrNull(const WasmFunction* function) const {
    return GetNameOrNull(function->name);
  }

  // Checks the given offset range is contained within the module bytes.
  bool BoundsCheck(uint32_t offset, uint32_t length) const {
    uint32_t size = static_cast<uint32_t>(module_bytes_.length());
    return offset <= size && length <= size - offset;
  }

  Vector<const byte> GetFunctionBytes(const WasmFunction* function) const {
    return module_bytes_.SubVector(function->code.offset(),
                                   function->code.end_offset());
  }

  const byte* start() const { return module_bytes_.start(); }
  const byte* end() const { return module_bytes_.end(); }
  size_t length() const { return module_bytes_.length(); }

 private:
  const Vector<const byte> module_bytes_;
};

// Interface provided to the decoder/graph builder which contains only
// minimal information about the globals, functions, and function tables.
struct V8_EXPORT_PRIVATE ModuleEnv {
  ModuleEnv(const WasmModule* module, WasmInstance* instance)
      : module(module),
        instance(instance),
        function_tables(instance ? &instance->function_tables : nullptr),
        signature_tables(instance ? &instance->signature_tables : nullptr) {}
  ModuleEnv(const WasmModule* module,
            std::vector<Handle<FixedArray>>* function_tables,
            std::vector<Handle<FixedArray>>* signature_tables)
      : module(module),
        instance(nullptr),
        function_tables(function_tables),
        signature_tables(signature_tables) {}

  const WasmModule* module;
  WasmInstance* instance;

  std::vector<Handle<FixedArray>>* function_tables;
  std::vector<Handle<FixedArray>>* signature_tables;

  bool IsValidGlobal(uint32_t index) const {
    return module && index < module->globals.size();
  }
  bool IsValidFunction(uint32_t index) const {
    return module && index < module->functions.size();
  }
  bool IsValidSignature(uint32_t index) const {
    return module && index < module->signatures.size();
  }
  bool IsValidTable(uint32_t index) const {
    return module && index < module->function_tables.size();
  }
  ValueType GetGlobalType(uint32_t index) {
    DCHECK(IsValidGlobal(index));
    return module->globals[index].type;
  }
  FunctionSig* GetFunctionSignature(uint32_t index) {
    DCHECK(IsValidFunction(index));
    return module->functions[index].sig;
  }
  FunctionSig* GetSignature(uint32_t index) {
    DCHECK(IsValidSignature(index));
    return module->signatures[index];
  }
  const WasmIndirectFunctionTable* GetTable(uint32_t index) const {
    DCHECK(IsValidTable(index));
    return &module->function_tables[index];
  }

  bool is_asm_js() const { return module->is_asm_js(); }
  bool is_wasm() const { return module->is_wasm(); }

  // Only used for testing.
  Handle<Code> GetFunctionCode(uint32_t index) {
    DCHECK_NOT_NULL(instance);
    return instance->function_code[index];
  }
};

// A ModuleEnv together with ModuleWireBytes.
struct ModuleBytesEnv {
  ModuleBytesEnv(const WasmModule* module, WasmInstance* instance,
                 Vector<const byte> module_bytes)
      : module_env(module, instance), wire_bytes(module_bytes) {}
  ModuleBytesEnv(const WasmModule* module, WasmInstance* instance,
                 const ModuleWireBytes& wire_bytes)
      : module_env(module, instance), wire_bytes(wire_bytes) {}

  ModuleEnv module_env;
  ModuleWireBytes wire_bytes;
};

// A helper for printing out the names of functions.
struct WasmFunctionName {
  WasmFunctionName(const WasmFunction* function, WasmName name)
      : function_(function), name_(name) {}

  const WasmFunction* function_;
  WasmName name_;
};

std::ostream& operator<<(std::ostream& os, const WasmFunctionName& name);

// Get the debug info associated with the given wasm object.
// If no debug info exists yet, it is created automatically.
Handle<WasmDebugInfo> GetDebugInfo(Handle<JSObject> wasm);

// Get the script of the wasm module. If the origin of the module is asm.js, the
// returned Script will be a JavaScript Script of Script::TYPE_NORMAL, otherwise
// it's of type TYPE_WASM.
Handle<Script> GetScript(Handle<JSObject> instance);

V8_EXPORT_PRIVATE MaybeHandle<WasmModuleObject> CreateModuleObjectFromBytes(
    Isolate* isolate, const byte* start, const byte* end, ErrorThrower* thrower,
    ModuleOrigin origin, Handle<Script> asm_js_script,
    Vector<const byte> asm_offset_table);

V8_EXPORT_PRIVATE bool IsWasmCodegenAllowed(Isolate* isolate,
                                            Handle<Context> context);

V8_EXPORT_PRIVATE Handle<JSArray> GetImports(Isolate* isolate,
                                             Handle<WasmModuleObject> module);
V8_EXPORT_PRIVATE Handle<JSArray> GetExports(Isolate* isolate,
                                             Handle<WasmModuleObject> module);
V8_EXPORT_PRIVATE Handle<JSArray> GetCustomSections(
    Isolate* isolate, Handle<WasmModuleObject> module, Handle<String> name,
    ErrorThrower* thrower);

// Decode local variable names from the names section. Return FixedArray of
// FixedArray of <undefined|String>. The outer fixed array is indexed by the
// function index, the inner one by the local index.
Handle<FixedArray> DecodeLocalNames(Isolate*, Handle<WasmCompiledModule>);

// Assumed to be called with a code object associated to a wasm module instance.
// Intended to be called from runtime functions.
// Returns nullptr on failing to get owning instance.
WasmInstanceObject* GetOwningWasmInstance(Code* code);

Handle<JSArrayBuffer> NewArrayBuffer(
    Isolate*, size_t size, bool enable_guard_regions,
    SharedFlag shared = SharedFlag::kNotShared);

Handle<JSArrayBuffer> SetupArrayBuffer(
    Isolate*, void* allocation_base, size_t allocation_length,
    void* backing_store, size_t size, bool is_external,
    bool enable_guard_regions, SharedFlag shared = SharedFlag::kNotShared);

void DetachWebAssemblyMemoryBuffer(Isolate* isolate,
                                   Handle<JSArrayBuffer> buffer,
                                   bool free_memory);

// The returned pointer is owned by the wasm instance target belongs to. The
// result is alive as long as the instance exists.
WasmFunction* GetWasmFunctionForImportWrapper(Isolate* isolate,
                                              Handle<Object> target);

Handle<Code> UnwrapImportWrapper(Handle<Object> import_wrapper);

void TableSet(ErrorThrower* thrower, Isolate* isolate,
              Handle<WasmTableObject> table, int32_t index,
              Handle<JSFunction> function);

void UpdateDispatchTables(Isolate* isolate, Handle<FixedArray> dispatch_tables,
                          int index, WasmFunction* function, Handle<Code> code);

//============================================================================
//== Compilation and instantiation ===========================================
//============================================================================
V8_EXPORT_PRIVATE bool SyncValidate(Isolate* isolate,
                                    const ModuleWireBytes& bytes);

V8_EXPORT_PRIVATE MaybeHandle<WasmModuleObject> SyncCompileTranslatedAsmJs(
    Isolate* isolate, ErrorThrower* thrower, const ModuleWireBytes& bytes,
    Handle<Script> asm_js_script, Vector<const byte> asm_js_offset_table_bytes);

V8_EXPORT_PRIVATE MaybeHandle<WasmModuleObject> SyncCompile(
    Isolate* isolate, ErrorThrower* thrower, const ModuleWireBytes& bytes);

V8_EXPORT_PRIVATE MaybeHandle<WasmInstanceObject> SyncInstantiate(
    Isolate* isolate, ErrorThrower* thrower,
    Handle<WasmModuleObject> module_object, MaybeHandle<JSReceiver> imports,
    MaybeHandle<JSArrayBuffer> memory);

V8_EXPORT_PRIVATE MaybeHandle<WasmInstanceObject> SyncCompileAndInstantiate(
    Isolate* isolate, ErrorThrower* thrower, const ModuleWireBytes& bytes,
    MaybeHandle<JSReceiver> imports, MaybeHandle<JSArrayBuffer> memory);

V8_EXPORT_PRIVATE void AsyncCompile(Isolate* isolate, Handle<JSPromise> promise,
                                    const ModuleWireBytes& bytes);

V8_EXPORT_PRIVATE void AsyncInstantiate(Isolate* isolate,
                                        Handle<JSPromise> promise,
                                        Handle<WasmModuleObject> module_object,
                                        MaybeHandle<JSReceiver> imports);

#if V8_TARGET_ARCH_64_BIT
const bool kGuardRegionsSupported = true;
#else
const bool kGuardRegionsSupported = false;
#endif

inline bool EnableGuardRegions() {
  return FLAG_wasm_guard_pages && kGuardRegionsSupported &&
         !FLAG_experimental_wasm_threads;
}

inline SharedFlag IsShared(Handle<JSArrayBuffer> buffer) {
  if (!buffer.is_null() && buffer->is_shared()) {
    DCHECK(FLAG_experimental_wasm_threads);
    return SharedFlag::kShared;
  }
  return SharedFlag::kNotShared;
}

void UnpackAndRegisterProtectedInstructions(Isolate* isolate,
                                            Handle<FixedArray> code_table);

// Triggered by the WasmCompileLazy builtin.
// Walks the stack (top three frames) to determine the wasm instance involved
// and which function to compile.
// Then triggers WasmCompiledModule::CompileLazy, taking care of correctly
// patching the call site or indirect function tables.
// Returns either the Code object that has been lazily compiled, or Illegal if
// an error occured. In the latter case, a pending exception has been set, which
// will be triggered when returning from the runtime function, i.e. the Illegal
// builtin will never be called.
Handle<Code> CompileLazy(Isolate* isolate);

// This class orchestrates the lazy compilation of wasm functions. It is
// triggered by the WasmCompileLazy builtin.
// It contains the logic for compiling and specializing wasm functions, and
// patching the calling wasm code.
// Once we support concurrent lazy compilation, this class will contain the
// logic to actually orchestrate parallel execution of wasm compilation jobs.
// TODO(clemensh): Implement concurrent lazy compilation.
class LazyCompilationOrchestrator {
  void CompileFunction(Isolate*, Handle<WasmInstanceObject>, int func_index);

 public:
  Handle<Code> CompileLazy(Isolate*, Handle<WasmInstanceObject>,
                           Handle<Code> caller, int call_offset,
                           int exported_func_index, bool patch_caller);
};

const char* ExternalKindName(WasmExternalKind);

namespace testing {
void ValidateInstancesChain(Isolate* isolate,
                            Handle<WasmModuleObject> module_obj,
                            int instance_count);
void ValidateModuleState(Isolate* isolate, Handle<WasmModuleObject> module_obj);
void ValidateOrphanedInstance(Isolate* isolate,
                              Handle<WasmInstanceObject> instance);
}  // namespace testing
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_MODULE_H_

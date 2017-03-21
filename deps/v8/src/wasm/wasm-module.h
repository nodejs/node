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
#include "src/parsing/preparse-data.h"

#include "src/wasm/managed.h"
#include "src/wasm/signature-map.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {

class WasmCompiledModule;
class WasmDebugInfo;
class WasmModuleObject;
class WasmInstanceObject;
class WasmMemoryObject;

namespace compiler {
class CallDescriptor;
class WasmCompilationUnit;
}

namespace wasm {
class ErrorThrower;

const uint32_t kWasmMagic = 0x6d736100;
const uint32_t kWasmVersion = 0x01;

const uint8_t kWasmFunctionTypeForm = 0x60;
const uint8_t kWasmAnyFunctionTypeForm = 0x70;

enum WasmSectionCode {
  kUnknownSectionCode = 0,   // code for unknown sections
  kTypeSectionCode = 1,      // Function signature declarations
  kImportSectionCode = 2,    // Import declarations
  kFunctionSectionCode = 3,  // Function declarations
  kTableSectionCode = 4,     // Indirect function table and other tables
  kMemorySectionCode = 5,    // Memory attributes
  kGlobalSectionCode = 6,    // Global declarations
  kExportSectionCode = 7,    // Exports
  kStartSectionCode = 8,     // Start function declaration
  kElementSectionCode = 9,   // Elements section
  kCodeSectionCode = 10,     // Function code
  kDataSectionCode = 11,     // Data segments
  kNameSectionCode = 12,     // Name section (encoded as a string)
};

inline bool IsValidSectionCode(uint8_t byte) {
  return kTypeSectionCode <= byte && byte <= kDataSectionCode;
}

const char* SectionName(WasmSectionCode code);

// Constants for fixed-size elements within a module.
static const uint8_t kResizableMaximumFlag = 1;
static const int32_t kInvalidFunctionIndex = -1;

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

// Static representation of a WASM function.
struct WasmFunction {
  FunctionSig* sig;      // signature of the function.
  uint32_t func_index;   // index into the function table.
  uint32_t sig_index;    // index into the signature table.
  uint32_t name_offset;  // offset in the module bytes of the name, if any.
  uint32_t name_length;  // length in bytes of the name.
  uint32_t code_start_offset;    // offset in the module bytes of code start.
  uint32_t code_end_offset;      // offset in the module bytes of code end.
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

// Static representation of a wasm data segment.
struct WasmDataSegment {
  WasmInitExpr dest_addr;  // destination memory address of the data.
  uint32_t source_offset;  // start offset in the module bytes.
  uint32_t source_size;    // end offset in the module bytes.
};

// Static representation of a wasm indirect call table.
struct WasmIndirectFunctionTable {
  uint32_t min_size;            // minimum table size.
  uint32_t max_size;            // maximum table size.
  bool has_max;                 // true if there is a maximum size.
  // TODO(titzer): Move this to WasmInstance. Needed by interpreter only.
  std::vector<int32_t> values;  // function table, -1 indicating invalid.
  bool imported;                // true if imported.
  bool exported;                // true if exported.
  SignatureMap map;             // canonicalizing map for sig indexes.
};

// Static representation of how to initialize a table.
struct WasmTableInit {
  uint32_t table_index;
  WasmInitExpr offset;
  std::vector<uint32_t> entries;
};

// Static representation of a WASM import.
struct WasmImport {
  uint32_t module_name_length;  // length in bytes of the module name.
  uint32_t module_name_offset;  // offset in module bytes of the module name.
  uint32_t field_name_length;   // length in bytes of the import name.
  uint32_t field_name_offset;   // offset in module bytes of the import name.
  WasmExternalKind kind;        // kind of the import.
  uint32_t index;               // index into the respective space.
};

// Static representation of a WASM export.
struct WasmExport {
  uint32_t name_length;   // length in bytes of the exported name.
  uint32_t name_offset;   // offset in module bytes of the name to export.
  WasmExternalKind kind;  // kind of the export.
  uint32_t index;         // index into the respective space.
};

enum ModuleOrigin : uint8_t { kWasmOrigin, kAsmJsOrigin };
struct ModuleWireBytes;

// Static representation of a module.
struct V8_EXPORT_PRIVATE WasmModule {
  static const uint32_t kPageSize = 0x10000;    // Page size, 64kb.
  static const uint32_t kMinMemPages = 1;       // Minimum memory size = 64kb

  Zone* owned_zone;
  uint32_t min_mem_pages = 0;  // minimum size of the memory in 64k pages
  uint32_t max_mem_pages = 0;  // maximum size of the memory in 64k pages
  bool has_max_mem = false;    // try if a maximum memory size exists
  bool has_memory = false;     // true if the memory was defined or imported
  bool mem_export = false;     // true if the memory is exported
  // TODO(wasm): reconcile start function index being an int with
  // the fact that we index on uint32_t, so we may technically not be
  // able to represent some start_function_index -es.
  int start_function_index = -1;      // start function, if any
  ModuleOrigin origin = kWasmOrigin;  // origin of the module

  std::vector<WasmGlobal> globals;             // globals in this module.
  uint32_t globals_size = 0;                   // size of globals table.
  uint32_t num_imported_functions = 0;         // number of imported functions.
  uint32_t num_declared_functions = 0;         // number of declared functions.
  uint32_t num_exported_functions = 0;         // number of exported functions.
  std::vector<FunctionSig*> signatures;        // signatures in this module.
  std::vector<WasmFunction> functions;         // functions in this module.
  std::vector<WasmDataSegment> data_segments;  // data segments in this module.
  std::vector<WasmIndirectFunctionTable> function_tables;  // function tables.
  std::vector<WasmImport> import_table;        // import table.
  std::vector<WasmExport> export_table;        // export table.
  std::vector<WasmTableInit> table_inits;      // initializations of tables
  // We store the semaphore here to extend its lifetime. In <libc-2.21, which we
  // use on the try bots, semaphore::Wait() can return while some compilation
  // tasks are still executing semaphore::Signal(). If the semaphore is cleaned
  // up right after semaphore::Wait() returns, then this can cause an
  // invalid-semaphore error in the compilation tasks.
  // TODO(wasm): Move this semaphore back to CompileInParallel when the try bots
  // switch to libc-2.21 or higher.
  std::unique_ptr<base::Semaphore> pending_tasks;

  WasmModule() : WasmModule(nullptr) {}
  WasmModule(Zone* owned_zone);
  ~WasmModule() {
    if (owned_zone) delete owned_zone;
  }

  // Creates a new instantiation of the module in the given isolate.
  static MaybeHandle<WasmInstanceObject> Instantiate(
      Isolate* isolate, ErrorThrower* thrower,
      Handle<WasmModuleObject> wasm_module, Handle<JSReceiver> ffi,
      Handle<JSArrayBuffer> memory = Handle<JSArrayBuffer>::null());

  MaybeHandle<WasmCompiledModule> CompileFunctions(
      Isolate* isolate, Handle<Managed<WasmModule>> module_wrapper,
      ErrorThrower* thrower, const ModuleWireBytes& wire_bytes,
      Handle<Script> asm_js_script,
      Vector<const byte> asm_js_offset_table_bytes) const;
};

typedef Managed<WasmModule> WasmModuleWrapper;

// An instantiated WASM module, including memory, function table, etc.
struct WasmInstance {
  const WasmModule* module;  // static representation of the module.
  // -- Heap allocated --------------------------------------------------------
  Handle<Context> context;               // JavaScript native context.
  std::vector<Handle<FixedArray>> function_tables;  // indirect function tables.
  std::vector<Handle<FixedArray>>
      signature_tables;                    // indirect signature tables.
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
};

// Interface to the storage (wire bytes) of a wasm module.
// It is illegal for anyone receiving a ModuleWireBytes to store pointers based
// on module_bytes, as this storage is only guaranteed to be alive as long as
// this struct is alive.
struct V8_EXPORT_PRIVATE ModuleWireBytes {
  ModuleWireBytes(Vector<const byte> module_bytes)
      : module_bytes(module_bytes) {}
  ModuleWireBytes(const byte* start, const byte* end)
      : module_bytes(start, static_cast<int>(end - start)) {
    DCHECK_GE(kMaxInt, end - start);
  }

  const Vector<const byte> module_bytes;

  // Get a string stored in the module bytes representing a name.
  WasmName GetName(uint32_t offset, uint32_t length) const {
    if (length == 0) return {"<?>", 3};  // no name.
    CHECK(BoundsCheck(offset, length));
    DCHECK_GE(length, 0);
    return Vector<const char>::cast(
        module_bytes.SubVector(offset, offset + length));
  }

  // Get a string stored in the module bytes representing a function name.
  WasmName GetName(const WasmFunction* function) const {
    return GetName(function->name_offset, function->name_length);
  }

  // Get a string stored in the module bytes representing a name.
  WasmName GetNameOrNull(uint32_t offset, uint32_t length) const {
    if (offset == 0 && length == 0) return {NULL, 0};  // no name.
    CHECK(BoundsCheck(offset, length));
    DCHECK_GE(length, 0);
    return Vector<const char>::cast(
        module_bytes.SubVector(offset, offset + length));
  }

  // Get a string stored in the module bytes representing a function name.
  WasmName GetNameOrNull(const WasmFunction* function) const {
    return GetNameOrNull(function->name_offset, function->name_length);
  }

  // Checks the given offset range is contained within the module bytes.
  bool BoundsCheck(uint32_t offset, uint32_t length) const {
    uint32_t size = static_cast<uint32_t>(module_bytes.length());
    return offset <= size && length <= size - offset;
  }
};

// Interface provided to the decoder/graph builder which contains only
// minimal information about the globals, functions, and function tables.
struct V8_EXPORT_PRIVATE ModuleEnv {
  ModuleEnv(const WasmModule* module, WasmInstance* instance)
      : module(module), instance(instance) {}

  const WasmModule* module;
  WasmInstance* instance;

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

  bool asm_js() { return module->origin == kAsmJsOrigin; }

  Handle<Code> GetFunctionCode(uint32_t index) {
    DCHECK_NOT_NULL(instance);
    return instance->function_code[index];
  }

  static compiler::CallDescriptor* GetWasmCallDescriptor(Zone* zone,
                                                         FunctionSig* sig);
  static compiler::CallDescriptor* GetI32WasmCallDescriptor(
      Zone* zone, compiler::CallDescriptor* descriptor);
  static compiler::CallDescriptor* GetI32WasmCallDescriptorForSimd(
      Zone* zone, compiler::CallDescriptor* descriptor);
};

// A ModuleEnv together with ModuleWireBytes.
struct ModuleBytesEnv : public ModuleEnv, public ModuleWireBytes {
  ModuleBytesEnv(const WasmModule* module, WasmInstance* instance,
                 Vector<const byte> module_bytes)
      : ModuleEnv(module, instance), ModuleWireBytes(module_bytes) {}
  ModuleBytesEnv(const WasmModule* module, WasmInstance* instance,
                 const ModuleWireBytes& wire_bytes)
      : ModuleEnv(module, instance), ModuleWireBytes(wire_bytes) {}
};

// A helper for printing out the names of functions.
struct WasmFunctionName {
  WasmFunctionName(const WasmFunction* function, ModuleBytesEnv* module_env)
      : function_(function), name_(module_env->GetNameOrNull(function)) {}

  const WasmFunction* function_;
  WasmName name_;
};

std::ostream& operator<<(std::ostream& os, const WasmModule& module);
std::ostream& operator<<(std::ostream& os, const WasmFunction& function);
std::ostream& operator<<(std::ostream& os, const WasmFunctionName& name);

// Get the debug info associated with the given wasm object.
// If no debug info exists yet, it is created automatically.
Handle<WasmDebugInfo> GetDebugInfo(Handle<JSObject> wasm);

// Check whether the given object represents a WebAssembly.Instance instance.
// This checks the number and type of internal fields, so it's not 100 percent
// secure. If it turns out that we need more complete checks, we could add a
// special marker as internal field, which will definitely never occur anywhere
// else.
bool IsWasmInstance(Object* instance);

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

V8_EXPORT_PRIVATE bool ValidateModuleBytes(Isolate* isolate, const byte* start,
                                           const byte* end,
                                           ErrorThrower* thrower,
                                           ModuleOrigin origin);

// Get the offset of the code of a function within a module.
int GetFunctionCodeOffset(Handle<WasmCompiledModule> compiled_module,
                          int func_index);

// Assumed to be called with a code object associated to a wasm module instance.
// Intended to be called from runtime functions.
// Returns nullptr on failing to get owning instance.
WasmInstanceObject* GetOwningWasmInstance(Code* code);

MaybeHandle<JSArrayBuffer> GetInstanceMemory(
    Isolate* isolate, Handle<WasmInstanceObject> instance);

int32_t GetInstanceMemorySize(Isolate* isolate,
                              Handle<WasmInstanceObject> instance);

int32_t GrowInstanceMemory(Isolate* isolate,
                           Handle<WasmInstanceObject> instance, uint32_t pages);

Handle<JSArrayBuffer> NewArrayBuffer(Isolate* isolate, size_t size,
                                     bool enable_guard_regions);

int32_t GrowWebAssemblyMemory(Isolate* isolate,
                              Handle<WasmMemoryObject> receiver,
                              uint32_t pages);

int32_t GrowMemory(Isolate* isolate, Handle<WasmInstanceObject> instance,
                   uint32_t pages);

void UpdateDispatchTables(Isolate* isolate, Handle<FixedArray> dispatch_tables,
                          int index, Handle<JSFunction> js_function);

void GrowDispatchTables(Isolate* isolate, Handle<FixedArray> dispatch_tables,
                        uint32_t old_size, uint32_t count);

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

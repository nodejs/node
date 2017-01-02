// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_MODULE_H_
#define V8_WASM_MODULE_H_

#include <memory>

#include "src/api.h"
#include "src/handles.h"
#include "src/parsing/preparse-data.h"

#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {

namespace compiler {
class CallDescriptor;
class WasmCompilationUnit;
}

namespace wasm {
const size_t kMaxModuleSize = 1024 * 1024 * 1024;
const size_t kMaxFunctionSize = 128 * 1024;
const size_t kMaxStringSize = 256;
const uint32_t kWasmMagic = 0x6d736100;
const uint32_t kWasmVersion = 0x0b;
const uint8_t kWasmFunctionTypeForm = 0x40;

// WebAssembly sections are named as strings in the binary format, but
// internally V8 uses an enum to handle them.
//
// Entries have the form F(enumerator, string).
#define FOR_EACH_WASM_SECTION_TYPE(F)  \
  F(Signatures, 1, "type")             \
  F(ImportTable, 2, "import")          \
  F(FunctionSignatures, 3, "function") \
  F(FunctionTable, 4, "table")         \
  F(Memory, 5, "memory")               \
  F(ExportTable, 6, "export")          \
  F(StartFunction, 7, "start")         \
  F(FunctionBodies, 8, "code")         \
  F(DataSegments, 9, "data")           \
  F(Names, 10, "name")                 \
  F(Globals, 0, "global")              \
  F(End, 0, "end")

// Contants for the above section types: {LEB128 length, characters...}.
#define WASM_SECTION_MEMORY 6, 'm', 'e', 'm', 'o', 'r', 'y'
#define WASM_SECTION_SIGNATURES 4, 't', 'y', 'p', 'e'
#define WASM_SECTION_GLOBALS 6, 'g', 'l', 'o', 'b', 'a', 'l'
#define WASM_SECTION_DATA_SEGMENTS 4, 'd', 'a', 't', 'a'
#define WASM_SECTION_FUNCTION_TABLE 5, 't', 'a', 'b', 'l', 'e'
#define WASM_SECTION_END 3, 'e', 'n', 'd'
#define WASM_SECTION_START_FUNCTION 5, 's', 't', 'a', 'r', 't'
#define WASM_SECTION_IMPORT_TABLE 6, 'i', 'm', 'p', 'o', 'r', 't'
#define WASM_SECTION_EXPORT_TABLE 6, 'e', 'x', 'p', 'o', 'r', 't'
#define WASM_SECTION_FUNCTION_SIGNATURES \
  8, 'f', 'u', 'n', 'c', 't', 'i', 'o', 'n'
#define WASM_SECTION_FUNCTION_BODIES 4, 'c', 'o', 'd', 'e'
#define WASM_SECTION_NAMES 4, 'n', 'a', 'm', 'e'

// Constants for the above section headers' size (LEB128 + characters).
#define WASM_SECTION_MEMORY_SIZE ((size_t)7)
#define WASM_SECTION_SIGNATURES_SIZE ((size_t)5)
#define WASM_SECTION_GLOBALS_SIZE ((size_t)7)
#define WASM_SECTION_DATA_SEGMENTS_SIZE ((size_t)5)
#define WASM_SECTION_FUNCTION_TABLE_SIZE ((size_t)6)
#define WASM_SECTION_END_SIZE ((size_t)4)
#define WASM_SECTION_START_FUNCTION_SIZE ((size_t)6)
#define WASM_SECTION_IMPORT_TABLE_SIZE ((size_t)7)
#define WASM_SECTION_EXPORT_TABLE_SIZE ((size_t)7)
#define WASM_SECTION_FUNCTION_SIGNATURES_SIZE ((size_t)9)
#define WASM_SECTION_FUNCTION_BODIES_SIZE ((size_t)5)
#define WASM_SECTION_NAMES_SIZE ((size_t)5)

class WasmDebugInfo;

struct WasmSection {
  enum class Code : uint32_t {
#define F(enumerator, order, string) enumerator,
    FOR_EACH_WASM_SECTION_TYPE(F)
#undef F
        Max
  };
  static WasmSection::Code begin();
  static WasmSection::Code end();
  static WasmSection::Code next(WasmSection::Code code);
  static const char* getName(Code code);
  static int getOrder(Code code);
  static size_t getNameLength(Code code);
  static WasmSection::Code lookup(const byte* string, uint32_t length);
};

enum WasmFunctionDeclBit {
  kDeclFunctionName = 0x01,
  kDeclFunctionExport = 0x08
};

// Constants for fixed-size elements within a module.
static const size_t kDeclMemorySize = 3;
static const size_t kDeclDataSegmentSize = 13;

static const uint32_t kMaxReturnCount = 1;

// Static representation of a WASM function.
struct WasmFunction {
  FunctionSig* sig;      // signature of the function.
  uint32_t func_index;   // index into the function table.
  uint32_t sig_index;    // index into the signature table.
  uint32_t name_offset;  // offset in the module bytes of the name, if any.
  uint32_t name_length;  // length in bytes of the name.
  uint32_t code_start_offset;    // offset in the module bytes of code start.
  uint32_t code_end_offset;      // offset in the module bytes of code end.
};

// Static representation of an imported WASM function.
struct WasmImport {
  FunctionSig* sig;               // signature of the function.
  uint32_t sig_index;             // index into the signature table.
  uint32_t module_name_offset;    // offset in module bytes of the module name.
  uint32_t module_name_length;    // length in bytes of the module name.
  uint32_t function_name_offset;  // offset in module bytes of the import name.
  uint32_t function_name_length;  // length in bytes of the import name.
};

// Static representation of an exported WASM function.
struct WasmExport {
  uint32_t func_index;   // index into the function table.
  uint32_t name_offset;  // offset in module bytes of the name to export.
  uint32_t name_length;  // length in bytes of the exported name.
};

// Static representation of a wasm global variable.
struct WasmGlobal {
  uint32_t name_offset;  // offset in the module bytes of the name, if any.
  uint32_t name_length;  // length in bytes of the global name.
  LocalType type;        // type of the global.
  uint32_t offset;       // offset from beginning of globals area.
  bool exported;         // true if this global is exported.
};

// Static representation of a wasm data segment.
struct WasmDataSegment {
  uint32_t dest_addr;      // destination memory address of the data.
  uint32_t source_offset;  // start offset in the module bytes.
  uint32_t source_size;    // end offset in the module bytes.
  bool init;               // true if loaded upon instantiation.
};

// Static representation of a wasm indirect call table.
struct WasmIndirectFunctionTable {
  uint32_t size;                 // initial table size.
  uint32_t max_size;             // maximum table size.
  std::vector<uint16_t> values;  // function table.
};

enum ModuleOrigin { kWasmOrigin, kAsmJsOrigin };

// Static representation of a module.
struct WasmModule {
  static const uint32_t kPageSize = 0x10000;    // Page size, 64kb.
  static const uint32_t kMinMemPages = 1;       // Minimum memory size = 64kb
  static const uint32_t kMaxMemPages = 16384;   // Maximum memory size =  1gb

  const byte* module_start;   // starting address for the module bytes.
  const byte* module_end;     // end address for the module bytes.
  uint32_t min_mem_pages;     // minimum size of the memory in 64k pages.
  uint32_t max_mem_pages;     // maximum size of the memory in 64k pages.
  bool mem_export;            // true if the memory is exported.
  bool mem_external;          // true if the memory is external.
  // TODO(wasm): reconcile start function index being an int with
  // the fact that we index on uint32_t, so we may technically not be
  // able to represent some start_function_index -es.
  int start_function_index;   // start function, if any.
  ModuleOrigin origin;        // origin of the module

  std::vector<WasmGlobal> globals;             // globals in this module.
  uint32_t globals_size;                       // size of globals table.
  std::vector<FunctionSig*> signatures;        // signatures in this module.
  std::vector<WasmFunction> functions;         // functions in this module.
  std::vector<WasmDataSegment> data_segments;  // data segments in this module.
  std::vector<WasmIndirectFunctionTable> function_tables;  // function tables.
  std::vector<WasmImport> import_table;        // import table.
  std::vector<WasmExport> export_table;        // export table.
  // We store the semaphore here to extend its lifetime. In <libc-2.21, which we
  // use on the try bots, semaphore::Wait() can return while some compilation
  // tasks are still executing semaphore::Signal(). If the semaphore is cleaned
  // up right after semaphore::Wait() returns, then this can cause an
  // invalid-semaphore error in the compilation tasks.
  // TODO(wasm): Move this semaphore back to CompileInParallel when the try bots
  // switch to libc-2.21 or higher.
  std::unique_ptr<base::Semaphore> pending_tasks;

  WasmModule() : WasmModule(nullptr) {}
  explicit WasmModule(byte* module_start);

  // Get a string stored in the module bytes representing a name.
  WasmName GetName(uint32_t offset, uint32_t length) const {
    if (length == 0) return {"<?>", 3};  // no name.
    CHECK(BoundsCheck(offset, offset + length));
    DCHECK_GE(static_cast<int>(length), 0);
    return {reinterpret_cast<const char*>(module_start + offset),
            static_cast<int>(length)};
  }

  // Get a string stored in the module bytes representing a function name.
  WasmName GetName(WasmFunction* function) const {
    return GetName(function->name_offset, function->name_length);
  }

  // Get a string stored in the module bytes representing a name.
  WasmName GetNameOrNull(uint32_t offset, uint32_t length) const {
    if (offset == 0 && length == 0) return {NULL, 0};  // no name.
    CHECK(BoundsCheck(offset, offset + length));
    DCHECK_GE(static_cast<int>(length), 0);
    return {reinterpret_cast<const char*>(module_start + offset),
            static_cast<int>(length)};
  }

  // Get a string stored in the module bytes representing a function name.
  WasmName GetNameOrNull(const WasmFunction* function) const {
    return GetNameOrNull(function->name_offset, function->name_length);
  }

  // Checks the given offset range is contained within the module bytes.
  bool BoundsCheck(uint32_t start, uint32_t end) const {
    size_t size = module_end - module_start;
    return start <= size && end <= size;
  }

  // Creates a new instantiation of the module in the given isolate.
  static MaybeHandle<JSObject> Instantiate(Isolate* isolate,
                                           Handle<FixedArray> compiled_module,
                                           Handle<JSReceiver> ffi,
                                           Handle<JSArrayBuffer> memory);

  MaybeHandle<FixedArray> CompileFunctions(Isolate* isolate,
                                           ErrorThrower* thrower) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(WasmModule);
};

// An instantiated WASM module, including memory, function table, etc.
struct WasmModuleInstance {
  const WasmModule* module;  // static representation of the module.
  // -- Heap allocated --------------------------------------------------------
  Handle<JSObject> js_object;            // JavaScript module object.
  Handle<Context> context;               // JavaScript native context.
  Handle<JSArrayBuffer> mem_buffer;      // Handle to array buffer of memory.
  Handle<JSArrayBuffer> globals_buffer;  // Handle to array buffer of globals.
  std::vector<Handle<FixedArray>> function_tables;  // indirect function tables.
  std::vector<Handle<Code>> function_code;  // code objects for each function.
  std::vector<Handle<Code>> import_code;    // code objects for each import.
  // -- raw memory ------------------------------------------------------------
  byte* mem_start;  // start of linear memory.
  uint32_t mem_size;  // size of the linear memory.
  // -- raw globals -----------------------------------------------------------
  byte* globals_start;  // start of the globals area.

  explicit WasmModuleInstance(const WasmModule* m)
      : module(m),
        function_tables(m->function_tables.size()),
        function_code(m->functions.size()),
        import_code(m->import_table.size()),
        mem_start(nullptr),
        mem_size(0),
        globals_start(nullptr) {}
};

// Interface provided to the decoder/graph builder which contains only
// minimal information about the globals, functions, and function tables.
struct ModuleEnv {
  const WasmModule* module;
  WasmModuleInstance* instance;
  ModuleOrigin origin;
  // TODO(mtrofin): remove this once we introduce WASM_DIRECT_CALL
  // reloc infos.
  std::vector<Handle<Code>> placeholders;

  bool IsValidGlobal(uint32_t index) const {
    return module && index < module->globals.size();
  }
  bool IsValidFunction(uint32_t index) const {
    return module && index < module->functions.size();
  }
  bool IsValidSignature(uint32_t index) const {
    return module && index < module->signatures.size();
  }
  bool IsValidImport(uint32_t index) const {
    return module && index < module->import_table.size();
  }
  bool IsValidTable(uint32_t index) const {
    return module && index < module->function_tables.size();
  }
  LocalType GetGlobalType(uint32_t index) {
    DCHECK(IsValidGlobal(index));
    return module->globals[index].type;
  }
  FunctionSig* GetFunctionSignature(uint32_t index) {
    DCHECK(IsValidFunction(index));
    return module->functions[index].sig;
  }
  FunctionSig* GetImportSignature(uint32_t index) {
    DCHECK(IsValidImport(index));
    return module->import_table[index].sig;
  }
  FunctionSig* GetSignature(uint32_t index) {
    DCHECK(IsValidSignature(index));
    return module->signatures[index];
  }
  const WasmIndirectFunctionTable* GetTable(uint32_t index) const {
    DCHECK(IsValidTable(index));
    return &module->function_tables[index];
  }

  bool asm_js() { return origin == kAsmJsOrigin; }

  Handle<Code> GetCodeOrPlaceholder(uint32_t index) const;
  Handle<Code> GetImportCode(uint32_t index);

  static compiler::CallDescriptor* GetWasmCallDescriptor(Zone* zone,
                                                         FunctionSig* sig);
  static compiler::CallDescriptor* GetI32WasmCallDescriptor(
      Zone* zone, compiler::CallDescriptor* descriptor);
  compiler::CallDescriptor* GetCallDescriptor(Zone* zone, uint32_t index);
};

// A helper for printing out the names of functions.
struct WasmFunctionName {
  const WasmFunction* function_;
  const WasmModule* module_;
  WasmFunctionName(const WasmFunction* function, const ModuleEnv* menv)
      : function_(function), module_(menv ? menv->module : nullptr) {}
};

std::ostream& operator<<(std::ostream& os, const WasmModule& module);
std::ostream& operator<<(std::ostream& os, const WasmFunction& function);
std::ostream& operator<<(std::ostream& os, const WasmFunctionName& name);

typedef Result<const WasmModule*> ModuleResult;
typedef Result<WasmFunction*> FunctionResult;
typedef std::vector<std::pair<int, int>> FunctionOffsets;
typedef Result<FunctionOffsets> FunctionOffsetsResult;

// Extract a function name from the given wasm object.
// Returns "<WASM UNNAMED>" if the function is unnamed or the name is not a
// valid UTF-8 string.
Handle<String> GetWasmFunctionName(Isolate* isolate, Handle<Object> wasm,
                                   uint32_t func_index);

// Extract a function name from the given wasm object.
// Returns a null handle if the function is unnamed or the name is not a valid
// UTF-8 string.
Handle<Object> GetWasmFunctionNameOrNull(Isolate* isolate, Handle<Object> wasm,
                                         uint32_t func_index);

// Return the binary source bytes of a wasm module.
SeqOneByteString* GetWasmBytes(JSObject* wasm);

// Get the debug info associated with the given wasm object.
// If no debug info exists yet, it is created automatically.
Handle<WasmDebugInfo> GetDebugInfo(Handle<JSObject> wasm);

// Return the number of functions in the given wasm object.
int GetNumberOfFunctions(JSObject* wasm);

// Create and export JSFunction
Handle<JSFunction> WrapExportCodeAsJSFunction(Isolate* isolate,
                                              Handle<Code> export_code,
                                              Handle<String> name, int arity,
                                              MaybeHandle<ByteArray> signature,
                                              Handle<JSObject> module_instance);

// Check whether the given object is a wasm object.
// This checks the number and type of internal fields, so it's not 100 percent
// secure. If it turns out that we need more complete checks, we could add a
// special marker as internal field, which will definitely never occur anywhere
// else.
bool IsWasmObject(Object* object);

// Update memory references of code objects associated with the module
bool UpdateWasmModuleMemory(Handle<JSObject> object, Address old_start,
                            Address new_start, uint32_t old_size,
                            uint32_t new_size);

// Constructs a single function table as a FixedArray of double size,
// populating it with function signature indices and function indices.
Handle<FixedArray> BuildFunctionTable(Isolate* isolate, uint32_t index,
                                      const WasmModule* module);

// Populates a function table by replacing function indices with handles to
// the compiled code.
void PopulateFunctionTable(Handle<FixedArray> table, uint32_t table_size,
                           const std::vector<Handle<Code>>* code_table);

Handle<JSObject> CreateCompiledModuleObject(Isolate* isolate,
                                            Handle<FixedArray> compiled_module);

namespace testing {

// Decode, verify, and run the function labeled "main" in the
// given encoded module. The module should have no imports.
int32_t CompileAndRunWasmModule(Isolate* isolate, const byte* module_start,
                                const byte* module_end, bool asm_js = false);

int32_t CallFunction(Isolate* isolate, Handle<JSObject> instance,
                     ErrorThrower* thrower, const char* name, int argc,
                     Handle<Object> argv[]);
}  // namespace testing
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_MODULE_H_

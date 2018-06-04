// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_MODULE_H_
#define V8_WASM_WASM_MODULE_H_

#include <memory>

#include "src/debug/debug-interface.h"
#include "src/globals.h"
#include "src/handles.h"
#include "src/managed.h"
#include "src/parsing/preparse-data.h"

#include "src/wasm/decoder.h"
#include "src/wasm/signature-map.h"
#include "src/wasm/wasm-constants.h"

namespace v8 {
namespace internal {

class WasmCompiledModule;
class WasmDebugInfo;
class WasmGlobalObject;
class WasmInstanceObject;
class WasmMemoryObject;
class WasmModuleObject;
class WasmSharedModuleData;
class WasmTableObject;

namespace compiler {
class CallDescriptor;
}

namespace wasm {
class ErrorThrower;
class NativeModule;
class TestingModuleBuilder;

// Static representation of a wasm function.
struct WasmFunction {
  FunctionSig* sig;      // signature of the function.
  uint32_t func_index;   // index into the function table.
  uint32_t sig_index;    // index into the signature table.
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
  FunctionSig* ToFunctionSig() const { return const_cast<FunctionSig*>(sig); }

  const WasmExceptionSig* sig;  // type signature of the exception.

  // Used to hold data on runtime exceptions.
  static constexpr const char* kRuntimeIdStr = "WasmExceptionRuntimeId";
  static constexpr const char* kRuntimeValuesStr = "WasmExceptionValues";

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
  MOVE_ONLY_WITH_DEFAULT_CONSTRUCTORS(WasmIndirectFunctionTable);

  uint32_t initial_size = 0;      // initial table size.
  uint32_t maximum_size = 0;      // maximum table size.
  bool has_maximum_size = false;  // true if there is a maximum size.
  // TODO(titzer): Move this to WasmInstance. Needed by interpreter only.
  std::vector<int32_t> values;  // function table, -1 indicating invalid.
  bool imported = false;        // true if imported.
  bool exported = false;        // true if exported.
};

// Static representation of how to initialize a table.
struct WasmTableInit {
  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(WasmTableInit);

  WasmTableInit(uint32_t table_index, WasmInitExpr offset)
      : table_index(table_index), offset(offset) {}

  uint32_t table_index;
  WasmInitExpr offset;
  std::vector<uint32_t> entries;
};

// Static representation of a wasm import.
struct WasmImport {
  WireBytesRef module_name;  // module name.
  WireBytesRef field_name;   // import name.
  ImportExportKindCode kind;  // kind of the import.
  uint32_t index;            // index into the respective space.
};

// Static representation of a wasm export.
struct WasmExport {
  WireBytesRef name;      // exported name.
  ImportExportKindCode kind;  // kind of the export.
  uint32_t index;         // index into the respective space.
};

enum ModuleOrigin : uint8_t { kWasmOrigin, kAsmJsOrigin };

struct ModuleWireBytes;

// Static representation of a module.
struct V8_EXPORT_PRIVATE WasmModule {
  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(WasmModule);

  std::unique_ptr<Zone> signature_zone;
  uint32_t initial_pages = 0;      // initial size of the memory in 64k pages
  uint32_t maximum_pages = 0;      // maximum size of the memory in 64k pages
  bool has_shared_memory = false;  // true if memory is a SharedArrayBuffer
  bool has_maximum_pages = false;  // true if there is a maximum memory size
  bool has_memory = false;         // true if the memory was defined or imported
  bool mem_export = false;         // true if the memory is exported
  int start_function_index = -1;   // start function, >= 0 if any

  std::vector<WasmGlobal> globals;
  uint32_t globals_size = 0;
  uint32_t num_imported_functions = 0;
  uint32_t num_declared_functions = 0;
  uint32_t num_exported_functions = 0;
  WireBytesRef name = {0, 0};
  // TODO(wasm): Add url here, for spec'ed location information.
  std::vector<FunctionSig*> signatures;  // by signature index
  std::vector<uint32_t> signature_ids;   // by signature index
  std::vector<WasmFunction> functions;
  std::vector<WasmDataSegment> data_segments;
  std::vector<WasmIndirectFunctionTable> function_tables;
  std::vector<WasmImport> import_table;
  std::vector<WasmExport> export_table;
  std::vector<WasmException> exceptions;
  std::vector<WasmTableInit> table_inits;
  SignatureMap signature_map;  // canonicalizing map for signature indexes.

  WasmModule() : WasmModule(nullptr) {}
  WasmModule(std::unique_ptr<Zone> owned);

  ModuleOrigin origin() const { return origin_; }
  void set_origin(ModuleOrigin new_value) { origin_ = new_value; }
  bool is_wasm() const { return origin_ == kWasmOrigin; }
  bool is_asm_js() const { return origin_ == kAsmJsOrigin; }

  WireBytesRef LookupName(const ModuleWireBytes* wire_bytes,
                          uint32_t function_index) const;
  WireBytesRef LookupName(SeqOneByteString* wire_bytes,
                          uint32_t function_index) const;
  void AddNameForTesting(int function_index, WireBytesRef name);

 private:
  // TODO(kschimpf) - Encapsulate more fields.
  ModuleOrigin origin_ = kWasmOrigin;  // origin of the module
  mutable std::unique_ptr<std::unordered_map<uint32_t, WireBytesRef>> names_;
};

typedef Managed<WasmModule> WasmModuleWrapper;

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
  WasmName GetName(WireBytesRef ref) const;

  // Get a string stored in the module bytes representing a function name.
  WasmName GetName(const WasmFunction* function,
                   const WasmModule* module) const;

  // Get a string stored in the module bytes representing a name.
  WasmName GetNameOrNull(WireBytesRef ref) const;

  // Get a string stored in the module bytes representing a function name.
  WasmName GetNameOrNull(const WasmFunction* function,
                         const WasmModule* module) const;

  // Checks the given offset range is contained within the module bytes.
  bool BoundsCheck(uint32_t offset, uint32_t length) const {
    uint32_t size = static_cast<uint32_t>(module_bytes_.length());
    return offset <= size && length <= size - offset;
  }

  Vector<const byte> GetFunctionBytes(const WasmFunction* function) const {
    return module_bytes_.SubVector(function->code.offset(),
                                   function->code.end_offset());
  }

  Vector<const byte> module_bytes() const { return module_bytes_; }
  const byte* start() const { return module_bytes_.start(); }
  const byte* end() const { return module_bytes_.end(); }
  size_t length() const { return module_bytes_.length(); }

 private:
  Vector<const byte> module_bytes_;
};

// A helper for printing out the names of functions.
struct WasmFunctionName {
  WasmFunctionName(const WasmFunction* function, WasmName name)
      : function_(function), name_(name) {}

  const WasmFunction* function_;
  const WasmName name_;
};

std::ostream& operator<<(std::ostream& os, const WasmFunctionName& name);

// Get the debug info associated with the given wasm object.
// If no debug info exists yet, it is created automatically.
Handle<WasmDebugInfo> GetDebugInfo(Handle<JSObject> wasm);

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
Handle<FixedArray> DecodeLocalNames(Isolate*, Handle<WasmSharedModuleData>);

// TruncatedUserString makes it easy to output names up to a certain length, and
// output a truncation followed by '...' if they exceed a limit.
// Use like this:
//   TruncatedUserString<> name (pc, len);
//   printf("... %.*s ...", name.length(), name.start())
template <int kMaxLen = 50>
class TruncatedUserString {
  static_assert(kMaxLen >= 4, "minimum length is 4 (length of '...' plus one)");

 public:
  template <typename T>
  explicit TruncatedUserString(Vector<T> name)
      : TruncatedUserString(name.start(), name.length()) {}

  TruncatedUserString(const byte* start, size_t len)
      : TruncatedUserString(reinterpret_cast<const char*>(start), len) {}

  TruncatedUserString(const char* start, size_t len)
      : start_(start), length_(std::min(kMaxLen, static_cast<int>(len))) {
    if (len > static_cast<size_t>(kMaxLen)) {
      memcpy(buffer_, start, kMaxLen - 3);
      memset(buffer_ + kMaxLen - 3, '.', 3);
      start_ = buffer_;
    }
  }

  const char* start() const { return start_; }

  int length() const { return length_; }

 private:
  const char* start_;
  const int length_;
  char buffer_[kMaxLen];
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_MODULE_H_

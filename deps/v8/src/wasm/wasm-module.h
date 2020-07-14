// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_MODULE_H_
#define V8_WASM_WASM_MODULE_H_

#include <memory>

#include "src/base/optional.h"
#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/utils/vector.h"
#include "src/wasm/signature-map.h"
#include "src/wasm/struct-types.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {

namespace internal {

class WasmModuleObject;

namespace wasm {

using WasmName = Vector<const char>;

struct AsmJsOffsets;
class ErrorThrower;

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
  const FunctionSig* sig;  // signature of the function.
  uint32_t func_index;     // index into the function table.
  uint32_t sig_index;      // index into the signature table.
  WireBytesRef code;       // code of this function.
  bool imported;
  bool exported;
  bool declared;
};

// Static representation of a wasm global variable.
struct WasmGlobal {
  ValueType type;     // type of the global.
  bool mutability;    // {true} if mutable.
  WasmInitExpr init;  // the initialization expression of the global.
  union {
    uint32_t index;   // index of imported mutable global.
    uint32_t offset;  // offset into global memory (if not imported & mutable).
  };
  bool imported;  // true if imported.
  bool exported;  // true if exported.
};

// Note: An exception signature only uses the params portion of a
// function signature.
using WasmExceptionSig = FunctionSig;

// Static representation of a wasm exception type.
struct WasmException {
  explicit WasmException(const WasmExceptionSig* sig) : sig(sig) {}
  const FunctionSig* ToFunctionSig() const { return sig; }

  const WasmExceptionSig* sig;  // type signature of the exception.
};

// Static representation of a wasm data segment.
struct WasmDataSegment {
  // Construct an active segment.
  explicit WasmDataSegment(WasmInitExpr dest_addr)
      : dest_addr(dest_addr), active(true) {}

  // Construct a passive segment, which has no dest_addr.
  WasmDataSegment() : active(false) {}

  WasmInitExpr dest_addr;  // destination memory address of the data.
  WireBytesRef source;     // start offset in the module bytes.
  bool active = true;      // true if copied automatically during instantiation.
};

// Static representation of a wasm indirect call table.
struct WasmTable {
  MOVE_ONLY_WITH_DEFAULT_CONSTRUCTORS(WasmTable);
  ValueType type = kWasmStmt;     // table type.
  uint32_t initial_size = 0;      // initial table size.
  uint32_t maximum_size = 0;      // maximum table size.
  bool has_maximum_size = false;  // true if there is a maximum size.
  bool imported = false;        // true if imported.
  bool exported = false;        // true if exported.
};

// Static representation of wasm element segment (table initializer).
struct WasmElemSegment {
  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(WasmElemSegment);

  // Construct an active segment.
  WasmElemSegment(uint32_t table_index, WasmInitExpr offset)
      : type(kWasmFuncRef),
        table_index(table_index),
        offset(offset),
        status(kStatusActive) {}

  // Construct a passive or declarative segment, which has no table index or
  // offset.
  explicit WasmElemSegment(bool declarative)
      : type(kWasmFuncRef),
        table_index(0),
        status(declarative ? kStatusDeclarative : kStatusPassive) {}

  // Used in the {entries} vector to represent a `ref.null` entry in a passive
  // segment.
  V8_EXPORT_PRIVATE static const uint32_t kNullIndex = ~0u;

  ValueType type;
  uint32_t table_index;
  WasmInitExpr offset;
  std::vector<uint32_t> entries;
  enum Status {
    kStatusActive,      // copied automatically during instantiation.
    kStatusPassive,     // copied explicitly after instantiation.
    kStatusDeclarative  // purely declarative and never copied.
  } status;
};

// Static representation of a wasm import.
struct WasmImport {
  WireBytesRef module_name;   // module name.
  WireBytesRef field_name;    // import name.
  ImportExportKindCode kind;  // kind of the import.
  uint32_t index;             // index into the respective space.
};

// Static representation of a wasm export.
struct WasmExport {
  WireBytesRef name;          // exported name.
  ImportExportKindCode kind;  // kind of the export.
  uint32_t index;             // index into the respective space.
};

enum class WasmCompilationHintStrategy : uint8_t {
  kDefault = 0,
  kLazy = 1,
  kEager = 2,
  kLazyBaselineEagerTopTier = 3,
};

enum class WasmCompilationHintTier : uint8_t {
  kDefault = 0,
  kBaseline = 1,
  kOptimized = 2,
};

// Static representation of a wasm compilation hint
struct WasmCompilationHint {
  WasmCompilationHintStrategy strategy;
  WasmCompilationHintTier baseline_tier;
  WasmCompilationHintTier top_tier;
};

enum ModuleOrigin : uint8_t {
  kWasmOrigin,
  kAsmJsSloppyOrigin,
  kAsmJsStrictOrigin
};

#define SELECT_WASM_COUNTER(counters, origin, prefix, suffix)     \
  ((origin) == kWasmOrigin ? (counters)->prefix##_wasm_##suffix() \
                           : (counters)->prefix##_asm_##suffix())

struct ModuleWireBytes;

class V8_EXPORT_PRIVATE LazilyGeneratedNames {
 public:
  WireBytesRef LookupFunctionName(const ModuleWireBytes& wire_bytes,
                                  uint32_t function_index,
                                  Vector<const WasmExport> export_table) const;

  // For memory and global.
  std::pair<WireBytesRef, WireBytesRef> LookupNameFromImportsAndExports(
      ImportExportKindCode kind, uint32_t index,
      const Vector<const WasmImport> import_table,
      const Vector<const WasmExport> export_table) const;

  void AddForTesting(int function_index, WireBytesRef name);

 private:
  // {function_names_}, {global_names_} and {memory_names_} are
  // populated lazily after decoding, and therefore need a mutex to protect
  // concurrent modifications from multiple {WasmModuleObject}.
  mutable base::Mutex mutex_;
  mutable std::unique_ptr<std::unordered_map<uint32_t, WireBytesRef>>
      function_names_;
  mutable std::unique_ptr<
      std::unordered_map<uint32_t, std::pair<WireBytesRef, WireBytesRef>>>
      global_names_;
  mutable std::unique_ptr<
      std::unordered_map<uint32_t, std::pair<WireBytesRef, WireBytesRef>>>
      memory_names_;
};

class V8_EXPORT_PRIVATE AsmJsOffsetInformation {
 public:
  explicit AsmJsOffsetInformation(Vector<const byte> encoded_offsets);

  // Destructor defined in wasm-module.cc, where the definition of
  // {AsmJsOffsets} is available.
  ~AsmJsOffsetInformation();

  int GetSourcePosition(int func_index, int byte_offset,
                        bool is_at_number_conversion);

  std::pair<int, int> GetFunctionOffsets(int func_index);

 private:
  void EnsureDecodedOffsets();

  // The offset information table is decoded lazily, hence needs to be
  // protected against concurrent accesses.
  // Exactly one of the two fields below will be set at a time.
  mutable base::Mutex mutex_;

  // Holds the encoded offset table bytes.
  OwnedVector<const uint8_t> encoded_offsets_;

  // Holds the decoded offset table.
  std::unique_ptr<AsmJsOffsets> decoded_offsets_;
};

struct TypeDefinition {
  explicit TypeDefinition(const FunctionSig* sig) : function_sig(sig) {}
  explicit TypeDefinition(const StructType* type) : struct_type(type) {}
  explicit TypeDefinition(const ArrayType* type) : array_type(type) {}
  union {
    const FunctionSig* function_sig;
    const StructType* struct_type;
    const ArrayType* array_type;
  };
};

struct V8_EXPORT_PRIVATE WasmDebugSymbols {
  enum class Type { None, SourceMap, EmbeddedDWARF, ExternalDWARF };
  Type type = Type::None;
  WireBytesRef external_url;
};

// Static representation of a module.
struct V8_EXPORT_PRIVATE WasmModule {
  std::unique_ptr<Zone> signature_zone;
  uint32_t initial_pages = 0;      // initial size of the memory in 64k pages
  uint32_t maximum_pages = 0;      // maximum size of the memory in 64k pages
  bool has_shared_memory = false;  // true if memory is a SharedArrayBuffer
  bool has_maximum_pages = false;  // true if there is a maximum memory size
  bool has_memory = false;         // true if the memory was defined or imported
  bool mem_export = false;         // true if the memory is exported
  int start_function_index = -1;   // start function, >= 0 if any

  std::vector<WasmGlobal> globals;
  // Size of the buffer required for all globals that are not imported and
  // mutable.
  uint32_t untagged_globals_buffer_size = 0;
  uint32_t tagged_globals_buffer_size = 0;
  uint32_t num_imported_mutable_globals = 0;
  uint32_t num_imported_functions = 0;
  uint32_t num_imported_tables = 0;
  uint32_t num_declared_functions = 0;  // excluding imported
  uint32_t num_exported_functions = 0;
  uint32_t num_declared_data_segments = 0;  // From the DataCount section.
  WireBytesRef code = {0, 0};
  WireBytesRef name = {0, 0};
  std::vector<TypeDefinition> types;    // by type index
  std::vector<uint8_t> type_kinds;      // by type index
  std::vector<uint32_t> signature_ids;  // by signature index
  void add_signature(const FunctionSig* sig) {
    types.push_back(TypeDefinition(sig));
    type_kinds.push_back(kWasmFunctionTypeCode);
    uint32_t canonical_id = sig ? signature_map.FindOrInsert(*sig) : 0;
    signature_ids.push_back(canonical_id);
  }
  const FunctionSig* signature(uint32_t index) const {
    DCHECK(type_kinds[index] == kWasmFunctionTypeCode);
    return types[index].function_sig;
  }
  bool has_signature(uint32_t index) const {
    return index < types.size() && type_kinds[index] == kWasmFunctionTypeCode;
  }
  void add_struct_type(const StructType* type) {
    types.push_back(TypeDefinition(type));
    type_kinds.push_back(kWasmStructTypeCode);
  }
  const StructType* struct_type(uint32_t index) const {
    DCHECK(type_kinds[index] == kWasmStructTypeCode);
    return types[index].struct_type;
  }
  bool has_struct(uint32_t index) const {
    return index < types.size() && type_kinds[index] == kWasmStructTypeCode;
  }
  void add_array_type(const ArrayType* type) {
    types.push_back(TypeDefinition(type));
    type_kinds.push_back(kWasmArrayTypeCode);
  }
  const ArrayType* array_type(uint32_t index) const {
    DCHECK(type_kinds[index] == kWasmArrayTypeCode);
    return types[index].array_type;
  }
  bool has_array(uint32_t index) const {
    return index < types.size() && type_kinds[index] == kWasmArrayTypeCode;
  }
  std::vector<WasmFunction> functions;
  std::vector<WasmDataSegment> data_segments;
  std::vector<WasmTable> tables;
  std::vector<WasmImport> import_table;
  std::vector<WasmExport> export_table;
  std::vector<WasmException> exceptions;
  std::vector<WasmElemSegment> elem_segments;
  std::vector<WasmCompilationHint> compilation_hints;
  SignatureMap signature_map;  // canonicalizing map for signature indexes.

  ModuleOrigin origin = kWasmOrigin;  // origin of the module
  LazilyGeneratedNames lazily_generated_names;
  WasmDebugSymbols debug_symbols;

  // Asm.js source position information. Only available for modules compiled
  // from asm.js.
  std::unique_ptr<AsmJsOffsetInformation> asm_js_offset_information;

  explicit WasmModule(std::unique_ptr<Zone> signature_zone = nullptr);

  DISALLOW_COPY_AND_ASSIGN(WasmModule);
};

inline bool is_asmjs_module(const WasmModule* module) {
  return module->origin != kWasmOrigin;
}

size_t EstimateStoredSize(const WasmModule* module);

// Returns the number of possible export wrappers for a given module.
V8_EXPORT_PRIVATE int MaxNumExportWrappers(const WasmModule* module);

// Returns the wrapper index for a function in {module} with signature {sig}
// and origin defined by {is_import}.
int GetExportWrapperIndex(const WasmModule* module, const FunctionSig* sig,
                          bool is_import);

// Return the byte offset of the function identified by the given index.
// The offset will be relative to the start of the module bytes.
// Returns -1 if the function index is invalid.
int GetWasmFunctionOffset(const WasmModule* module, uint32_t func_index);

// Returns the function containing the given byte offset.
// Returns -1 if the byte offset is not contained in any
// function of this module.
int GetContainingWasmFunction(const WasmModule* module, uint32_t byte_offset);

// Returns the function containing the given byte offset.
// Will return preceding function if the byte offset is not
// contained within a function.
int GetNearestWasmFunction(const WasmModule* module, uint32_t byte_offset);

// Interface to the storage (wire bytes) of a wasm module.
// It is illegal for anyone receiving a ModuleWireBytes to store pointers based
// on module_bytes, as this storage is only guaranteed to be alive as long as
// this struct is alive.
struct V8_EXPORT_PRIVATE ModuleWireBytes {
  explicit ModuleWireBytes(Vector<const byte> module_bytes)
      : module_bytes_(module_bytes) {}
  ModuleWireBytes(const byte* start, const byte* end)
      : module_bytes_(start, static_cast<int>(end - start)) {
    DCHECK_GE(kMaxInt, end - start);
  }

  // Get a string stored in the module bytes representing a name.
  WasmName GetNameOrNull(WireBytesRef ref) const;

  // Get a string stored in the module bytes representing a function name.
  WasmName GetNameOrNull(const WasmFunction* function,
                         const WasmModule* module) const;

  // Checks the given reference is contained within the module bytes.
  bool BoundsCheck(WireBytesRef ref) const {
    uint32_t size = static_cast<uint32_t>(module_bytes_.length());
    return ref.offset() <= size && ref.length() <= size - ref.offset();
  }

  Vector<const byte> GetFunctionBytes(const WasmFunction* function) const {
    return module_bytes_.SubVector(function->code.offset(),
                                   function->code.end_offset());
  }

  Vector<const byte> module_bytes() const { return module_bytes_; }
  const byte* start() const { return module_bytes_.begin(); }
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

V8_EXPORT_PRIVATE bool IsWasmCodegenAllowed(Isolate* isolate,
                                            Handle<Context> context);

Handle<JSObject> GetTypeForFunction(Isolate* isolate, const FunctionSig* sig);
Handle<JSObject> GetTypeForGlobal(Isolate* isolate, bool is_mutable,
                                  ValueType type);
Handle<JSObject> GetTypeForMemory(Isolate* isolate, uint32_t min_size,
                                  base::Optional<uint32_t> max_size);
Handle<JSObject> GetTypeForTable(Isolate* isolate, ValueType type,
                                 uint32_t min_size,
                                 base::Optional<uint32_t> max_size);
Handle<JSArray> GetImports(Isolate* isolate, Handle<WasmModuleObject> module);
Handle<JSArray> GetExports(Isolate* isolate, Handle<WasmModuleObject> module);
Handle<JSArray> GetCustomSections(Isolate* isolate,
                                  Handle<WasmModuleObject> module,
                                  Handle<String> name, ErrorThrower* thrower);

// Get the source position from a given function index and byte offset,
// for either asm.js or pure Wasm modules.
int GetSourcePosition(const WasmModule*, uint32_t func_index,
                      uint32_t byte_offset, bool is_at_number_conversion);

// Translate function index to the index relative to the first declared (i.e.
// non-imported) function.
inline int declared_function_index(const WasmModule* module, int func_index) {
  DCHECK_LE(module->num_imported_functions, func_index);
  int declared_idx = func_index - module->num_imported_functions;
  DCHECK_GT(module->num_declared_functions, declared_idx);
  return declared_idx;
}

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
      : TruncatedUserString(name.begin(), name.length()) {}

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

// Print the signature into the given {buffer}. If {buffer} is non-empty, it
// will be null-terminated, even if the signature is cut off. Returns the number
// of characters written, excluding the terminating null-byte.
size_t PrintSignature(Vector<char> buffer, const wasm::FunctionSig*);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_MODULE_H_

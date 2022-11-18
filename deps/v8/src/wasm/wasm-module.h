// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_WASM_MODULE_H_
#define V8_WASM_WASM_MODULE_H_

#include <map>
#include <memory>

#include "src/base/optional.h"
#include "src/base/platform/mutex.h"
#include "src/base/vector.h"
#include "src/codegen/signature.h"
#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/wasm/branch-hint-map.h"
#include "src/wasm/constant-expression.h"
#include "src/wasm/struct-types.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-init-expr.h"
#include "src/wasm/wasm-limits.h"

namespace v8::internal {
class WasmModuleObject;
}

namespace v8::internal::wasm {

using WasmName = base::Vector<const char>;

struct AsmJsOffsets;
class ErrorThrower;

// Reference to a string in the wire bytes.
class WireBytesRef {
 public:
  constexpr WireBytesRef() = default;
  constexpr WireBytesRef(uint32_t offset, uint32_t length)
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
  uint32_t offset_ = 0;
  uint32_t length_ = 0;
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
  ValueType type;           // type of the global.
  bool mutability;          // {true} if mutable.
  ConstantExpression init;  // the initialization expression of the global.
  union {
    // Index of imported mutable global.
    uint32_t index;
    // Offset into global memory (if not imported & mutable). Expressed in bytes
    // for value-typed globals, and in tagged words for reference-typed globals.
    uint32_t offset;
  };
  bool imported;  // true if imported.
  bool exported;  // true if exported.
};

// Note: An exception tag signature only uses the params portion of a function
// signature.
using WasmTagSig = FunctionSig;

// Static representation of a wasm tag type.
struct WasmTag {
  explicit WasmTag(const WasmTagSig* sig) : sig(sig) {}
  const FunctionSig* ToFunctionSig() const { return sig; }

  const WasmTagSig* sig;  // type signature of the tag.
};

// Static representation of a wasm literal stringref.
struct WasmStringRefLiteral {
  explicit WasmStringRefLiteral(const WireBytesRef& source) : source(source) {}
  WireBytesRef source;  // start offset in the module bytes.
};

// Static representation of a wasm data segment.
struct WasmDataSegment {
  // Construct an active segment.
  explicit WasmDataSegment(ConstantExpression dest_addr)
      : dest_addr(dest_addr), active(true) {}

  // Construct a passive segment, which has no dest_addr.
  WasmDataSegment() : active(false) {}

  ConstantExpression dest_addr;  // destination memory address of the data.
  WireBytesRef source;           // start offset in the module bytes.
  bool active = true;  // true if copied automatically during instantiation.
};

// Static representation of wasm element segment (table initializer).
struct WasmElemSegment {
  enum Status {
    kStatusActive,      // copied automatically during instantiation.
    kStatusPassive,     // copied explicitly after instantiation.
    kStatusDeclarative  // purely declarative and never copied.
  };
  enum ElementType { kFunctionIndexElements, kExpressionElements };

  // Construct an active segment.
  WasmElemSegment(ValueType type, uint32_t table_index,
                  ConstantExpression offset, ElementType element_type)
      : status(kStatusActive),
        type(type),
        table_index(table_index),
        offset(std::move(offset)),
        element_type(element_type) {}

  // Construct a passive or declarative segment, which has no table index or
  // offset.
  WasmElemSegment(ValueType type, Status status, ElementType element_type)
      : status(status), type(type), table_index(0), element_type(element_type) {
    DCHECK_NE(status, kStatusActive);
  }

  // Default constructor. Constucts an invalid segment.
  WasmElemSegment()
      : status(kStatusActive),
        type(kWasmBottom),
        table_index(0),
        element_type(kFunctionIndexElements) {}

  WasmElemSegment(const WasmElemSegment&) = delete;
  WasmElemSegment(WasmElemSegment&&) V8_NOEXCEPT = default;
  WasmElemSegment& operator=(const WasmElemSegment&) = delete;
  WasmElemSegment& operator=(WasmElemSegment&&) V8_NOEXCEPT = default;

  Status status;
  ValueType type;
  uint32_t table_index;
  ConstantExpression offset;
  ElementType element_type;
  std::vector<ConstantExpression> entries;
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

// Uses a map as backing storage when sparsely, or a vector when densely
// populated. Requires {Value} to implement `bool is_set()` to identify
// uninitialized objects.
template <class Value>
class AdaptiveMap {
 public:
  // The technical limitation here is that index+1 must not overflow. Since
  // we have significantly lower maximums on anything that can be named,
  // we can have a tighter limit here to reject useless entries early.
  static constexpr uint32_t kMaxKey = 10'000'000;
  static_assert(kMaxKey < std::numeric_limits<uint32_t>::max());

  AdaptiveMap() : map_(new MapType()) {}

  explicit AdaptiveMap(const AdaptiveMap&) = delete;
  AdaptiveMap& operator=(const AdaptiveMap&) = delete;

  AdaptiveMap(AdaptiveMap&& other) V8_NOEXCEPT { *this = std::move(other); }

  AdaptiveMap& operator=(AdaptiveMap&& other) V8_NOEXCEPT {
    mode_ = other.mode_;
    vector_.swap(other.vector_);
    map_.swap(other.map_);
    return *this;
  }

  void FinishInitialization();

  bool is_set() const { return mode_ != kInitializing; }

  void Put(uint32_t key, const Value& value) {
    DCHECK(mode_ == kInitializing);
    DCHECK_LE(key, kMaxKey);
    map_->insert(std::make_pair(key, value));
  }

  void Put(uint32_t key, Value&& value) {
    DCHECK(mode_ == kInitializing);
    DCHECK_LE(key, kMaxKey);
    map_->insert(std::make_pair(key, std::move(value)));
  }

  const Value* Get(uint32_t key) const {
    if (mode_ == kDense) {
      if (key >= vector_.size()) return nullptr;
      if (!vector_[key].is_set()) return nullptr;
      return &vector_[key];
    } else {
      DCHECK(mode_ == kSparse || mode_ == kInitializing);
      auto it = map_->find(key);
      if (it == map_->end()) return nullptr;
      return &it->second;
    }
  }

  bool Has(uint32_t key) const {
    if (mode_ == kDense) {
      return key < vector_.size() && vector_[key].is_set();
    } else {
      DCHECK(mode_ == kSparse || mode_ == kInitializing);
      return map_->find(key) != map_->end();
    }
  }

 private:
  static constexpr uint32_t kLoadFactor = 4;
  using MapType = std::map<uint32_t, Value>;
  enum Mode { kDense, kSparse, kInitializing };

  Mode mode_{kInitializing};
  std::vector<Value> vector_;
  std::unique_ptr<MapType> map_;
};
using NameMap = AdaptiveMap<WireBytesRef>;
using IndirectNameMap = AdaptiveMap<AdaptiveMap<WireBytesRef>>;

struct ModuleWireBytes;

class V8_EXPORT_PRIVATE LazilyGeneratedNames {
 public:
  WireBytesRef LookupFunctionName(const ModuleWireBytes& wire_bytes,
                                  uint32_t function_index);

  void AddForTesting(int function_index, WireBytesRef name);
  bool Has(uint32_t function_index);

 private:
  // Lazy loading must guard against concurrent modifications from multiple
  // {WasmModuleObject}s.
  base::Mutex mutex_;
  bool has_functions_{false};
  NameMap function_names_;
};

class V8_EXPORT_PRIVATE AsmJsOffsetInformation {
 public:
  explicit AsmJsOffsetInformation(base::Vector<const byte> encoded_offsets);

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
  base::OwnedVector<const uint8_t> encoded_offsets_;

  // Holds the decoded offset table.
  std::unique_ptr<AsmJsOffsets> decoded_offsets_;
};

// Used as the supertype for a type at the top of the type hierarchy.
constexpr uint32_t kNoSuperType = std::numeric_limits<uint32_t>::max();

struct TypeDefinition {
  enum Kind { kFunction, kStruct, kArray };

  TypeDefinition(const FunctionSig* sig, uint32_t supertype)
      : function_sig(sig), supertype(supertype), kind(kFunction) {}
  TypeDefinition(const StructType* type, uint32_t supertype)
      : struct_type(type), supertype(supertype), kind(kStruct) {}
  TypeDefinition(const ArrayType* type, uint32_t supertype)
      : array_type(type), supertype(supertype), kind(kArray) {}
  TypeDefinition()
      : function_sig(nullptr), supertype(kNoSuperType), kind(kFunction) {}

  union {
    const FunctionSig* function_sig;
    const StructType* struct_type;
    const ArrayType* array_type;
  };

  bool operator==(const TypeDefinition& other) const {
    if (supertype != other.supertype || kind != other.kind) {
      return false;
    }
    switch (kind) {
      case kFunction:
        return *function_sig == *other.function_sig;
      case kStruct:
        return *struct_type == *other.struct_type;
      case kArray:
        return *array_type == *other.array_type;
    }
  }

  bool operator!=(const TypeDefinition& other) const {
    return !(*this == other);
  }

  uint32_t supertype;
  Kind kind;
};

struct V8_EXPORT_PRIVATE WasmDebugSymbols {
  enum class Type { None, SourceMap, EmbeddedDWARF, ExternalDWARF };
  Type type = Type::None;
  WireBytesRef external_url;
};

class CallSiteFeedback {
 public:
  struct PolymorphicCase {
    int function_index;
    int absolute_call_frequency;
  };

  // Regular constructor: uninitialized/unknown, monomorphic, or polymorphic.
  CallSiteFeedback() : index_or_count_(-1), frequency_or_ool_(0) {}
  CallSiteFeedback(int function_index, int call_count)
      : index_or_count_(function_index), frequency_or_ool_(call_count) {}
  CallSiteFeedback(PolymorphicCase* polymorphic_cases, int num_cases)
      : index_or_count_(-num_cases),
        frequency_or_ool_(reinterpret_cast<intptr_t>(polymorphic_cases)) {}

  // Copying and assignment: prefer moving, as it's cheaper.
  // The code below makes sure external polymorphic storage is copied and/or
  // freed as appropriate.
  CallSiteFeedback(const CallSiteFeedback& other) V8_NOEXCEPT { *this = other; }
  CallSiteFeedback(CallSiteFeedback&& other) V8_NOEXCEPT { *this = other; }
  CallSiteFeedback& operator=(const CallSiteFeedback& other) V8_NOEXCEPT {
    index_or_count_ = other.index_or_count_;
    if (other.is_polymorphic()) {
      int num_cases = other.num_cases();
      PolymorphicCase* polymorphic = new PolymorphicCase[num_cases];
      for (int i = 0; i < num_cases; i++) {
        polymorphic[i].function_index = other.function_index(i);
        polymorphic[i].absolute_call_frequency = other.call_count(i);
      }
      frequency_or_ool_ = reinterpret_cast<intptr_t>(polymorphic);
    } else {
      frequency_or_ool_ = other.frequency_or_ool_;
    }
    return *this;
  }
  CallSiteFeedback& operator=(CallSiteFeedback&& other) V8_NOEXCEPT {
    if (this != &other) {
      index_or_count_ = other.index_or_count_;
      frequency_or_ool_ = other.frequency_or_ool_;
      other.frequency_or_ool_ = 0;
    }
    return *this;
  }

  ~CallSiteFeedback() {
    if (is_polymorphic()) delete[] polymorphic_storage();
  }

  int num_cases() const {
    if (is_monomorphic()) return 1;
    if (is_invalid()) return 0;
    return -index_or_count_;
  }
  int function_index(int i) const {
    DCHECK(!is_invalid());
    if (is_monomorphic()) return index_or_count_;
    return polymorphic_storage()[i].function_index;
  }
  int call_count(int i) const {
    if (index_or_count_ >= 0) return static_cast<int>(frequency_or_ool_);
    return polymorphic_storage()[i].absolute_call_frequency;
  }

 private:
  bool is_monomorphic() const { return index_or_count_ >= 0; }
  bool is_polymorphic() const { return index_or_count_ <= -2; }
  bool is_invalid() const { return index_or_count_ == -1; }
  const PolymorphicCase* polymorphic_storage() const {
    DCHECK(is_polymorphic());
    return reinterpret_cast<PolymorphicCase*>(frequency_or_ool_);
  }

  int index_or_count_;
  intptr_t frequency_or_ool_;
};

struct FunctionTypeFeedback {
  // {feedback_vector} is computed from {call_targets} and the instance-specific
  // feedback vector by {TransitiveTypeFeedbackProcessor}.
  std::vector<CallSiteFeedback> feedback_vector;

  // {call_targets} has one entry per "call" and "call_ref" in the function.
  // For "call", it holds the index of the called function, for "call_ref" the
  // value will be {kNonDirectCall}.
  base::OwnedVector<uint32_t> call_targets;

  // {tierup_priority} is updated and used when triggering tier-up.
  // TODO(clemensb): This does not belong here; find a better place.
  int tierup_priority = 0;

  static constexpr uint32_t kNonDirectCall = 0xFFFFFFFF;
};

struct TypeFeedbackStorage {
  std::unordered_map<uint32_t, FunctionTypeFeedback> feedback_for_function;
  // Accesses to {feedback_for_function} are guarded by this mutex.
  mutable base::Mutex mutex;
};

struct WasmTable;

// Static representation of a module.
struct V8_EXPORT_PRIVATE WasmModule {
  std::unique_ptr<Zone> signature_zone;
  uint32_t initial_pages = 0;      // initial size of the memory in 64k pages
  uint32_t maximum_pages = 0;      // maximum size of the memory in 64k pages
  bool has_shared_memory = false;  // true if memory is a SharedArrayBuffer
  bool has_maximum_pages = false;  // true if there is a maximum memory size
  bool is_memory64 = false;        // true if the memory is 64 bit
  bool has_memory = false;         // true if the memory was defined or imported
  bool mem_export = false;         // true if the memory is exported
  int start_function_index = -1;   // start function, >= 0 if any

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
  // Position and size of the code section (payload only, i.e. without section
  // ID and length).
  WireBytesRef code = {0, 0};
  WireBytesRef name = {0, 0};
  // Position and size of the name section (payload only, i.e. without section
  // ID and length).
  WireBytesRef name_section = {0, 0};

  void add_type(TypeDefinition type) {
    types.push_back(type);
    // Isorecursive canonical type will be computed later.
    isorecursive_canonical_type_ids.push_back(kNoSuperType);
  }

  bool has_type(uint32_t index) const { return index < types.size(); }

  void add_signature(const FunctionSig* sig, uint32_t supertype) {
    DCHECK_NOT_NULL(sig);
    add_type(TypeDefinition(sig, supertype));
  }
  bool has_signature(uint32_t index) const {
    return index < types.size() &&
           types[index].kind == TypeDefinition::kFunction;
  }
  const FunctionSig* signature(uint32_t index) const {
    DCHECK(has_signature(index));
    return types[index].function_sig;
  }

  void add_struct_type(const StructType* type, uint32_t supertype) {
    DCHECK_NOT_NULL(type);
    add_type(TypeDefinition(type, supertype));
  }
  bool has_struct(uint32_t index) const {
    return index < types.size() && types[index].kind == TypeDefinition::kStruct;
  }
  const StructType* struct_type(uint32_t index) const {
    DCHECK(has_struct(index));
    return types[index].struct_type;
  }

  void add_array_type(const ArrayType* type, uint32_t supertype) {
    DCHECK_NOT_NULL(type);
    add_type(TypeDefinition(type, supertype));
  }
  bool has_array(uint32_t index) const {
    return index < types.size() && types[index].kind == TypeDefinition::kArray;
  }
  const ArrayType* array_type(uint32_t index) const {
    DCHECK(has_array(index));
    return types[index].array_type;
  }

  uint32_t supertype(uint32_t index) const {
    DCHECK(index < types.size());
    return types[index].supertype;
  }
  bool has_supertype(uint32_t index) const {
    return supertype(index) != kNoSuperType;
  }

  // Linear search. Returns -1 if types are empty.
  int MaxCanonicalTypeIndex() const {
    if (isorecursive_canonical_type_ids.empty()) return -1;
    return *std::max_element(isorecursive_canonical_type_ids.begin(),
                             isorecursive_canonical_type_ids.end());
  }

  std::vector<TypeDefinition> types;  // by type index
  // Maps each type index to its global (cross-module) canonical index as per
  // isorecursive type canonicalization.
  std::vector<uint32_t> isorecursive_canonical_type_ids;
  std::vector<WasmFunction> functions;
  std::vector<WasmGlobal> globals;
  std::vector<WasmDataSegment> data_segments;
  std::vector<WasmTable> tables;
  std::vector<WasmImport> import_table;
  std::vector<WasmExport> export_table;
  std::vector<WasmTag> tags;
  std::vector<WasmStringRefLiteral> stringref_literals;
  std::vector<WasmElemSegment> elem_segments;
  std::vector<WasmCompilationHint> compilation_hints;
  BranchHintInfo branch_hints;
  // Pairs of module offsets and mark id.
  std::vector<std::pair<uint32_t, uint32_t>> inst_traces;
  mutable TypeFeedbackStorage type_feedback;

  ModuleOrigin origin = kWasmOrigin;  // origin of the module
  mutable LazilyGeneratedNames lazily_generated_names;
  WasmDebugSymbols debug_symbols;

  // Asm.js source position information. Only available for modules compiled
  // from asm.js.
  std::unique_ptr<AsmJsOffsetInformation> asm_js_offset_information;

  explicit WasmModule(std::unique_ptr<Zone> signature_zone = nullptr);
  WasmModule(const WasmModule&) = delete;
  WasmModule& operator=(const WasmModule&) = delete;
};

// Static representation of a wasm indirect call table.
struct WasmTable {
  MOVE_ONLY_WITH_DEFAULT_CONSTRUCTORS(WasmTable);

  ValueType type = kWasmVoid;     // table type.
  uint32_t initial_size = 0;      // initial table size.
  uint32_t maximum_size = 0;      // maximum table size.
  bool has_maximum_size = false;  // true if there is a maximum size.
  bool imported = false;          // true if imported.
  bool exported = false;          // true if exported.
  ConstantExpression initial_value;
};

inline bool is_asmjs_module(const WasmModule* module) {
  return module->origin != kWasmOrigin;
}

size_t EstimateStoredSize(const WasmModule* module);

// Returns the wrapper index for a function with isorecursive canonical
// signature index {canonical_sig_index}, and origin defined by {is_import}.
int GetExportWrapperIndex(uint32_t canonical_sig_index, bool is_import);

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

// Gets the explicitly defined subtyping depth for the given type.
// Returns 0 if the type has no explicit supertype.
// The result is capped to {kV8MaxRttSubtypingDepth + 1}.
// Invalid cyclic hierarchies will return -1.
V8_EXPORT_PRIVATE int GetSubtypingDepth(const WasmModule* module,
                                        uint32_t type_index);

// Interface to the storage (wire bytes) of a wasm module.
// It is illegal for anyone receiving a ModuleWireBytes to store pointers based
// on module_bytes, as this storage is only guaranteed to be alive as long as
// this struct is alive.
struct V8_EXPORT_PRIVATE ModuleWireBytes {
  explicit ModuleWireBytes(base::Vector<const byte> module_bytes)
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

  base::Vector<const byte> GetFunctionBytes(
      const WasmFunction* function) const {
    return module_bytes_.SubVector(function->code.offset(),
                                   function->code.end_offset());
  }

  base::Vector<const byte> module_bytes() const { return module_bytes_; }
  const byte* start() const { return module_bytes_.begin(); }
  const byte* end() const { return module_bytes_.end(); }
  size_t length() const { return module_bytes_.length(); }

 private:
  base::Vector<const byte> module_bytes_;
};

// A helper for printing out the names of functions.
struct WasmFunctionName {
  WasmFunctionName(const WasmFunction* function, WasmName name)
      : function_(function), name_(name) {}

  const WasmFunction* function_;
  const WasmName name_;
};

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           const WasmFunctionName& name);

V8_EXPORT_PRIVATE bool IsWasmCodegenAllowed(Isolate* isolate,
                                            Handle<Context> context);
V8_EXPORT_PRIVATE Handle<String> ErrorStringForCodegen(Isolate* isolate,
                                                       Handle<Context> context);

Handle<JSObject> GetTypeForFunction(Isolate* isolate, const FunctionSig* sig,
                                    bool for_exception = false);
Handle<JSObject> GetTypeForGlobal(Isolate* isolate, bool is_mutable,
                                  ValueType type);
Handle<JSObject> GetTypeForMemory(Isolate* isolate, uint32_t min_size,
                                  base::Optional<uint32_t> max_size,
                                  bool shared);
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

// Translate from function index to jump table offset.
int JumpTableOffset(const WasmModule* module, int func_index);

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
  explicit TruncatedUserString(base::Vector<T> name)
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

// Print the signature into the given {buffer}, using {delimiter} as separator
// between parameter types and return types. If {buffer} is non-empty, it will
// be null-terminated, even if the signature is cut off. Returns the number of
// characters written, excluding the terminating null-byte.
size_t PrintSignature(base::Vector<char> buffer, const wasm::FunctionSig*,
                      char delimiter = ':');

V8_EXPORT_PRIVATE size_t
GetWireBytesHash(base::Vector<const uint8_t> wire_bytes);

// Get the required number of feedback slots for a function.
int NumFeedbackSlots(const WasmModule* module, int func_index);

}  // namespace v8::internal::wasm

#endif  // V8_WASM_WASM_MODULE_H_

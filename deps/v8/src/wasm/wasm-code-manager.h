// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_CODE_MANAGER_H_
#define V8_WASM_WASM_CODE_MANAGER_H_

#include <atomic>
#include <list>
#include <map>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/base/address-region.h"
#include "src/base/macros.h"
#include "src/base/optional.h"
#include "src/builtins/builtins-definitions.h"
#include "src/handles/handles.h"
#include "src/trap-handler/trap-handler.h"
#include "src/utils/vector.h"
#include "src/wasm/compilation-environment.h"
#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module-sourcemap.h"
#include "src/wasm/wasm-tier.h"

namespace v8 {
namespace internal {

class Code;
class CodeDesc;
class Isolate;

namespace wasm {

class DebugInfo;
class NativeModule;
class WasmCodeManager;
struct WasmCompilationResult;
class WasmEngine;
class WasmImportWrapperCache;
struct WasmModule;

// Convenience macro listing all wasm runtime stubs. Note that the first few
// elements of the list coincide with {compiler::TrapId}, order matters.
#define WASM_RUNTIME_STUB_LIST(V, VTRAP) \
  FOREACH_WASM_TRAPREASON(VTRAP)         \
  V(WasmCompileLazy)                     \
  V(WasmDebugBreak)                      \
  V(WasmAtomicNotify)                    \
  V(WasmI32AtomicWait32)                 \
  V(WasmI32AtomicWait64)                 \
  V(WasmI64AtomicWait32)                 \
  V(WasmI64AtomicWait64)                 \
  V(WasmMemoryGrow)                      \
  V(WasmTableGet)                        \
  V(WasmTableSet)                        \
  V(WasmStackGuard)                      \
  V(WasmStackOverflow)                   \
  V(WasmThrow)                           \
  V(WasmRethrow)                         \
  V(WasmTraceMemory)                     \
  V(AllocateHeapNumber)                  \
  V(ArgumentsAdaptorTrampoline)          \
  V(BigIntToI32Pair)                     \
  V(BigIntToI64)                         \
  V(DoubleToI)                           \
  V(I32PairToBigInt)                     \
  V(I64ToBigInt)                         \
  V(RecordWrite)                         \
  V(ToNumber)

// Sorted, disjoint and non-overlapping memory regions. A region is of the
// form [start, end). So there's no [start, end), [end, other_end),
// because that should have been reduced to [start, other_end).
class V8_EXPORT_PRIVATE DisjointAllocationPool final {
 public:
  MOVE_ONLY_WITH_DEFAULT_CONSTRUCTORS(DisjointAllocationPool);
  explicit DisjointAllocationPool(base::AddressRegion region)
      : regions_({region}) {}

  // Merge the parameter region into this object while preserving ordering of
  // the regions. The assumption is that the passed parameter is not
  // intersecting this object - for example, it was obtained from a previous
  // Allocate. Returns the merged region.
  base::AddressRegion Merge(base::AddressRegion);

  // Allocate a contiguous region of size {size}. Return an empty pool on
  // failure.
  base::AddressRegion Allocate(size_t size);

  // Allocate a contiguous region of size {size} within {region}. Return an
  // empty pool on failure.
  base::AddressRegion AllocateInRegion(size_t size, base::AddressRegion);

  bool IsEmpty() const { return regions_.empty(); }
  const std::list<base::AddressRegion>& regions() const { return regions_; }

 private:
  std::list<base::AddressRegion> regions_;
};

class V8_EXPORT_PRIVATE WasmCode final {
 public:
  enum Kind {
    kFunction,
    kWasmToCapiWrapper,
    kWasmToJsWrapper,
    kInterpreterEntry,
    kJumpTable
  };

  // Each runtime stub is identified by an id. This id is used to reference the
  // stub via {RelocInfo::WASM_STUB_CALL} and gets resolved during relocation.
  enum RuntimeStubId {
#define DEF_ENUM(Name) k##Name,
#define DEF_ENUM_TRAP(Name) kThrowWasm##Name,
    WASM_RUNTIME_STUB_LIST(DEF_ENUM, DEF_ENUM_TRAP)
#undef DEF_ENUM_TRAP
#undef DEF_ENUM
        kRuntimeStubCount
  };

  Vector<byte> instructions() const { return instructions_; }
  Address instruction_start() const {
    return reinterpret_cast<Address>(instructions_.begin());
  }
  Vector<const byte> reloc_info() const {
    return {protected_instructions_data().end(),
            static_cast<size_t>(reloc_info_size_)};
  }
  Vector<const byte> source_positions() const {
    return {reloc_info().end(), static_cast<size_t>(source_positions_size_)};
  }

  // TODO(clemensb): Make this return int.
  uint32_t index() const {
    DCHECK_LE(0, index_);
    return index_;
  }
  // Anonymous functions are functions that don't carry an index.
  bool IsAnonymous() const { return index_ == kAnonymousFuncIndex; }
  Kind kind() const { return kind_; }
  NativeModule* native_module() const { return native_module_; }
  ExecutionTier tier() const { return tier_; }
  Address constant_pool() const;
  Address handler_table() const;
  int handler_table_size() const;
  Address code_comments() const;
  int code_comments_size() const;
  int constant_pool_offset() const { return constant_pool_offset_; }
  int safepoint_table_offset() const { return safepoint_table_offset_; }
  int handler_table_offset() const { return handler_table_offset_; }
  int code_comments_offset() const { return code_comments_offset_; }
  int unpadded_binary_size() const { return unpadded_binary_size_; }
  int stack_slots() const { return stack_slots_; }
  int tagged_parameter_slots() const { return tagged_parameter_slots_; }
  bool is_liftoff() const { return tier_ == ExecutionTier::kLiftoff; }
  bool contains(Address pc) const {
    return reinterpret_cast<Address>(instructions_.begin()) <= pc &&
           pc < reinterpret_cast<Address>(instructions_.end());
  }

  Vector<const uint8_t> protected_instructions_data() const {
    return {meta_data_.get(),
            static_cast<size_t>(protected_instructions_size_)};
  }

  Vector<const trap_handler::ProtectedInstructionData> protected_instructions()
      const {
    return Vector<const trap_handler::ProtectedInstructionData>::cast(
        protected_instructions_data());
  }

  void Validate() const;
  void Print(const char* name = nullptr) const;
  void MaybePrint(const char* name = nullptr) const;
  void Disassemble(const char* name, std::ostream& os,
                   Address current_pc = kNullAddress) const;

  static bool ShouldBeLogged(Isolate* isolate);
  void LogCode(Isolate* isolate) const;

  ~WasmCode();

  void IncRef() {
    int old_val = ref_count_.fetch_add(1, std::memory_order_acq_rel);
    DCHECK_LE(1, old_val);
    DCHECK_GT(kMaxInt, old_val);
    USE(old_val);
  }

  // Decrement the ref count. Returns whether this code becomes dead and needs
  // to be freed.
  V8_WARN_UNUSED_RESULT bool DecRef() {
    int old_count = ref_count_.load(std::memory_order_acquire);
    while (true) {
      DCHECK_LE(1, old_count);
      if (V8_UNLIKELY(old_count == 1)) return DecRefOnPotentiallyDeadCode();
      if (ref_count_.compare_exchange_weak(old_count, old_count - 1,
                                           std::memory_order_acq_rel)) {
        return false;
      }
    }
  }

  // Decrement the ref count on code that is known to be dead, even though there
  // might still be C++ references. Returns whether this drops the last
  // reference and the code needs to be freed.
  V8_WARN_UNUSED_RESULT bool DecRefOnDeadCode() {
    return ref_count_.fetch_sub(1, std::memory_order_acq_rel) == 1;
  }

  // Decrement the ref count on a set of {WasmCode} objects, potentially
  // belonging to different {NativeModule}s. Dead code will be deleted.
  static void DecrementRefCount(Vector<WasmCode* const>);

  enum FlushICache : bool { kFlushICache = true, kNoFlushICache = false };

 private:
  friend class NativeModule;

  WasmCode(NativeModule* native_module, int index, Vector<byte> instructions,
           int stack_slots, int tagged_parameter_slots,
           int safepoint_table_offset, int handler_table_offset,
           int constant_pool_offset, int code_comments_offset,
           int unpadded_binary_size,
           Vector<const byte> protected_instructions_data,
           Vector<const byte> reloc_info,
           Vector<const byte> source_position_table, Kind kind,
           ExecutionTier tier)
      : instructions_(instructions),
        native_module_(native_module),
        meta_data_(ConcatenateBytes(
            {protected_instructions_data, reloc_info, source_position_table})),
        reloc_info_size_(reloc_info.length()),
        source_positions_size_(source_position_table.length()),
        protected_instructions_size_(protected_instructions_data.length()),
        index_(index),
        kind_(kind),
        constant_pool_offset_(constant_pool_offset),
        stack_slots_(stack_slots),
        tagged_parameter_slots_(tagged_parameter_slots),
        safepoint_table_offset_(safepoint_table_offset),
        handler_table_offset_(handler_table_offset),
        code_comments_offset_(code_comments_offset),
        unpadded_binary_size_(unpadded_binary_size),
        tier_(tier) {
    DCHECK_LE(safepoint_table_offset, unpadded_binary_size);
    DCHECK_LE(handler_table_offset, unpadded_binary_size);
    DCHECK_LE(code_comments_offset, unpadded_binary_size);
    DCHECK_LE(constant_pool_offset, unpadded_binary_size);
  }

  std::unique_ptr<const byte[]> ConcatenateBytes(
      std::initializer_list<Vector<const byte>>);

  // Code objects that have been registered with the global trap handler within
  // this process, will have a {trap_handler_index} associated with them.
  int trap_handler_index() const {
    CHECK(has_trap_handler_index());
    return trap_handler_index_;
  }
  void set_trap_handler_index(int value) {
    CHECK(!has_trap_handler_index());
    trap_handler_index_ = value;
  }
  bool has_trap_handler_index() const { return trap_handler_index_ >= 0; }

  // Register protected instruction information with the trap handler. Sets
  // trap_handler_index.
  void RegisterTrapHandlerData();

  // Slow path for {DecRef}: The code becomes potentially dead.
  // Returns whether this code becomes dead and needs to be freed.
  V8_NOINLINE bool DecRefOnPotentiallyDeadCode();

  Vector<byte> instructions_;
  NativeModule* native_module_ = nullptr;
  // {meta_data_} contains several byte vectors concatenated into one:
  //  - protected instructions data of size {protected_instructions_size_}
  //  - relocation info of size {reloc_info_size_}
  //  - source positions of size {source_positions_size_}
  // Note that the protected instructions come first to ensure alignment.
  std::unique_ptr<const byte[]> meta_data_;
  const int reloc_info_size_;
  const int source_positions_size_;
  const int protected_instructions_size_;
  int index_;
  Kind kind_;
  int constant_pool_offset_ = 0;
  int stack_slots_ = 0;
  // Number of tagged parameters passed to this function via the stack. This
  // value is used by the stack walker (e.g. GC) to find references.
  int tagged_parameter_slots_ = 0;
  // We care about safepoint data for wasm-to-js functions, since there may be
  // stack/register tagged values for large number conversions.
  int safepoint_table_offset_ = 0;
  int handler_table_offset_ = 0;
  int code_comments_offset_ = 0;
  int unpadded_binary_size_ = 0;
  int trap_handler_index_ = -1;
  ExecutionTier tier_;

  // WasmCode is ref counted. Counters are held by:
  //   1) The jump table / code table.
  //   2) {WasmCodeRefScope}s.
  //   3) The set of potentially dead code in the {WasmEngine}.
  // If a decrement of (1) would drop the ref count to 0, that code becomes a
  // candidate for garbage collection. At that point, we add a ref count for (3)
  // *before* decrementing the counter to ensure the code stays alive as long as
  // it's being used. Once the ref count drops to zero (i.e. after being removed
  // from (3) and all (2)), the code object is deleted and the memory for the
  // machine code is freed.
  std::atomic<int> ref_count_{1};

  DISALLOW_COPY_AND_ASSIGN(WasmCode);
};

// Check that {WasmCode} objects are sufficiently small. We create many of them,
// often for rather small functions.
// Increase the limit if needed, but first check if the size increase is
// justified.
STATIC_ASSERT(sizeof(WasmCode) <= 96);

WasmCode::Kind GetCodeKind(const WasmCompilationResult& result);

// Return a textual description of the kind.
const char* GetWasmCodeKindAsString(WasmCode::Kind);

// Manages the code reservations and allocations of a single {NativeModule}.
class WasmCodeAllocator {
 public:
  // {OptionalLock} is passed between {WasmCodeAllocator} and {NativeModule} to
  // indicate that the lock on the {WasmCodeAllocator} is already taken. It's
  // optional to allow to also call methods without holding the lock.
  class OptionalLock {
   public:
    // External users can only instantiate a non-locked {OptionalLock}.
    OptionalLock() = default;
    ~OptionalLock();
    bool is_locked() const { return allocator_ != nullptr; }

   private:
    friend class WasmCodeAllocator;
    // {Lock} is called from the {WasmCodeAllocator} if no locked {OptionalLock}
    // is passed.
    void Lock(WasmCodeAllocator*);

    WasmCodeAllocator* allocator_ = nullptr;
  };

  WasmCodeAllocator(WasmCodeManager*, VirtualMemory code_space,
                    std::shared_ptr<Counters> async_counters);
  ~WasmCodeAllocator();

  // Call before use, after the {NativeModule} is set up completely.
  void Init(NativeModule*);

  size_t committed_code_space() const {
    return committed_code_space_.load(std::memory_order_acquire);
  }
  size_t generated_code_size() const {
    return generated_code_size_.load(std::memory_order_acquire);
  }
  size_t freed_code_size() const {
    return freed_code_size_.load(std::memory_order_acquire);
  }

  // Allocate code space. Returns a valid buffer or fails with OOM (crash).
  Vector<byte> AllocateForCode(NativeModule*, size_t size);

  // Allocate code space within a specific region. Returns a valid buffer or
  // fails with OOM (crash).
  Vector<byte> AllocateForCodeInRegion(NativeModule*, size_t size,
                                       base::AddressRegion,
                                       const WasmCodeAllocator::OptionalLock&);

  // Sets permissions of all owned code space to executable, or read-write (if
  // {executable} is false). Returns true on success.
  V8_EXPORT_PRIVATE bool SetExecutable(bool executable);

  // Free memory pages of all given code objects. Used for wasm code GC.
  void FreeCode(Vector<WasmCode* const>);

  // Retrieve the number of separately reserved code spaces.
  size_t GetNumCodeSpaces() const;

 private:
  // The engine-wide wasm code manager.
  WasmCodeManager* const code_manager_;

  mutable base::Mutex mutex_;

  //////////////////////////////////////////////////////////////////////////////
  // Protected by {mutex_}:

  // Code space that was reserved and is available for allocations (subset of
  // {owned_code_space_}).
  DisjointAllocationPool free_code_space_;
  // Code space that was allocated for code (subset of {owned_code_space_}).
  DisjointAllocationPool allocated_code_space_;
  // Code space that was allocated before but is dead now. Full pages within
  // this region are discarded. It's still a subset of {owned_code_space_}.
  DisjointAllocationPool freed_code_space_;
  std::vector<VirtualMemory> owned_code_space_;

  // End of fields protected by {mutex_}.
  //////////////////////////////////////////////////////////////////////////////

  std::atomic<size_t> committed_code_space_{0};
  std::atomic<size_t> generated_code_size_{0};
  std::atomic<size_t> freed_code_size_{0};

  bool is_executable_ = false;

  std::shared_ptr<Counters> async_counters_;
};

class V8_EXPORT_PRIVATE NativeModule final {
 public:
#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_S390X || V8_TARGET_ARCH_ARM64
  static constexpr bool kNeedsFarJumpsBetweenCodeSpaces = true;
#else
  static constexpr bool kNeedsFarJumpsBetweenCodeSpaces = false;
#endif

  // {AddCode} is thread safe w.r.t. other calls to {AddCode} or methods adding
  // code below, i.e. it can be called concurrently from background threads.
  // The returned code still needs to be published via {PublishCode}.
  std::unique_ptr<WasmCode> AddCode(int index, const CodeDesc& desc,
                                    int stack_slots, int tagged_parameter_slots,
                                    Vector<const byte> protected_instructions,
                                    Vector<const byte> source_position_table,
                                    WasmCode::Kind kind, ExecutionTier tier);

  // {PublishCode} makes the code available to the system by entering it into
  // the code table and patching the jump table. It returns a raw pointer to the
  // given {WasmCode} object.
  WasmCode* PublishCode(std::unique_ptr<WasmCode>);
  // Hold the {allocation_mutex_} when calling {PublishCodeLocked}.
  WasmCode* PublishCodeLocked(std::unique_ptr<WasmCode>);

  WasmCode* AddDeserializedCode(
      int index, Vector<const byte> instructions, int stack_slots,
      int tagged_parameter_slots, int safepoint_table_offset,
      int handler_table_offset, int constant_pool_offset,
      int code_comments_offset, int unpadded_binary_size,
      Vector<const byte> protected_instructions_data,
      Vector<const byte> reloc_info, Vector<const byte> source_position_table,
      WasmCode::Kind kind, ExecutionTier tier);

  // Adds anonymous code for testing purposes.
  WasmCode* AddCodeForTesting(Handle<Code> code);

  // Use {UseLazyStub} to setup lazy compilation per function. It will use the
  // existing {WasmCode::kWasmCompileLazy} runtime stub and populate the jump
  // table with trampolines accordingly.
  void UseLazyStub(uint32_t func_index);

  // Creates a snapshot of the current state of the code table. This is useful
  // to get a consistent view of the table (e.g. used by the serializer).
  std::vector<WasmCode*> SnapshotCodeTable() const;

  WasmCode* GetCode(uint32_t index) const;
  bool HasCode(uint32_t index) const;
  bool HasCodeWithTier(uint32_t index, ExecutionTier tier) const;

  void SetWasmSourceMap(std::unique_ptr<WasmModuleSourceMap> source_map);
  WasmModuleSourceMap* GetWasmSourceMap() const;

  Address jump_table_start() const {
    return main_jump_table_ ? main_jump_table_->instruction_start()
                            : kNullAddress;
  }

  uint32_t GetJumpTableOffset(uint32_t func_index) const;

  // Returns the canonical target to call for the given function (the slot in
  // the first jump table).
  Address GetCallTargetForFunction(uint32_t func_index) const;

  struct JumpTablesRef {
    const Address jump_table_start = kNullAddress;
    const Address far_jump_table_start = kNullAddress;

    bool is_valid() const { return far_jump_table_start != kNullAddress; }
  };

  // Finds the jump tables that should be used for given code region. This
  // information is then passed to {GetNearCallTargetForFunction} and
  // {GetNearRuntimeStubEntry} to avoid the overhead of looking this information
  // up there. Return an empty struct if no suitable jump tables exist.
  JumpTablesRef FindJumpTablesForRegion(base::AddressRegion) const;

  // Similarly to {GetCallTargetForFunction}, but uses the jump table previously
  // looked up via {FindJumpTablesForRegion}.
  Address GetNearCallTargetForFunction(uint32_t func_index,
                                       const JumpTablesRef&) const;

  // Get a runtime stub entry (which is a far jump table slot) in the jump table
  // previously looked up via {FindJumpTablesForRegion}.
  Address GetNearRuntimeStubEntry(WasmCode::RuntimeStubId index,
                                  const JumpTablesRef&) const;

  // Reverse lookup from a given call target (which must be a jump table slot)
  // to a function index.
  uint32_t GetFunctionIndexFromJumpTableSlot(Address slot_address) const;

  bool SetExecutable(bool executable) {
    return code_allocator_.SetExecutable(executable);
  }

  // For cctests, where we build both WasmModule and the runtime objects
  // on the fly, and bypass the instance builder pipeline.
  void ReserveCodeTableForTesting(uint32_t max_functions);

  void LogWasmCodes(Isolate* isolate);

  CompilationState* compilation_state() { return compilation_state_.get(); }

  // Create a {CompilationEnv} object for compilation. The caller has to ensure
  // that the {WasmModule} pointer stays valid while the {CompilationEnv} is
  // being used.
  CompilationEnv CreateCompilationEnv() const;

  uint32_t num_functions() const {
    return module_->num_declared_functions + module_->num_imported_functions;
  }
  uint32_t num_imported_functions() const {
    return module_->num_imported_functions;
  }
  UseTrapHandler use_trap_handler() const { return use_trap_handler_; }
  void set_lazy_compile_frozen(bool frozen) { lazy_compile_frozen_ = frozen; }
  bool lazy_compile_frozen() const { return lazy_compile_frozen_; }
  Vector<const uint8_t> wire_bytes() const { return wire_bytes_->as_vector(); }
  const WasmModule* module() const { return module_.get(); }
  std::shared_ptr<const WasmModule> shared_module() const { return module_; }
  size_t committed_code_space() const {
    return code_allocator_.committed_code_space();
  }
  WasmEngine* engine() const { return engine_; }

  void SetWireBytes(OwnedVector<const uint8_t> wire_bytes);

  WasmCode* Lookup(Address) const;

  WasmImportWrapperCache* import_wrapper_cache() const {
    return import_wrapper_cache_.get();
  }

  ~NativeModule();

  const WasmFeatures& enabled_features() const { return enabled_features_; }

  // Returns the runtime stub id that corresponds to the given address (which
  // must be a far jump table slot). Returns {kRuntimeStubCount} on failure.
  WasmCode::RuntimeStubId GetRuntimeStubId(Address runtime_stub_target) const;

  const char* GetRuntimeStubName(Address runtime_stub_target) const;

  // Sample the current code size of this modules to the given counters.
  enum CodeSamplingTime : int8_t { kAfterBaseline, kAfterTopTier, kSampling };
  void SampleCodeSize(Counters*, CodeSamplingTime) const;

  WasmCode* AddCompiledCode(WasmCompilationResult);
  std::vector<WasmCode*> AddCompiledCode(Vector<WasmCompilationResult>);

  // Allows to check whether a function has been redirected to the interpreter
  // by publishing an entry stub with the {Kind::kInterpreterEntry} code kind.
  bool IsRedirectedToInterpreter(uint32_t func_index);

  // Set {tier_down_} flag. Return previous state.
  bool SetTieredDown();
  bool IsTieredDown();

  // Sets the flag, triggers recompilation of all methods to tier down or up,
  // waits for that to complete.
  void TierDown(Isolate* isolate);
  void TierUp(Isolate* isolate);

  // Free a set of functions of this module. Uncommits whole pages if possible.
  // The given vector must be ordered by the instruction start address, and all
  // {WasmCode} objects must not be used any more.
  // Should only be called via {WasmEngine::FreeDeadCode}, so the engine can do
  // its accounting.
  void FreeCode(Vector<WasmCode* const>);

  // Retrieve the number of separately reserved code spaces for this module.
  size_t GetNumberOfCodeSpacesForTesting() const;

  // Get or create the debug info for this NativeModule.
  DebugInfo* GetDebugInfo();

 private:
  friend class WasmCode;
  friend class WasmCodeAllocator;
  friend class WasmCodeManager;
  friend class NativeModuleModificationScope;

  struct CodeSpaceData {
    base::AddressRegion region;
    WasmCode* jump_table;
    WasmCode* far_jump_table;
  };

  // Private constructor, called via {WasmCodeManager::NewNativeModule()}.
  NativeModule(WasmEngine* engine, const WasmFeatures& enabled_features,
               VirtualMemory code_space,
               std::shared_ptr<const WasmModule> module,
               std::shared_ptr<Counters> async_counters,
               std::shared_ptr<NativeModule>* shared_this);

  std::unique_ptr<WasmCode> AddCodeWithCodeSpace(
      int index, const CodeDesc& desc, int stack_slots,
      int tagged_parameter_slots,
      Vector<const byte> protected_instructions_data,
      Vector<const byte> source_position_table, WasmCode::Kind kind,
      ExecutionTier tier, Vector<uint8_t> code_space,
      const JumpTablesRef& jump_tables_ref);

  WasmCode* CreateEmptyJumpTableInRegion(
      int jump_table_size, base::AddressRegion,
      const WasmCodeAllocator::OptionalLock&);

  // Hold the {allocation_mutex_} when calling one of these methods.
  // {slot_index} is the index in the declared functions, i.e. function index
  // minus the number of imported functions.
  void PatchJumpTablesLocked(uint32_t slot_index, Address target);
  void PatchJumpTableLocked(const CodeSpaceData&, uint32_t slot_index,
                            Address target);

  // Called by the {WasmCodeAllocator} to register a new code space.
  void AddCodeSpace(base::AddressRegion,
                    const WasmCodeAllocator::OptionalLock&);

  // Hold the {allocation_mutex_} when calling this method.
  bool has_interpreter_redirection(uint32_t func_index) {
    DCHECK_LT(func_index, num_functions());
    DCHECK_LE(module_->num_imported_functions, func_index);
    if (!interpreter_redirections_) return false;
    uint32_t bitset_idx = declared_function_index(module(), func_index);
    uint8_t byte = interpreter_redirections_[bitset_idx / kBitsPerByte];
    return byte & (1 << (bitset_idx % kBitsPerByte));
  }

  // Hold the {allocation_mutex_} when calling this method.
  void SetInterpreterRedirection(uint32_t func_index) {
    DCHECK_LT(func_index, num_functions());
    DCHECK_LE(module_->num_imported_functions, func_index);
    if (!interpreter_redirections_) {
      interpreter_redirections_.reset(
          new uint8_t[RoundUp<kBitsPerByte>(module_->num_declared_functions) /
                      kBitsPerByte]{});
    }
    uint32_t bitset_idx = declared_function_index(module(), func_index);
    uint8_t& byte = interpreter_redirections_[bitset_idx / kBitsPerByte];
    byte |= 1 << (bitset_idx % kBitsPerByte);
  }

  // {WasmCodeAllocator} manages all code reservations and allocations for this
  // {NativeModule}.
  WasmCodeAllocator code_allocator_;

  // Features enabled for this module. We keep a copy of the features that
  // were enabled at the time of the creation of this native module,
  // to be consistent across asynchronous compilations later.
  const WasmFeatures enabled_features_;

  // The decoded module, stored in a shared_ptr such that background compile
  // tasks can keep this alive.
  std::shared_ptr<const WasmModule> module_;

  std::unique_ptr<WasmModuleSourceMap> source_map_;

  // Wire bytes, held in a shared_ptr so they can be kept alive by the
  // {WireBytesStorage}, held by background compile tasks.
  std::shared_ptr<OwnedVector<const uint8_t>> wire_bytes_;

  // Jump table used by external calls (from JS). Wasm calls use one of the jump
  // tables stored in {code_space_data_}.
  WasmCode* main_jump_table_ = nullptr;

  // Lazy compile stub table, containing entries to jump to the
  // {WasmCompileLazy} builtin, passing the function index.
  WasmCode* lazy_compile_table_ = nullptr;

  // The compilation state keeps track of compilation tasks for this module.
  // Note that its destructor blocks until all tasks are finished/aborted and
  // hence needs to be destructed first when this native module dies.
  std::unique_ptr<CompilationState> compilation_state_;

  // A cache of the import wrappers, keyed on the kind and signature.
  std::unique_ptr<WasmImportWrapperCache> import_wrapper_cache_;

  // This mutex protects concurrent calls to {AddCode} and friends.
  mutable base::Mutex allocation_mutex_;

  //////////////////////////////////////////////////////////////////////////////
  // Protected by {allocation_mutex_}:

  // Holds all allocated code objects. For lookup based on pc, the key is the
  // instruction start address of the value.
  std::map<Address, std::unique_ptr<WasmCode>> owned_code_;

  // Table of the latest code object per function, updated on initial
  // compilation and tier up. The number of entries is
  // {WasmModule::num_declared_functions}, i.e. there are no entries for
  // imported functions.
  std::unique_ptr<WasmCode*[]> code_table_;

  // Null if no redirections exist, otherwise a bitset over all functions in
  // this module marking those functions that have been redirected.
  std::unique_ptr<uint8_t[]> interpreter_redirections_;

  // Data (especially jump table) per code space.
  std::vector<CodeSpaceData> code_space_data_;

  // Debug information for this module. You only need to hold the allocation
  // mutex while getting the {DebugInfo} pointer, or initializing this field.
  // Further accesses to the {DebugInfo} do not need to be protected by the
  // mutex.
  std::unique_ptr<DebugInfo> debug_info_;

  bool tier_down_ = false;
  // End of fields protected by {allocation_mutex_}.
  //////////////////////////////////////////////////////////////////////////////

  WasmEngine* const engine_;
  int modification_scope_depth_ = 0;
  UseTrapHandler use_trap_handler_ = kNoTrapHandler;
  bool lazy_compile_frozen_ = false;

  DISALLOW_COPY_AND_ASSIGN(NativeModule);
};

class V8_EXPORT_PRIVATE WasmCodeManager final {
 public:
  explicit WasmCodeManager(size_t max_committed);

#ifdef DEBUG
  ~WasmCodeManager() {
    // No more committed code space.
    DCHECK_EQ(0, total_committed_code_space_.load());
  }
#endif

#if defined(V8_OS_WIN64)
  bool CanRegisterUnwindInfoForNonABICompliantCodeRange() const;
#endif  // V8_OS_WIN64

  NativeModule* LookupNativeModule(Address pc) const;
  WasmCode* LookupCode(Address pc) const;
  size_t committed_code_space() const {
    return total_committed_code_space_.load();
  }

  // Estimate the needed code space for a Liftoff function based on the size of
  // the function body (wasm byte code).
  static size_t EstimateLiftoffCodeSize(int body_size);
  // Estimate the needed code space from a completely decoded module.
  static size_t EstimateNativeModuleCodeSize(const WasmModule* module,
                                             bool include_liftoff);
  // Estimate the needed code space from the number of functions and total code
  // section length.
  static size_t EstimateNativeModuleCodeSize(int num_functions,
                                             int num_imported_functions,
                                             int code_section_length,
                                             bool include_liftoff);
  // Estimate the size of meta data needed for the NativeModule, excluding
  // generated code. This data still be stored on the C++ heap.
  static size_t EstimateNativeModuleMetaDataSize(const WasmModule* module);

 private:
  friend class WasmCodeAllocator;
  friend class WasmEngine;

  std::shared_ptr<NativeModule> NewNativeModule(
      WasmEngine* engine, Isolate* isolate,
      const WasmFeatures& enabled_features, size_t code_size_estimate,
      std::shared_ptr<const WasmModule> module);

  V8_WARN_UNUSED_RESULT VirtualMemory TryAllocate(size_t size,
                                                  void* hint = nullptr);
  bool Commit(base::AddressRegion);
  void Decommit(base::AddressRegion);

  void FreeNativeModule(Vector<VirtualMemory> owned_code,
                        size_t committed_size);

  void AssignRange(base::AddressRegion, NativeModule*);

  const size_t max_committed_code_space_;

  std::atomic<size_t> total_committed_code_space_{0};
  // If the committed code space exceeds {critical_committed_code_space_}, then
  // we trigger a GC before creating the next module. This value is set to the
  // currently committed space plus 50% of the available code space on creation
  // and updated after each GC.
  std::atomic<size_t> critical_committed_code_space_;

  mutable base::Mutex native_modules_mutex_;

  //////////////////////////////////////////////////////////////////////////////
  // Protected by {native_modules_mutex_}:

  std::map<Address, std::pair<Address, NativeModule*>> lookup_map_;

  // End of fields protected by {native_modules_mutex_}.
  //////////////////////////////////////////////////////////////////////////////

  DISALLOW_COPY_AND_ASSIGN(WasmCodeManager);
};

// Within the scope, the native_module is writable and not executable.
// At the scope's destruction, the native_module is executable and not writable.
// The states inside the scope and at the scope termination are irrespective of
// native_module's state when entering the scope.
// We currently mark the entire module's memory W^X:
//  - for AOT, that's as efficient as it can be.
//  - for Lazy, we don't have a heuristic for functions that may need patching,
//    and even if we did, the resulting set of pages may be fragmented.
//    Currently, we try and keep the number of syscalls low.
// -  similar argument for debug time.
class NativeModuleModificationScope final {
 public:
  explicit NativeModuleModificationScope(NativeModule* native_module);
  ~NativeModuleModificationScope();

 private:
  NativeModule* native_module_;
};

// {WasmCodeRefScope}s form a perfect stack. New {WasmCode} pointers generated
// by e.g. creating new code or looking up code by its address are added to the
// top-most {WasmCodeRefScope}.
class V8_EXPORT_PRIVATE WasmCodeRefScope {
 public:
  WasmCodeRefScope();
  ~WasmCodeRefScope();

  // Register a {WasmCode} reference in the current {WasmCodeRefScope}. Fails if
  // there is no current scope.
  static void AddRef(WasmCode*);

 private:
  WasmCodeRefScope* const previous_scope_;
  std::unordered_set<WasmCode*> code_ptrs_;

  DISALLOW_COPY_AND_ASSIGN(WasmCodeRefScope);
};

// Similarly to a global handle, a {GlobalWasmCodeRef} stores a single
// ref-counted pointer to a {WasmCode} object.
class GlobalWasmCodeRef {
 public:
  explicit GlobalWasmCodeRef(WasmCode* code,
                             std::shared_ptr<NativeModule> native_module)
      : code_(code), native_module_(std::move(native_module)) {
    code_->IncRef();
  }

  ~GlobalWasmCodeRef() { WasmCode::DecrementRefCount({&code_, 1}); }

  // Get a pointer to the contained {WasmCode} object. This is only guaranteed
  // to exist as long as this {GlobalWasmCodeRef} exists.
  WasmCode* code() const { return code_; }

 private:
  WasmCode* const code_;
  // Also keep the {NativeModule} alive.
  const std::shared_ptr<NativeModule> native_module_;
  DISALLOW_COPY_AND_ASSIGN(GlobalWasmCodeRef);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_CODE_MANAGER_H_

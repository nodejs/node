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
#include "src/wasm/wasm-tier.h"

namespace v8 {
namespace internal {

class Code;
class CodeDesc;
class Isolate;

namespace wasm {

class NativeModule;
class WasmCodeManager;
struct WasmCompilationResult;
class WasmEngine;
class WasmMemoryTracker;
class WasmImportWrapperCache;
struct WasmModule;

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
    kRuntimeStub,
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
  Vector<const byte> reloc_info() const { return reloc_info_.as_vector(); }
  Vector<const byte> source_positions() const {
    return source_position_table_.as_vector();
  }

  uint32_t index() const {
    DCHECK(!IsAnonymous());
    return index_;
  }
  // Anonymous functions are functions that don't carry an index.
  bool IsAnonymous() const { return index_ == kAnonymousFuncIndex; }
  Kind kind() const { return kind_; }
  NativeModule* native_module() const { return native_module_; }
  ExecutionTier tier() const { return tier_; }
  Address constant_pool() const;
  Address handler_table() const;
  uint32_t handler_table_size() const;
  Address code_comments() const;
  uint32_t code_comments_size() const;
  size_t constant_pool_offset() const { return constant_pool_offset_; }
  size_t safepoint_table_offset() const { return safepoint_table_offset_; }
  size_t handler_table_offset() const { return handler_table_offset_; }
  size_t code_comments_offset() const { return code_comments_offset_; }
  size_t unpadded_binary_size() const { return unpadded_binary_size_; }
  uint32_t stack_slots() const { return stack_slots_; }
  uint32_t tagged_parameter_slots() const { return tagged_parameter_slots_; }
  bool is_liftoff() const { return tier_ == ExecutionTier::kLiftoff; }
  bool contains(Address pc) const {
    return reinterpret_cast<Address>(instructions_.begin()) <= pc &&
           pc < reinterpret_cast<Address>(instructions_.end());
  }

  Vector<trap_handler::ProtectedInstructionData> protected_instructions()
      const {
    return protected_instructions_.as_vector();
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

  STATIC_ASSERT(kAnonymousFuncIndex > kV8MaxWasmFunctions);

 private:
  friend class NativeModule;

  WasmCode(NativeModule* native_module, uint32_t index,
           Vector<byte> instructions, uint32_t stack_slots,
           uint32_t tagged_parameter_slots, size_t safepoint_table_offset,
           size_t handler_table_offset, size_t constant_pool_offset,
           size_t code_comments_offset, size_t unpadded_binary_size,
           OwnedVector<trap_handler::ProtectedInstructionData>
               protected_instructions,
           OwnedVector<const byte> reloc_info,
           OwnedVector<const byte> source_position_table, Kind kind,
           ExecutionTier tier)
      : instructions_(instructions),
        reloc_info_(std::move(reloc_info)),
        source_position_table_(std::move(source_position_table)),
        native_module_(native_module),
        index_(index),
        kind_(kind),
        constant_pool_offset_(constant_pool_offset),
        stack_slots_(stack_slots),
        tagged_parameter_slots_(tagged_parameter_slots),
        safepoint_table_offset_(safepoint_table_offset),
        handler_table_offset_(handler_table_offset),
        code_comments_offset_(code_comments_offset),
        unpadded_binary_size_(unpadded_binary_size),
        protected_instructions_(std::move(protected_instructions)),
        tier_(tier) {
    DCHECK_LE(safepoint_table_offset, unpadded_binary_size);
    DCHECK_LE(handler_table_offset, unpadded_binary_size);
    DCHECK_LE(code_comments_offset, unpadded_binary_size);
    DCHECK_LE(constant_pool_offset, unpadded_binary_size);
  }

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
  OwnedVector<const byte> reloc_info_;
  OwnedVector<const byte> source_position_table_;
  NativeModule* native_module_ = nullptr;
  uint32_t index_;
  Kind kind_;
  size_t constant_pool_offset_ = 0;
  uint32_t stack_slots_ = 0;
  // Number of tagged parameters passed to this function via the stack. This
  // value is used by the stack walker (e.g. GC) to find references.
  uint32_t tagged_parameter_slots_ = 0;
  // we care about safepoint data for wasm-to-js functions,
  // since there may be stack/register tagged values for large number
  // conversions.
  size_t safepoint_table_offset_ = 0;
  size_t handler_table_offset_ = 0;
  size_t code_comments_offset_ = 0;
  size_t unpadded_binary_size_ = 0;
  int trap_handler_index_ = -1;
  OwnedVector<trap_handler::ProtectedInstructionData> protected_instructions_;
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

WasmCode::Kind GetCodeKind(const WasmCompilationResult& result);

// Return a textual description of the kind.
const char* GetWasmCodeKindAsString(WasmCode::Kind);

// Manages the code reservations and allocations of a single {NativeModule}.
class WasmCodeAllocator {
 public:
  WasmCodeAllocator(WasmCodeManager*, VirtualMemory code_space,
                    bool can_request_more,
                    std::shared_ptr<Counters> async_counters);
  ~WasmCodeAllocator();

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

  // Sets permissions of all owned code space to executable, or read-write (if
  // {executable} is false). Returns true on success.
  V8_EXPORT_PRIVATE bool SetExecutable(bool executable);

  // Free memory pages of all given code objects. Used for wasm code GC.
  void FreeCode(Vector<WasmCode* const>);

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

  const bool can_request_more_memory_;

  std::shared_ptr<Counters> async_counters_;
};

class V8_EXPORT_PRIVATE NativeModule final {
 public:
#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_S390X || V8_TARGET_ARCH_ARM64
  static constexpr bool kCanAllocateMoreMemory = false;
#else
  static constexpr bool kCanAllocateMoreMemory = true;
#endif

  // {AddCode} is thread safe w.r.t. other calls to {AddCode} or methods adding
  // code below, i.e. it can be called concurrently from background threads.
  // The returned code still needs to be published via {PublishCode}.
  std::unique_ptr<WasmCode> AddCode(
      uint32_t index, const CodeDesc& desc, uint32_t stack_slots,
      uint32_t tagged_parameter_slots,
      OwnedVector<trap_handler::ProtectedInstructionData>
          protected_instructions,
      OwnedVector<const byte> source_position_table, WasmCode::Kind kind,
      ExecutionTier tier);

  // {PublishCode} makes the code available to the system by entering it into
  // the code table and patching the jump table. It returns a raw pointer to the
  // given {WasmCode} object.
  WasmCode* PublishCode(std::unique_ptr<WasmCode>);
  // Hold the {allocation_mutex_} when calling {PublishCodeLocked}.
  WasmCode* PublishCodeLocked(std::unique_ptr<WasmCode>);

  WasmCode* AddDeserializedCode(
      uint32_t index, Vector<const byte> instructions, uint32_t stack_slots,
      uint32_t tagged_parameter_slots, size_t safepoint_table_offset,
      size_t handler_table_offset, size_t constant_pool_offset,
      size_t code_comments_offset, size_t unpadded_binary_size,
      OwnedVector<trap_handler::ProtectedInstructionData>
          protected_instructions,
      OwnedVector<const byte> reloc_info,
      OwnedVector<const byte> source_position_table, WasmCode::Kind kind,
      ExecutionTier tier);

  // Adds anonymous code for testing purposes.
  WasmCode* AddCodeForTesting(Handle<Code> code);

  // Use {UseLazyStub} to setup lazy compilation per function. It will use the
  // existing {WasmCode::kWasmCompileLazy} runtime stub and populate the jump
  // table with trampolines accordingly.
  void UseLazyStub(uint32_t func_index);

  // Initializes all runtime stubs by setting up entry addresses in the runtime
  // stub table. It must be called exactly once per native module before adding
  // other WasmCode so that runtime stub ids can be resolved during relocation.
  void SetRuntimeStubs(Isolate* isolate);

  // Creates a snapshot of the current state of the code table. This is useful
  // to get a consistent view of the table (e.g. used by the serializer).
  std::vector<WasmCode*> SnapshotCodeTable() const;

  WasmCode* GetCode(uint32_t index) const;
  bool HasCode(uint32_t index) const;

  Address runtime_stub_entry(WasmCode::RuntimeStubId index) const {
    DCHECK_LT(index, WasmCode::kRuntimeStubCount);
    Address entry_address = runtime_stub_entries_[index];
    DCHECK_NE(kNullAddress, entry_address);
    return entry_address;
  }

  Address jump_table_start() const {
    return jump_table_ ? jump_table_->instruction_start() : kNullAddress;
  }

  uint32_t GetJumpTableOffset(uint32_t func_index) const;

  bool is_jump_table_slot(Address address) const {
    return jump_table_->contains(address);
  }

  // Returns the target to call for the given function (returns a jump table
  // slot within {jump_table_}).
  Address GetCallTargetForFunction(uint32_t func_index) const;

  // Reverse lookup from a given call target (i.e. a jump table slot as the
  // above {GetCallTargetForFunction} returns) to a function index.
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

  const char* GetRuntimeStubName(Address runtime_stub_entry) const;

  // Sample the current code size of this modules to the given counters.
  enum CodeSamplingTime : int8_t { kAfterBaseline, kAfterTopTier, kSampling };
  void SampleCodeSize(Counters*, CodeSamplingTime) const;

  WasmCode* AddCompiledCode(WasmCompilationResult);
  std::vector<WasmCode*> AddCompiledCode(Vector<WasmCompilationResult>);

  // Allows to check whether a function has been redirected to the interpreter
  // by publishing an entry stub with the {Kind::kInterpreterEntry} code kind.
  bool IsRedirectedToInterpreter(uint32_t func_index);

  // Free a set of functions of this module. Uncommits whole pages if possible.
  // The given vector must be ordered by the instruction start address, and all
  // {WasmCode} objects must not be used any more.
  // Should only be called via {WasmEngine::FreeDeadCode}, so the engine can do
  // its accounting.
  void FreeCode(Vector<WasmCode* const>);

 private:
  friend class WasmCode;
  friend class WasmCodeManager;
  friend class NativeModuleModificationScope;

  // Private constructor, called via {WasmCodeManager::NewNativeModule()}.
  NativeModule(WasmEngine* engine, const WasmFeatures& enabled_features,
               bool can_request_more, VirtualMemory code_space,
               std::shared_ptr<const WasmModule> module,
               std::shared_ptr<Counters> async_counters,
               std::shared_ptr<NativeModule>* shared_this);

  std::unique_ptr<WasmCode> AddCodeWithCodeSpace(
      uint32_t index, const CodeDesc& desc, uint32_t stack_slots,
      uint32_t tagged_parameter_slots,
      OwnedVector<trap_handler::ProtectedInstructionData>
          protected_instructions,
      OwnedVector<const byte> source_position_table, WasmCode::Kind kind,
      ExecutionTier tier, Vector<uint8_t> code_space);

  // Add and publish anonymous code.
  WasmCode* AddAndPublishAnonymousCode(Handle<Code>, WasmCode::Kind kind,
                                       const char* name = nullptr);

  WasmCode* CreateEmptyJumpTable(uint32_t jump_table_size);

  // Hold the {allocation_mutex_} when calling this method.
  bool has_interpreter_redirection(uint32_t func_index) {
    DCHECK_LT(func_index, num_functions());
    DCHECK_LE(module_->num_imported_functions, func_index);
    if (!interpreter_redirections_) return false;
    uint32_t bitset_idx = func_index - module_->num_imported_functions;
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
    uint32_t bitset_idx = func_index - module_->num_imported_functions;
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

  // Wire bytes, held in a shared_ptr so they can be kept alive by the
  // {WireBytesStorage}, held by background compile tasks.
  std::shared_ptr<OwnedVector<const uint8_t>> wire_bytes_;

  // Contains entry points for runtime stub calls via {WASM_STUB_CALL}.
  Address runtime_stub_entries_[WasmCode::kRuntimeStubCount] = {kNullAddress};

  // Jump table used for runtime stubs (i.e. trampolines to embedded builtins).
  WasmCode* runtime_stub_table_ = nullptr;

  // Jump table used to easily redirect wasm function calls.
  WasmCode* jump_table_ = nullptr;

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

  std::unique_ptr<WasmCode* []> code_table_;

  // Null if no redirections exist, otherwise a bitset over all functions in
  // this module marking those functions that have been redirected.
  std::unique_ptr<uint8_t[]> interpreter_redirections_;

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
  explicit WasmCodeManager(WasmMemoryTracker* memory_tracker,
                           size_t max_committed);

#ifdef DEBUG
  ~WasmCodeManager() {
    // No more committed code space.
    DCHECK_EQ(0, total_committed_code_space_.load());
  }
#endif

#if defined(V8_OS_WIN_X64)
  bool CanRegisterUnwindInfoForNonABICompliantCodeRange() const;
#endif

  NativeModule* LookupNativeModule(Address pc) const;
  WasmCode* LookupCode(Address pc) const;
  size_t committed_code_space() const {
    return total_committed_code_space_.load();
  }

  void SetMaxCommittedMemoryForTesting(size_t limit);

#if defined(V8_OS_WIN_X64)
  void DisableWin64UnwindInfoForTesting() {
    is_win64_unwind_info_disabled_for_testing_ = true;
  }
#endif

  static size_t EstimateNativeModuleCodeSize(const WasmModule* module);
  static size_t EstimateNativeModuleNonCodeSize(const WasmModule* module);

 private:
  friend class WasmCodeAllocator;
  friend class WasmEngine;

  std::shared_ptr<NativeModule> NewNativeModule(
      WasmEngine* engine, Isolate* isolate,
      const WasmFeatures& enabled_features, size_t code_size_estimate,
      bool can_request_more, std::shared_ptr<const WasmModule> module);

  V8_WARN_UNUSED_RESULT VirtualMemory TryAllocate(size_t size,
                                                  void* hint = nullptr);
  bool Commit(base::AddressRegion);
  void Decommit(base::AddressRegion);

  void FreeNativeModule(Vector<VirtualMemory> owned_code,
                        size_t committed_size);

  void AssignRange(base::AddressRegion, NativeModule*);

  WasmMemoryTracker* const memory_tracker_;

  size_t max_committed_code_space_;

#if defined(V8_OS_WIN_X64)
  bool is_win64_unwind_info_disabled_for_testing_;
#endif

  std::atomic<size_t> total_committed_code_space_;
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

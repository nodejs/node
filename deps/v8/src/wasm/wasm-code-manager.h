// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_CODE_MANAGER_H_
#define V8_WASM_WASM_CODE_MANAGER_H_

#include <functional>
#include <list>
#include <map>
#include <unordered_map>

#include "src/base/macros.h"
#include "src/handles.h"
#include "src/trap-handler/trap-handler.h"
#include "src/vector.h"
#include "src/wasm/module-compiler.h"

namespace v8 {
class Isolate;
namespace internal {

struct CodeDesc;
class Code;
class Histogram;
class WasmCompiledModule;

namespace wasm {

class NativeModule;
class WasmCodeManager;
struct WasmModule;

// Sorted, disjoint and non-overlapping memory ranges. A range is of the
// form [start, end). So there's no [start, end), [end, other_end),
// because that should have been reduced to [start, other_end).
using AddressRange = std::pair<Address, Address>;
class V8_EXPORT_PRIVATE DisjointAllocationPool final {
 public:
  enum ExtractionMode : bool { kAny = false, kContiguous = true };
  DisjointAllocationPool() {}

  explicit DisjointAllocationPool(Address, Address);

  DisjointAllocationPool(DisjointAllocationPool&& other) = default;
  DisjointAllocationPool& operator=(DisjointAllocationPool&& other) = default;

  // Merge the ranges of the parameter into this object. Ordering is
  // preserved. The assumption is that the passed parameter is
  // not intersecting this object - for example, it was obtained
  // from a previous Allocate{Pool}.
  void Merge(DisjointAllocationPool&&);

  // Allocate a contiguous range of size {size}. Return an empty pool on
  // failure.
  DisjointAllocationPool Allocate(size_t size) {
    return Extract(size, kContiguous);
  }

  // Allocate a sub-pool of size {size}. Return an empty pool on failure.
  DisjointAllocationPool AllocatePool(size_t size) {
    return Extract(size, kAny);
  }

  bool IsEmpty() const { return ranges_.empty(); }
  const std::list<AddressRange>& ranges() const { return ranges_; }

 private:
  // Extract out a total of {size}. By default, the return may
  // be more than one range. If kContiguous is passed, the return
  // will be one range. If the operation fails, this object is
  // unchanged, and the return {IsEmpty()}
  DisjointAllocationPool Extract(size_t size, ExtractionMode mode);

  std::list<AddressRange> ranges_;

  DISALLOW_COPY_AND_ASSIGN(DisjointAllocationPool)
};

using ProtectedInstructions =
    std::vector<trap_handler::ProtectedInstructionData>;

class V8_EXPORT_PRIVATE WasmCode final {
 public:
  enum Kind {
    kFunction,
    kWasmToJsWrapper,
    kLazyStub,
    kInterpreterEntry,
    kTrampoline
  };

  // kOther is used if we have WasmCode that is neither
  // liftoff- nor turbofan-compiled, i.e. if Kind is
  // not a kFunction.
  enum Tier : int8_t { kLiftoff, kTurbofan, kOther };

  Vector<byte> instructions() const { return instructions_; }
  Address instruction_start() const {
    return reinterpret_cast<Address>(instructions_.start());
  }
  Vector<const byte> reloc_info() const {
    return {reloc_info_.get(), reloc_size_};
  }
  Vector<const byte> source_positions() const {
    return {source_position_table_.get(), source_position_size_};
  }

  uint32_t index() const { return index_.ToChecked(); }
  // Anonymous functions are functions that don't carry an index, like
  // trampolines.
  bool IsAnonymous() const { return index_.IsNothing(); }
  Kind kind() const { return kind_; }
  NativeModule* native_module() const { return native_module_; }
  Tier tier() const { return tier_; }
  Address constant_pool() const;
  size_t constant_pool_offset() const { return constant_pool_offset_; }
  size_t safepoint_table_offset() const { return safepoint_table_offset_; }
  size_t handler_table_offset() const { return handler_table_offset_; }
  uint32_t stack_slots() const { return stack_slots_; }
  bool is_liftoff() const { return tier_ == kLiftoff; }
  bool contains(Address pc) const {
    return reinterpret_cast<Address>(instructions_.start()) <= pc &&
           pc < reinterpret_cast<Address>(instructions_.end());
  }

  const ProtectedInstructions& protected_instructions() const {
    // TODO(mstarzinger): Code that doesn't have trapping instruction should
    // not be required to have this vector, make it possible to be null.
    DCHECK_NOT_NULL(protected_instructions_);
    return *protected_instructions_.get();
  }

  // Register protected instruction information with the trap handler. Sets
  // trap_handler_index.
  void RegisterTrapHandlerData();

  void Print(Isolate* isolate) const;
  void Disassemble(const char* name, Isolate* isolate, std::ostream& os,
                   Address current_pc = kNullAddress) const;

  static bool ShouldBeLogged(Isolate* isolate);
  void LogCode(Isolate* isolate) const;

  ~WasmCode();

  enum FlushICache : bool { kFlushICache = true, kNoFlushICache = false };

  // Offset of {instructions_.start()}. It is used for tiering, when
  // we check if optimized code is available during the prologue
  // of Liftoff-compiled code.
  static constexpr int kInstructionStartOffset = 0;

 private:
  friend class NativeModule;

  WasmCode(Vector<byte> instructions, std::unique_ptr<const byte[]> reloc_info,
           size_t reloc_size, std::unique_ptr<const byte[]> source_pos,
           size_t source_pos_size, NativeModule* native_module,
           Maybe<uint32_t> index, Kind kind, size_t constant_pool_offset,
           uint32_t stack_slots, size_t safepoint_table_offset,
           size_t handler_table_offset,
           std::unique_ptr<ProtectedInstructions> protected_instructions,
           Tier tier)
      : instructions_(instructions),
        reloc_info_(std::move(reloc_info)),
        reloc_size_(reloc_size),
        source_position_table_(std::move(source_pos)),
        source_position_size_(source_pos_size),
        native_module_(native_module),
        index_(index),
        kind_(kind),
        constant_pool_offset_(constant_pool_offset),
        stack_slots_(stack_slots),
        safepoint_table_offset_(safepoint_table_offset),
        handler_table_offset_(handler_table_offset),
        protected_instructions_(std::move(protected_instructions)),
        tier_(tier) {
    DCHECK_LE(safepoint_table_offset, instructions.size());
    DCHECK_LE(constant_pool_offset, instructions.size());
    DCHECK_LE(handler_table_offset, instructions.size());
    DCHECK_EQ(kInstructionStartOffset, OFFSET_OF(WasmCode, instructions_));
  }

  // Code objects that have been registered with the global trap handler within
  // this process, will have a {trap_handler_index} associated with them.
  size_t trap_handler_index() const;
  void set_trap_handler_index(size_t);
  bool HasTrapHandlerIndex() const;
  void ResetTrapHandlerIndex();

  Vector<byte> instructions_;
  std::unique_ptr<const byte[]> reloc_info_;
  size_t reloc_size_ = 0;
  std::unique_ptr<const byte[]> source_position_table_;
  size_t source_position_size_ = 0;
  NativeModule* native_module_ = nullptr;
  Maybe<uint32_t> index_;
  Kind kind_;
  size_t constant_pool_offset_ = 0;
  uint32_t stack_slots_ = 0;
  // we care about safepoint data for wasm-to-js functions,
  // since there may be stack/register tagged values for large number
  // conversions.
  size_t safepoint_table_offset_ = 0;
  size_t handler_table_offset_ = 0;
  intptr_t trap_handler_index_ = -1;
  std::unique_ptr<ProtectedInstructions> protected_instructions_;
  Tier tier_;

  DISALLOW_COPY_AND_ASSIGN(WasmCode);
};

// Return a textual description of the kind.
const char* GetWasmCodeKindAsString(WasmCode::Kind);

// Note that we currently need to add code on the main thread, because we may
// trigger a GC if we believe there's a chance the GC would clear up native
// modules. The code is ready for concurrency otherwise, we just need to be
// careful about this GC consideration. See WouldGCHelp and
// WasmCodeManager::Commit.
class V8_EXPORT_PRIVATE NativeModule final {
 public:
  WasmCode* AddCode(const CodeDesc& desc, uint32_t frame_count, uint32_t index,
                    size_t safepoint_table_offset, size_t handler_table_offset,
                    std::unique_ptr<ProtectedInstructions>,
                    Handle<ByteArray> source_position_table,
                    WasmCode::Tier tier);

  // A way to copy over JS-allocated code. This is because we compile
  // certain wrappers using a different pipeline.
  WasmCode* AddCodeCopy(Handle<Code> code, WasmCode::Kind kind, uint32_t index);

  // Add an interpreter entry. For the same reason as AddCodeCopy, we
  // currently compile these using a different pipeline and we can't get a
  // CodeDesc here. When adding interpreter wrappers, we do not insert them in
  // the code_table, however, we let them self-identify as the {index} function
  WasmCode* AddInterpreterEntry(Handle<Code> code, uint32_t index);

  // When starting lazy compilation, provide the WasmLazyCompile builtin by
  // calling SetLazyBuiltin. It will initialize the code table with it. Copies
  // of it might be cloned from them later when creating entries for exported
  // functions and indirect callable functions, so that they may be identified
  // by the runtime.
  void SetLazyBuiltin(Handle<Code> code);

  // function_count is WasmModule::functions.size().
  uint32_t function_count() const {
    DCHECK_LE(code_table_.size(), std::numeric_limits<uint32_t>::max());
    return static_cast<uint32_t>(code_table_.size());
  }

  WasmCode* code(uint32_t index) const {
    DCHECK_LT(index, function_count());
    DCHECK_LE(num_imported_functions(), index);
    return code_table_[index];
  }

  // TODO(clemensh): Remove this method once we have the jump table
  // (crbug.com/v8/7758).
  void SetCodeForTesting(uint32_t index, WasmCode* code) {
    DCHECK_LT(index, function_count());
    DCHECK_LE(num_imported_functions(), index);
    code_table_[index] = code;
  }

  bool has_code(uint32_t index) const {
    DCHECK_LT(index, function_count());
    return code_table_[index] != nullptr;
  }

  // Register/release the protected instructions in all code objects with the
  // global trap handler for this process.
  void UnpackAndRegisterProtectedInstructions();
  void ReleaseProtectedInstructions();

  // Returns the instruction start of code suitable for indirect or import calls
  // for the given function index. If the code at the given index is the lazy
  // compile stub, it will clone a non-anonymous lazy compile stub for the
  // purpose. This will soon change to always return a jump table slot.
  Address GetCallTargetForFunction(uint32_t index);

  bool SetExecutable(bool executable);

  // For cctests, where we build both WasmModule and the runtime objects
  // on the fly, and bypass the instance builder pipeline.
  void ResizeCodeTableForTesting(size_t num_functions, size_t max_functions);

  CompilationState* compilation_state() { return compilation_state_.get(); }

  // TODO(mstarzinger): The link to the {shared_module_data} is deprecated and
  // all uses should vanish to make {NativeModule} independent of the Isolate.
  WasmSharedModuleData* shared_module_data() const;
  void SetSharedModuleData(Handle<WasmSharedModuleData>);

  uint32_t num_imported_functions() const { return num_imported_functions_; }
  const std::vector<WasmCode*>& code_table() const { return code_table_; }
  bool use_trap_handler() const { return use_trap_handler_; }
  void set_lazy_compile_frozen(bool frozen) { lazy_compile_frozen_ = frozen; }
  bool lazy_compile_frozen() const { return lazy_compile_frozen_; }

  const size_t instance_id = 0;
  ~NativeModule();

 private:
  friend class WasmCodeManager;
  friend class NativeModuleSerializer;
  friend class NativeModuleDeserializer;
  friend class NativeModuleModificationScope;

  static base::AtomicNumber<size_t> next_id_;
  NativeModule(uint32_t num_functions, uint32_t num_imports,
               bool can_request_more, VirtualMemory* code_space,
               WasmCodeManager* code_manager, ModuleEnv& env);

  WasmCode* AddAnonymousCode(Handle<Code>, WasmCode::Kind kind);
  Address AllocateForCode(size_t size);

  // Primitive for adding code to the native module. All code added to a native
  // module is owned by that module. Various callers get to decide on how the
  // code is obtained (CodeDesc vs, as a point in time, Code*), the kind,
  // whether it has an index or is anonymous, etc.
  WasmCode* AddOwnedCode(Vector<const byte> orig_instructions,
                         std::unique_ptr<const byte[]> reloc_info,
                         size_t reloc_size,
                         std::unique_ptr<const byte[]> source_pos,
                         size_t source_pos_size, Maybe<uint32_t> index,
                         WasmCode::Kind kind, size_t constant_pool_offset,
                         uint32_t stack_slots, size_t safepoint_table_offset,
                         size_t handler_table_offset,
                         std::unique_ptr<ProtectedInstructions>, WasmCode::Tier,
                         WasmCode::FlushICache);
  WasmCode* CloneCode(const WasmCode*, WasmCode::FlushICache);
  WasmCode* Lookup(Address);
  Address GetLocalAddressFor(Handle<Code>);
  Address CreateTrampolineTo(Handle<Code>);

  // Holds all allocated code objects, is maintained to be in ascending order
  // according to the codes instruction start address to allow lookups.
  std::vector<std::unique_ptr<WasmCode>> owned_code_;

  std::vector<WasmCode*> code_table_;
  std::unique_ptr<std::vector<WasmCode*>> lazy_compile_stubs_;
  uint32_t num_imported_functions_;

  // Maps from instruction start of an immovable code object to instruction
  // start of the trampoline.
  std::unordered_map<Address, Address> trampolines_;

  std::unique_ptr<CompilationState, CompilationStateDeleter> compilation_state_;

  // A phantom reference to the {WasmSharedModuleData}. It is intentionally not
  // typed {Handle<WasmSharedModuleData>} because this location will be cleared
  // when the phantom reference is cleared.
  WasmSharedModuleData** shared_module_data_ = nullptr;

  DisjointAllocationPool free_code_space_;
  DisjointAllocationPool allocated_code_space_;
  std::list<VirtualMemory> owned_code_space_;

  WasmCodeManager* wasm_code_manager_;
  base::Mutex allocation_mutex_;
  size_t committed_code_space_ = 0;
  int modification_scope_depth_ = 0;
  bool can_request_more_memory_;
  bool use_trap_handler_;
  bool is_executable_ = false;
  bool lazy_compile_frozen_ = false;

  DISALLOW_COPY_AND_ASSIGN(NativeModule);
};

class V8_EXPORT_PRIVATE WasmCodeManager final {
 public:
  // The only reason we depend on Isolate is to report native memory used
  // and held by a GC-ed object. We'll need to mitigate that when we
  // start sharing wasm heaps.
  WasmCodeManager(v8::Isolate*, size_t max_committed);
  // Create a new NativeModule. The caller is responsible for its
  // lifetime. The native module will be given some memory for code,
  // which will be page size aligned. The size of the initial memory
  // is determined with a heuristic based on the total size of wasm
  // code. The native module may later request more memory.
  std::unique_ptr<NativeModule> NewNativeModule(const WasmModule& module,
                                                ModuleEnv& env);
  std::unique_ptr<NativeModule> NewNativeModule(size_t memory_estimate,
                                                uint32_t num_functions,
                                                uint32_t num_imported_functions,
                                                bool can_request_more,
                                                ModuleEnv& env);

  WasmCode* LookupCode(Address pc) const;
  WasmCode* GetCodeFromStartAddress(Address pc) const;
  size_t remaining_uncommitted_code_space() const;

  void SetModuleCodeSizeHistogram(Histogram* histogram) {
    module_code_size_mb_ = histogram;
  }

 private:
  friend class NativeModule;

  void TryAllocate(size_t size, VirtualMemory*, void* hint = nullptr);
  bool Commit(Address, size_t);
  // Currently, we uncommit a whole module, so all we need is account
  // for the freed memory size. We do that in FreeNativeModule.
  // There's no separate Uncommit.

  void FreeNativeModule(NativeModule*);
  void Free(VirtualMemory* mem);
  void AssignRanges(Address start, Address end, NativeModule*);
  size_t GetAllocationChunk(const WasmModule& module);
  bool WouldGCHelp() const;

  std::map<Address, std::pair<Address, NativeModule*>> lookup_map_;
  // Count of NativeModules not yet collected. Helps determine if it's
  // worth requesting a GC on memory pressure.
  size_t active_ = 0;
  std::atomic<size_t> remaining_uncommitted_code_space_;

  // TODO(mtrofin): remove the dependency on isolate.
  v8::Isolate* isolate_;

  // Histogram to update with the maximum used code space for each NativeModule.
  Histogram* module_code_size_mb_ = nullptr;

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

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_CODE_MANAGER_H_

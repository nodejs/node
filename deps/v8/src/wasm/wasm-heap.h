// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_HEAP_H_
#define V8_WASM_HEAP_H_

#include <functional>
#include <list>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include "src/base/macros.h"
#include "src/handles.h"
#include "src/trap-handler/trap-handler.h"
#include "src/vector.h"

namespace v8 {
class Isolate;
namespace internal {

struct CodeDesc;
class Code;
class WasmCompiledModule;

namespace wasm {

using GlobalHandleAddress = Address;
class NativeModule;
struct WasmModule;

struct AddressHasher {
  size_t operator()(const Address& addr) const {
    return std::hash<intptr_t>()(reinterpret_cast<intptr_t>(addr));
  }
};

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
    Function,
    WasmToWasmWrapper,
    WasmToJsWrapper,
    LazyStub,
    InterpreterStub,
    CopiedStub,
    Trampoline
  };

  Vector<byte> instructions() const { return instructions_; }
  Vector<const byte> reloc_info() const {
    return {reloc_info_.get(), reloc_size_};
  }

  uint32_t index() const { return index_.ToChecked(); }
  // Anonymous functions are functions that don't carry an index, like
  // trampolines.
  bool IsAnonymous() const { return index_.IsNothing(); }
  Kind kind() const { return kind_; }
  NativeModule* owner() const { return owner_; }
  Address constant_pool() const;
  size_t constant_pool_offset() const { return constant_pool_offset_; }
  size_t safepoint_table_offset() const { return safepoint_table_offset_; }
  uint32_t stack_slots() const { return stack_slots_; }
  bool is_liftoff() const { return is_liftoff_; }

  size_t trap_handler_index() const;
  void set_trap_handler_index(size_t);
  bool HasTrapHandlerIndex() const;
  void ResetTrapHandlerIndex();

  const ProtectedInstructions& protected_instructions() const {
    return *protected_instructions_.get();
  }

  void Disassemble(Isolate* isolate, const char* name, std::ostream& os) const;
  void Print(Isolate* isolate) const;

  ~WasmCode();

 private:
  friend class NativeModule;
  friend class NativeModuleDeserializer;

  // A constructor used just for implementing Lookup.
  WasmCode(Address pc) : instructions_(pc, 0), index_(Nothing<uint32_t>()) {}

  WasmCode(Vector<byte> instructions,
           std::unique_ptr<const byte[]>&& reloc_info, size_t reloc_size,
           NativeModule* owner, Maybe<uint32_t> index, Kind kind,
           size_t constant_pool_offset, uint32_t stack_slots,
           size_t safepoint_table_offset,
           std::shared_ptr<ProtectedInstructions> protected_instructions,
           bool is_liftoff = false)
      : instructions_(instructions),
        reloc_info_(std::move(reloc_info)),
        reloc_size_(reloc_size),
        owner_(owner),
        index_(index),
        kind_(kind),
        constant_pool_offset_(constant_pool_offset),
        stack_slots_(stack_slots),
        safepoint_table_offset_(safepoint_table_offset),
        protected_instructions_(protected_instructions),
        is_liftoff_(is_liftoff) {}

  WasmCode(const WasmCode&) = delete;
  WasmCode& operator=(const WasmCode&) = delete;

  Vector<byte> instructions_;
  std::unique_ptr<const byte[]> reloc_info_;
  size_t reloc_size_ = 0;
  NativeModule* owner_ = nullptr;
  Maybe<uint32_t> index_;
  Kind kind_;
  size_t constant_pool_offset_ = 0;
  uint32_t stack_slots_ = 0;
  // we care about safepoint data for wasm-to-js functions,
  // since there may be stack/register tagged values for large number
  // conversions.
  size_t safepoint_table_offset_ = 0;
  intptr_t trap_handler_index_ = -1;
  std::shared_ptr<ProtectedInstructions> protected_instructions_;
  bool is_liftoff_;
};

class WasmCodeManager;

// Note that we currently need to add code on the main thread, because we may
// trigger a GC if we believe there's a chance the GC would clear up native
// modules. The code is ready for concurrency otherwise, we just need to be
// careful about this GC consideration. See WouldGCHelp and
// WasmCodeManager::Commit.
class V8_EXPORT_PRIVATE NativeModule final {
 public:
  std::unique_ptr<NativeModule> Clone();

  WasmCode* AddCode(const CodeDesc& desc, uint32_t frame_count, uint32_t index,
                    size_t safepoint_table_offset,
                    std::shared_ptr<ProtectedInstructions>,
                    bool is_liftoff = false);

  // A way to copy over JS-allocated code. This is because we compile
  // certain wrappers using a different pipeline.
  WasmCode* AddCodeCopy(Handle<Code> code, WasmCode::Kind kind, uint32_t index);

  // Add an interpreter wrapper. For the same reason as AddCodeCopy, we
  // currently compile these using a different pipeline and we can't get a
  // CodeDesc here. When adding interpreter wrappers, we do not insert them in
  // the code_table, however, we let them self-identify as the {index} function
  WasmCode* AddInterpreterWrapper(Handle<Code> code, uint32_t index);

  // When starting lazy compilation, provide the WasmLazyCompile builtin by
  // calling SetLazyBuiltin. It will initialize the code table with it, and the
  // lazy_builtin_ field. The latter is used when creating entries for exported
  // functions and indirect callable functions, so that they may be identified
  // by the runtime.
  WasmCode* SetLazyBuiltin(Handle<Code> code);

  // ExportedWrappers are WasmToWasmWrappers for functions placed on import
  // tables. We construct them as-needed.
  WasmCode* GetExportedWrapper(uint32_t index);
  WasmCode* AddExportedWrapper(Handle<Code> code, uint32_t index);

  // FunctionCount is WasmModule::functions.size().
  uint32_t FunctionCount() const;
  WasmCode* GetCode(uint32_t index) const;

  WasmCode* lazy_builtin() const { return lazy_builtin_; }

  // We special-case lazy cloning because we currently rely on making copies
  // of the lazy builtin, to be able to identify, in the runtime, which function
  // the lazy builtin is a placeholder of. If we used trampolines, we would call
  // the runtime function from a common pc. We could, then, figure who the
  // caller was if the trampolines called rather than jumped to the common
  // builtin. The logic for seeking though frames would change, though.
  // TODO(mtrofin): perhaps we can do exactly that - either before or after
  // this change.
  WasmCode* CloneLazyBuiltinInto(uint32_t);

  // For cctests, where we build both WasmModule and the runtime objects
  // on the fly, and bypass the instance builder pipeline.
  void ResizeCodeTableForTest(size_t);
  void LinkAll();
  void Link(uint32_t index);

  // TODO(mtrofin): needed until we sort out exception handlers and
  // source positions, which are still on the  GC-heap.
  WasmCompiledModule* compiled_module() const;
  void SetCompiledModule(Handle<WasmCompiledModule>);

  // Shorthand accessors to the specialization data content.
  std::vector<wasm::GlobalHandleAddress>& function_tables() {
    return specialization_data_.function_tables;
  }
  std::vector<wasm::GlobalHandleAddress>& signature_tables() {
    return specialization_data_.signature_tables;
  }

  std::vector<wasm::GlobalHandleAddress>& empty_function_tables() {
    return specialization_data_.empty_function_tables;
  }
  std::vector<wasm::GlobalHandleAddress>& empty_signature_tables() {
    return specialization_data_.empty_signature_tables;
  }

  uint32_t num_imported_functions() const { return num_imported_functions_; }
  size_t num_function_tables() const {
    return specialization_data_.empty_function_tables.size();
  }

  size_t committed_memory() const { return committed_memory_; }
  const size_t instance_id = 0;
  ~NativeModule();

 private:
  friend class WasmCodeManager;
  friend class NativeModuleSerializer;
  friend class NativeModuleDeserializer;

  struct WasmCodeUniquePtrComparer {
    bool operator()(const std::unique_ptr<WasmCode>& a,
                    const std::unique_ptr<WasmCode>& b) {
      DCHECK(a);
      DCHECK(b);
      return a->instructions().start() < b->instructions().start();
    }
  };

  static base::AtomicNumber<uint32_t> next_id_;
  NativeModule(const NativeModule&) = delete;
  NativeModule& operator=(const NativeModule&) = delete;
  NativeModule(uint32_t num_functions, uint32_t num_imports,
               bool can_request_more, VirtualMemory* vmem,
               WasmCodeManager* code_manager);

  WasmCode* AddAnonymousCode(Handle<Code>, WasmCode::Kind kind);
  Address AllocateForCode(size_t size);

  // Primitive for adding code to the native module. All code added to a native
  // module is owned by that module. Various callers get to decide on how the
  // code is obtained (CodeDesc vs, as a point in time, Code*), the kind,
  // whether it has an index or is anonymous, etc.
  WasmCode* AddOwnedCode(Vector<const byte> orig_instructions,
                         std::unique_ptr<const byte[]>&& reloc_info,
                         size_t reloc_size, Maybe<uint32_t> index,
                         WasmCode::Kind kind, size_t constant_pool_offset,
                         uint32_t stack_slots, size_t safepoint_table_offset,
                         std::shared_ptr<ProtectedInstructions>,
                         bool is_liftoff = false);
  void SetCodeTable(uint32_t, wasm::WasmCode*);
  WasmCode* CloneCode(const WasmCode*);
  bool CloneTrampolinesAndStubs(const NativeModule* other);
  WasmCode* Lookup(Address);
  Address GetLocalAddressFor(Handle<Code>);
  Address CreateTrampolineTo(Handle<Code>);

  std::vector<std::unique_ptr<WasmCode>> owned_code_;
  std::unordered_map<uint32_t, WasmCode*> exported_wasm_to_wasm_wrappers_;

  WasmCodeUniquePtrComparer owned_code_comparer_;

  std::vector<WasmCode*> code_table_;
  uint32_t num_imported_functions_;

  std::unordered_map<Address, Address, AddressHasher> trampolines_;
  std::unordered_map<uint32_t, WasmCode*> stubs_;

  DisjointAllocationPool free_memory_;
  DisjointAllocationPool allocated_memory_;
  std::list<VirtualMemory> owned_memory_;
  WasmCodeManager* wasm_code_manager_;
  wasm::WasmCode* lazy_builtin_ = nullptr;
  base::Mutex allocation_mutex_;
  Handle<WasmCompiledModule> compiled_module_;
  size_t committed_memory_ = 0;
  bool can_request_more_memory_;

  // Specialization data that needs to be serialized and cloned.
  // Keeping it groupped together because it makes cloning of all these
  // elements a 1 line copy.
  struct {
    std::vector<wasm::GlobalHandleAddress> function_tables;
    std::vector<wasm::GlobalHandleAddress> signature_tables;
    std::vector<wasm::GlobalHandleAddress> empty_function_tables;
    std::vector<wasm::GlobalHandleAddress> empty_signature_tables;
  } specialization_data_;
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
  std::unique_ptr<NativeModule> NewNativeModule(const WasmModule&);
  std::unique_ptr<NativeModule> NewNativeModule(size_t memory_estimate,
                                                uint32_t num_functions,
                                                uint32_t num_imported_functions,
                                                bool can_request_more);

  WasmCode* LookupCode(Address pc) const;
  WasmCode* GetCodeFromStartAddress(Address pc) const;
  intptr_t remaining_uncommitted() const;

 private:
  friend class NativeModule;

  WasmCodeManager(const WasmCodeManager&) = delete;
  WasmCodeManager& operator=(const WasmCodeManager&) = delete;
  void TryAllocate(size_t size, VirtualMemory*, void* hint = nullptr);
  bool Commit(Address, size_t);
  // Currently, we uncommit a whole module, so all we need is account
  // for the freed memory size. We do that in FreeNativeModuleMemories.
  // There's no separate Uncommit.

  void FreeNativeModuleMemories(NativeModule*);
  void Free(VirtualMemory* mem);
  void AssignRanges(void* start, void* end, NativeModule*);
  size_t GetAllocationChunk(const WasmModule& module);
  bool WouldGCHelp() const;

  std::map<Address, std::pair<Address, NativeModule*>> lookup_map_;
  // count of NativeModules not yet collected. Helps determine if it's
  // worth requesting a GC on memory pressure.
  size_t active_ = 0;
  base::AtomicNumber<intptr_t> remaining_uncommitted_;
  v8::Isolate* isolate_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8
#endif

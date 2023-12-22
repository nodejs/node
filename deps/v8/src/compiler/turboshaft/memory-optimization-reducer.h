// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_MEMORY_OPTIMIZATION_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_MEMORY_OPTIMIZATION_REDUCER_H_

#include "src/base/template-utils.h"
#include "src/builtins/builtins.h"
#include "src/codegen/external-reference.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/phase.h"
#include "src/compiler/turboshaft/utils.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

const TSCallDescriptor* CreateAllocateBuiltinDescriptor(Zone* zone,
                                                        Isolate* isolate);

// The main purpose of memory optimization is folding multiple allocations into
// one. For this, the first allocation reserves additional space, that is
// consumed by subsequent allocations, which only move the allocation top
// pointer and are therefore guaranteed to succeed. Another nice side-effect of
// allocation folding is that more stores are performed on the most recent
// allocation, which allows us to eliminate the write barrier for the store.
//
// This analysis works by keeping track of the most recent non-folded
// allocation, as well as the number of bytes this allocation needs to reserve
// to satisfy all subsequent allocations.
// We can do write barrier elimination across loops if the loop does not contain
// any potentially allocating operations.
struct MemoryAnalyzer {
  enum class AllocationFolding { kDoAllocationFolding, kDontAllocationFolding };

  Zone* phase_zone;
  const Graph& input_graph;
  AllocationFolding allocation_folding;
  MemoryAnalyzer(Zone* phase_zone, const Graph& input_graph,
                 AllocationFolding allocation_folding)
      : phase_zone(phase_zone),
        input_graph(input_graph),
        allocation_folding(allocation_folding) {}

  struct BlockState {
    const AllocateOp* last_allocation = nullptr;
    base::Optional<uint32_t> reserved_size = base::nullopt;

    bool operator!=(const BlockState& other) {
      return last_allocation != other.last_allocation ||
             reserved_size != other.reserved_size;
    }
  };
  FixedSidetable<base::Optional<BlockState>, BlockIndex> block_states{
      input_graph.block_count(), phase_zone};
  ZoneUnorderedMap<const AllocateOp*, const AllocateOp*> folded_into{
      phase_zone};
  ZoneUnorderedSet<OpIndex> skipped_write_barriers{phase_zone};
  ZoneUnorderedMap<const AllocateOp*, uint32_t> reserved_size{phase_zone};
  BlockIndex current_block = BlockIndex(0);
  BlockState state;

  bool SkipWriteBarrier(const Operation& object) {
    if (ShouldSkipOptimizationStep()) return false;
    if (state.last_allocation == nullptr ||
        state.last_allocation->type != AllocationType::kYoung) {
      return false;
    }
    if (state.last_allocation == &object) {
      return true;
    }
    if (!object.Is<AllocateOp>()) return false;
    auto it = folded_into.find(&object.Cast<AllocateOp>());
    return it != folded_into.end() && it->second == state.last_allocation;
  }

  bool IsFoldedAllocation(OpIndex op) {
    return folded_into.count(
        input_graph.Get(op).template TryCast<AllocateOp>());
  }

  base::Optional<uint32_t> ReservedSize(OpIndex alloc) {
    if (auto it = reserved_size.find(
            input_graph.Get(alloc).template TryCast<AllocateOp>());
        it != reserved_size.end()) {
      return it->second;
    }
    return base::nullopt;
  }

  void Run();

  void Process(const Operation& op);
  void ProcessBlockTerminator(const Operation& op);
  void ProcessAllocation(const AllocateOp& alloc);
  void ProcessStore(OpIndex store, OpIndex object);
  void MergeCurrentStateIntoSuccessor(const Block* successor);
};

template <class Next>
class MemoryOptimizationReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE()

  void Analyze() {
    analyzer_.emplace(
        __ phase_zone(), __ input_graph(),
        PipelineData::Get().info()->allocation_folding()
            ? MemoryAnalyzer::AllocationFolding::kDoAllocationFolding
            : MemoryAnalyzer::AllocationFolding::kDontAllocationFolding);
    analyzer_->Run();
    Next::Analyze();
  }

  OpIndex REDUCE(Store)(OpIndex base, OpIndex index, OpIndex value,
                        StoreOp::Kind kind, MemoryRepresentation stored_rep,
                        WriteBarrierKind write_barrier, int32_t offset,
                        uint8_t element_scale,
                        bool maybe_initializing_or_transitioning,
                        IndirectPointerTag indirect_pointer_tag) {
    if (!ShouldSkipOptimizationStep() &&
        analyzer_->skipped_write_barriers.count(
            __ current_operation_origin())) {
      write_barrier = WriteBarrierKind::kNoWriteBarrier;
    }
    return Next::ReduceStore(base, index, value, kind, stored_rep,
                             write_barrier, offset, element_scale,
                             maybe_initializing_or_transitioning,
                             indirect_pointer_tag);
  }

  OpIndex REDUCE(Allocate)(OpIndex size, AllocationType type) {
    DCHECK_EQ(type, any_of(AllocationType::kYoung, AllocationType::kOld));

    if (v8_flags.single_generation && type == AllocationType::kYoung) {
      type = AllocationType::kOld;
    }

    OpIndex top_address;
    if (isolate_ != nullptr) {
      top_address = __ ExternalConstant(
          type == AllocationType::kYoung
              ? ExternalReference::new_space_allocation_top_address(isolate_)
              : ExternalReference::old_space_allocation_top_address(isolate_));
    } else {
      // Wasm mode: producing isolate-independent code, loading the isolate
      // address at runtime.
#if V8_ENABLE_WEBASSEMBLY
      V<WasmInstanceObject> instance_node = __ WasmInstanceParameter();
      int top_address_offset =
          type == AllocationType::kYoung
              ? WasmInstanceObject::kNewAllocationTopAddressOffset
              : WasmInstanceObject::kOldAllocationTopAddressOffset;
      top_address =
          __ Load(instance_node, LoadOp::Kind::TaggedBase().Immutable(),
                  MemoryRepresentation::PointerSized(), top_address_offset);
#else
      UNREACHABLE();
#endif  // V8_ENABLE_WEBASSEMBLY
    }

    if (analyzer_->IsFoldedAllocation(__ current_operation_origin())) {
      DCHECK_NE(__ GetVariable(top(type)), OpIndex::Invalid());
      OpIndex obj_addr = __ GetVariable(top(type));
      __ SetVariable(top(type), __ PointerAdd(__ GetVariable(top(type)), size));
      __ StoreOffHeap(top_address, __ GetVariable(top(type)),
                      MemoryRepresentation::PointerSized());
      return __ BitcastWordPtrToTagged(
          __ PointerAdd(obj_addr, __ IntPtrConstant(kHeapObjectTag)));
    }

    __ SetVariable(
        top(type),
        __ LoadOffHeap(top_address, MemoryRepresentation::PointerSized()));

    OpIndex allocate_builtin;
    if (isolate_ != nullptr) {
      if (type == AllocationType::kYoung) {
        allocate_builtin =
            __ BuiltinCode(Builtin::kAllocateInYoungGeneration, isolate_);
      } else {
        allocate_builtin =
            __ BuiltinCode(Builtin::kAllocateInOldGeneration, isolate_);
      }
    } else {
      // This lowering is used by Wasm, where we compile isolate-independent
      // code. Builtin calls simply encode the target builtin ID, which will
      // be patched to the builtin's address later.
#if V8_ENABLE_WEBASSEMBLY
      Builtin builtin;
      if (type == AllocationType::kYoung) {
        builtin = Builtin::kAllocateInYoungGeneration;
      } else {
        builtin = Builtin::kAllocateInOldGeneration;
      }
      static_assert(std::is_same<Smi, BuiltinPtr>(), "BuiltinPtr must be Smi");
      allocate_builtin = __ NumberConstant(static_cast<int>(builtin));
#else
      UNREACHABLE();
#endif
    }

    Block* call_runtime = __ NewBlock();
    Block* done = __ NewBlock();

    OpIndex limit_address = GetLimitAddress(type);
    OpIndex limit =
        __ LoadOffHeap(limit_address, MemoryRepresentation::PointerSized());

    // If the allocation size is not statically known or is known to be larger
    // than kMaxRegularHeapObjectSize, do not update {top(type)} in case of a
    // runtime call. This is needed because we cannot allocation-fold large and
    // normal-sized objects.
    uint64_t constant_size{};
    if (!__ matcher().MatchWordConstant(
            size, WordRepresentation::PointerSized(), &constant_size) ||
        constant_size > kMaxRegularHeapObjectSize) {
      Variable result =
          __ NewLoopInvariantVariable(RegisterRepresentation::Tagged());
      if (!constant_size) {
        // Check if we can do bump pointer allocation here.
        OpIndex top_value = __ GetVariable(top(type));
        __ SetVariable(result,
                       __ BitcastWordPtrToTagged(__ WordPtrAdd(
                           top_value, __ IntPtrConstant(kHeapObjectTag))));
        OpIndex new_top = __ PointerAdd(top_value, size);
        __ GotoIfNot(LIKELY(__ UintPtrLessThan(new_top, limit)), call_runtime);
        __ GotoIfNot(LIKELY(__ UintPtrLessThan(
                         size, __ IntPtrConstant(kMaxRegularHeapObjectSize))),
                     call_runtime);
        __ SetVariable(top(type), new_top);
        __ StoreOffHeap(top_address, new_top,
                        MemoryRepresentation::PointerSized());
        __ Goto(done);
      }
      if (constant_size || __ Bind(call_runtime)) {
        __ SetVariable(result, __ Call(allocate_builtin, {size},
                                       AllocateBuiltinDescriptor()));
        __ Goto(done);
      }

      __ BindReachable(done);
      return __ GetVariable(result);
    }

    OpIndex reservation_size;
    if (auto c = analyzer_->ReservedSize(__ current_operation_origin())) {
      reservation_size = __ UintPtrConstant(*c);
    } else {
      reservation_size = size;
    }
    // Check if we can do bump pointer allocation here.
    bool reachable =
        __ GotoIfNot(__ UintPtrLessThan(
                         size, __ IntPtrConstant(kMaxRegularHeapObjectSize)),
                     call_runtime, BranchHint::kTrue) !=
        ConditionalGotoStatus::kGotoDestination;
    if (reachable) {
      __ Branch(__ UintPtrLessThan(
                    __ PointerAdd(__ GetVariable(top(type)), reservation_size),
                    limit),
                done, call_runtime, BranchHint::kTrue);
    }

    // Call the runtime if bump pointer area exhausted.
    if (__ Bind(call_runtime)) {
      OpIndex allocated = __ Call(allocate_builtin, {reservation_size},
                                  AllocateBuiltinDescriptor());
      __ SetVariable(top(type),
                     __ PointerSub(__ BitcastTaggedToWord(allocated),
                                   __ IntPtrConstant(kHeapObjectTag)));
      __ Goto(done);
    }

    __ BindReachable(done);
    // Compute the new top and write it back.
    OpIndex obj_addr = __ GetVariable(top(type));
    __ SetVariable(top(type), __ PointerAdd(__ GetVariable(top(type)), size));
    __ StoreOffHeap(top_address, __ GetVariable(top(type)),
                    MemoryRepresentation::PointerSized());
    return __ BitcastWordPtrToTagged(
        __ PointerAdd(obj_addr, __ IntPtrConstant(kHeapObjectTag)));
  }

  OpIndex REDUCE(DecodeExternalPointer)(OpIndex handle,
                                        ExternalPointerTag tag) {
#ifdef V8_ENABLE_SANDBOX
    // Decode loaded external pointer.
    V<WordPtr> table;
    if (isolate_ != nullptr) {
      // Here we access the external pointer table through an ExternalReference.
      // Alternatively, we could also hardcode the address of the table since it
      // is never reallocated. However, in that case we must be able to
      // guarantee that the generated code is never executed under a different
      // Isolate, as that would allow access to external objects from different
      // Isolates. It also would break if the code is serialized/deserialized at
      // some point.
      V<WordPtr> table_address =
          IsSharedExternalPointerType(tag)
              ? __
                LoadOffHeap(
                    __ ExternalConstant(
                        ExternalReference::
                            shared_external_pointer_table_address_address(
                                isolate_)),
                    MemoryRepresentation::PointerSized())
              : __ ExternalConstant(
                    ExternalReference::external_pointer_table_address(
                        isolate_));
      table = __ LoadOffHeap(table_address,
                             Internals::kExternalPointerTableBasePointerOffset,
                             MemoryRepresentation::PointerSized());
    } else {
#if V8_ENABLE_WEBASSEMBLY
      V<WordPtr> isolate_root = __ LoadRootRegister();
      if (IsSharedExternalPointerType(tag)) {
        V<WordPtr> table_address =
            __ Load(isolate_root, LoadOp::Kind::RawAligned(),
                    MemoryRepresentation::PointerSized(),
                    IsolateData::shared_external_pointer_table_offset());
        table = __ Load(table_address, LoadOp::Kind::RawAligned(),
                        MemoryRepresentation::PointerSized(),
                        Internals::kExternalPointerTableBasePointerOffset);
      } else {
        table = __ Load(isolate_root, LoadOp::Kind::RawAligned(),
                        MemoryRepresentation::PointerSized(),
                        IsolateData::external_pointer_table_offset() +
                            Internals::kExternalPointerTableBasePointerOffset);
      }
#else
      UNREACHABLE();
#endif
    }

    OpIndex index = __ ShiftRightLogical(handle, kExternalPointerIndexShift,
                                         WordRepresentation::Word32());
    OpIndex pointer = __ LoadOffHeap(table, __ ChangeUint32ToUint64(index), 0,
                                     MemoryRepresentation::PointerSized());
    pointer = __ Word64BitwiseAnd(pointer, __ Word64Constant(~tag));
    return pointer;
#else   // V8_ENABLE_SANDBOX
    UNREACHABLE();
#endif  // V8_ENABLE_SANDBOX
  }

 private:
  base::Optional<MemoryAnalyzer> analyzer_;
  Isolate* isolate_ = PipelineData::Get().isolate();
  const TSCallDescriptor* allocate_builtin_descriptor_ = nullptr;
  base::Optional<Variable> top_[2];

  static_assert(static_cast<int>(AllocationType::kYoung) == 0);
  static_assert(static_cast<int>(AllocationType::kOld) == 1);
  Variable top(AllocationType type) {
    DCHECK(type == AllocationType::kYoung || type == AllocationType::kOld);
    if (V8_UNLIKELY(!top_[static_cast<int>(type)].has_value())) {
      top_[static_cast<int>(type)].emplace(
          __ NewLoopInvariantVariable(RegisterRepresentation::PointerSized()));
    }
    return top_[static_cast<int>(type)].value();
  }

  const TSCallDescriptor* AllocateBuiltinDescriptor() {
    if (allocate_builtin_descriptor_ == nullptr) {
      allocate_builtin_descriptor_ =
          CreateAllocateBuiltinDescriptor(__ graph_zone(), isolate_);
    }
    return allocate_builtin_descriptor_;
  }

  OpIndex GetLimitAddress(AllocationType type) {
    OpIndex limit_address;
    if (isolate_ != nullptr) {
      limit_address = __ ExternalConstant(
          type == AllocationType::kYoung
              ? ExternalReference::new_space_allocation_limit_address(isolate_)
              : ExternalReference::old_space_allocation_limit_address(
                    isolate_));
    } else {
      // Wasm mode: producing isolate-independent code, loading the isolate
      // address at runtime.
#if V8_ENABLE_WEBASSEMBLY
      V<WasmInstanceObject> instance_node = __ WasmInstanceParameter();
      int limit_address_offset =
          type == AllocationType::kYoung
              ? WasmInstanceObject::kNewAllocationLimitAddressOffset
              : WasmInstanceObject::kOldAllocationLimitAddressOffset;
      limit_address =
          __ Load(instance_node, LoadOp::Kind::TaggedBase(),
                  MemoryRepresentation::PointerSized(), limit_address_offset);
#else
      UNREACHABLE();
#endif  // V8_ENABLE_WEBASSEMBLY
    }
    return limit_address;
  }
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_MEMORY_OPTIMIZATION_REDUCER_H_

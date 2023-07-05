// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_MEMORY_OPTIMIZATION_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_MEMORY_OPTIMIZATION_REDUCER_H_

#include "src/base/template-utils.h"
#include "src/builtins/builtins.h"
#include "src/codegen/external-reference.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/utils.h"

namespace v8::internal::compiler::turboshaft {

const TSCallDescriptor* CreateAllocateBuiltinDescriptor(Zone* zone);

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
  Zone* phase_zone;
  const Graph& input_graph;
  MemoryAnalyzer(Zone* phase_zone, const Graph& input_graph)
      : phase_zone(phase_zone), input_graph(input_graph) {}

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

struct MemoryOptimizationReducerArgs {
  Isolate* isolate;
};

template <class Next>
class MemoryOptimizationReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE()

  using ArgT = base::append_tuple_type<typename Next::ArgT,
                                       MemoryOptimizationReducerArgs>;

  template <class... Args>
  explicit MemoryOptimizationReducer(const std::tuple<Args...>& args)
      : Next(args),
        isolate_(std::get<MemoryOptimizationReducerArgs>(args).isolate) {}

  void Analyze() {
    analyzer_.emplace(Asm().phase_zone(), Asm().input_graph());
    analyzer_->Run();
    Next::Analyze();
  }

  OpIndex ReduceStore(OpIndex base, OpIndex index, OpIndex value,
                      StoreOp::Kind kind, MemoryRepresentation stored_rep,
                      WriteBarrierKind write_barrier, int32_t offset,
                      uint8_t element_scale) {
    if (!ShouldSkipOptimizationStep() &&
        analyzer_->skipped_write_barriers.count(
            Asm().current_operation_origin())) {
      write_barrier = WriteBarrierKind::kNoWriteBarrier;
    }
    return Next::ReduceStore(base, index, value, kind, stored_rep,
                             write_barrier, offset, element_scale);
  }

  OpIndex ReduceAllocate(OpIndex size, AllocationType type,
                         AllowLargeObjects allow_large_objects) {
    DCHECK_EQ(type, any_of(AllocationType::kYoung, AllocationType::kOld));

    if (v8_flags.single_generation && type == AllocationType::kYoung) {
      type = AllocationType::kOld;
    }

    OpIndex top_address = Asm().ExternalConstant(
        type == AllocationType::kYoung
            ? ExternalReference::new_space_allocation_top_address(isolate_)
            : ExternalReference::old_space_allocation_top_address(isolate_));
    Variable top =
        Asm().NewFreshVariable(RegisterRepresentation::PointerSized());
    Asm().Set(top, Asm().LoadOffHeap(top_address,
                                     MemoryRepresentation::PointerSized()));

    if (analyzer_->IsFoldedAllocation(Asm().current_operation_origin())) {
      Asm().StoreOffHeap(top_address, Asm().PointerAdd(Asm().Get(top), size),
                         MemoryRepresentation::PointerSized());
      return Asm().BitcastWordToTagged(Asm().PointerAdd(
          Asm().Get(top), Asm().IntPtrConstant(kHeapObjectTag)));
    }

    OpIndex allocate_builtin;
    if (type == AllocationType::kYoung) {
      if (allow_large_objects == AllowLargeObjects::kTrue) {
        allocate_builtin =
            Asm().BuiltinCode(Builtin::kAllocateInYoungGeneration, isolate_);
      } else {
        allocate_builtin = Asm().BuiltinCode(
            Builtin::kAllocateRegularInYoungGeneration, isolate_);
      }
    } else {
      if (allow_large_objects == AllowLargeObjects::kTrue) {
        allocate_builtin =
            Asm().BuiltinCode(Builtin::kAllocateInOldGeneration, isolate_);
      } else {
        allocate_builtin = Asm().BuiltinCode(
            Builtin::kAllocateRegularInOldGeneration, isolate_);
      }
    }

    Block* call_runtime = Asm().NewBlock();
    Block* done = Asm().NewBlock();

    OpIndex limit_address = Asm().ExternalConstant(
        type == AllocationType::kYoung
            ? ExternalReference::new_space_allocation_limit_address(isolate_)
            : ExternalReference::old_space_allocation_limit_address(isolate_));
    OpIndex limit =
        Asm().LoadOffHeap(limit_address, MemoryRepresentation::PointerSized());

    OpIndex reservation_size;
    if (auto c = analyzer_->ReservedSize(Asm().current_operation_origin())) {
      reservation_size = Asm().UintPtrConstant(*c);
    } else {
      reservation_size = size;
    }
    // Check if we can do bump pointer allocation here.
    bool reachable = true;
    if (allow_large_objects == AllowLargeObjects::kTrue) {
      reachable = Asm().GotoIfNot(
          Asm().UintPtrLessThan(
              size, Asm().IntPtrConstant(kMaxRegularHeapObjectSize)),
          call_runtime, BranchHint::kTrue);
    }
    if (reachable) {
      Asm().Branch(
          Asm().UintPtrLessThan(
              Asm().PointerAdd(Asm().Get(top), reservation_size), limit),
          done, call_runtime, BranchHint::kTrue);
    }

    // Call the runtime if bump pointer area exhausted.
    if (Asm().Bind(call_runtime)) {
      OpIndex allocated = Asm().Call(allocate_builtin, {reservation_size},
                                     AllocateBuiltinDescriptor());
      Asm().Set(top, Asm().PointerSub(Asm().BitcastTaggedToWord(allocated),
                                      Asm().IntPtrConstant(kHeapObjectTag)));
      Asm().Goto(done);
    }

    Asm().BindReachable(done);
    // Compute the new top and write it back.
    Asm().StoreOffHeap(top_address, Asm().PointerAdd(Asm().Get(top), size),
                       MemoryRepresentation::PointerSized());
    return Asm().BitcastWordToTagged(
        Asm().PointerAdd(Asm().Get(top), Asm().IntPtrConstant(kHeapObjectTag)));
  }

  OpIndex ReduceDecodeExternalPointer(OpIndex handle, ExternalPointerTag tag) {
#ifdef V8_ENABLE_SANDBOX
    // Decode loaded external pointer.
    //
    // Here we access the external pointer table through an ExternalReference.
    // Alternatively, we could also hardcode the address of the table since it
    // is never reallocated. However, in that case we must be able to guarantee
    // that the generated code is never executed under a different Isolate, as
    // that would allow access to external objects from different Isolates. It
    // also would break if the code is serialized/deserialized at some point.
    OpIndex table_address =
        IsSharedExternalPointerType(tag)
            ? Asm().LoadOffHeap(
                  Asm().ExternalConstant(
                      ExternalReference::
                          shared_external_pointer_table_address_address(
                              isolate_)),
                  MemoryRepresentation::PointerSized())
            : Asm().ExternalConstant(
                  ExternalReference::external_pointer_table_address(isolate_));
    OpIndex table = Asm().LoadOffHeap(
        table_address, Internals::kExternalPointerTableBufferOffset,
        MemoryRepresentation::PointerSized());
    OpIndex index = Asm().ShiftRightLogical(handle, kExternalPointerIndexShift,
                                            WordRepresentation::Word32());
    OpIndex pointer =
        Asm().LoadOffHeap(table, Asm().ChangeUint32ToUint64(index), 0,
                          MemoryRepresentation::PointerSized());
    pointer = Asm().Word64BitwiseAnd(pointer, Asm().Word64Constant(~tag));
    return pointer;
#else   // V8_ENABLE_SANDBOX
    UNREACHABLE();
#endif  // V8_ENABLE_SANDBOX
  }

 private:
  base::Optional<MemoryAnalyzer> analyzer_;
  Isolate* isolate_;
  const TSCallDescriptor* allocate_builtin_descriptor_ = nullptr;

  const TSCallDescriptor* AllocateBuiltinDescriptor() {
    if (allocate_builtin_descriptor_ == nullptr) {
      allocate_builtin_descriptor_ =
          CreateAllocateBuiltinDescriptor(Asm().graph_zone());
    }
    return allocate_builtin_descriptor_;
  }
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_MEMORY_OPTIMIZATION_REDUCER_H_

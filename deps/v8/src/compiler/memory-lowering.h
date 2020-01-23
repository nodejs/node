// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_MEMORY_LOWERING_H_
#define V8_COMPILER_MEMORY_LOWERING_H_

#include "src/compiler/graph-assembler.h"
#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
struct ElementAccess;
class Graph;
class JSGraph;
class MachineOperatorBuilder;
class Node;
class Operator;

// Provides operations to lower all simplified memory access and allocation
// related nodes (i.e. Allocate, LoadField, StoreField and friends) to machine
// operators.
class MemoryLowering final : public Reducer {
 public:
  enum class AllocationFolding { kDoAllocationFolding, kDontAllocationFolding };
  class AllocationGroup;

  // An allocation state is propagated on the effect paths through the graph.
  class AllocationState final : public ZoneObject {
   public:
    static AllocationState const* Empty(Zone* zone) {
      return new (zone) AllocationState();
    }
    static AllocationState const* Closed(AllocationGroup* group, Node* effect,
                                         Zone* zone) {
      return new (zone) AllocationState(group, effect);
    }
    static AllocationState const* Open(AllocationGroup* group, intptr_t size,
                                       Node* top, Node* effect, Zone* zone) {
      return new (zone) AllocationState(group, size, top, effect);
    }

    bool IsYoungGenerationAllocation() const;

    AllocationGroup* group() const { return group_; }
    Node* top() const { return top_; }
    Node* effect() const { return effect_; }
    intptr_t size() const { return size_; }

   private:
    AllocationState();
    explicit AllocationState(AllocationGroup* group, Node* effect);
    AllocationState(AllocationGroup* group, intptr_t size, Node* top,
                    Node* effect);

    AllocationGroup* const group_;
    // The upper bound of the combined allocated object size on the current path
    // (max int if allocation folding is impossible on this path).
    intptr_t const size_;
    Node* const top_;
    Node* const effect_;

    DISALLOW_COPY_AND_ASSIGN(AllocationState);
  };

  using WriteBarrierAssertFailedCallback = std::function<void(
      Node* node, Node* object, const char* name, Zone* temp_zone)>;

  MemoryLowering(
      JSGraph* jsgraph, Zone* zone, PoisoningMitigationLevel poisoning_level,
      AllocationFolding allocation_folding =
          AllocationFolding::kDontAllocationFolding,
      WriteBarrierAssertFailedCallback callback = [](Node*, Node*, const char*,
                                                     Zone*) { UNREACHABLE(); },
      const char* function_debug_name = nullptr);
  ~MemoryLowering() = default;

  const char* reducer_name() const override { return "MemoryReducer"; }

  // Perform memory lowering reduction on the given Node.
  Reduction Reduce(Node* node) override;

  // Specific reducers for each optype to enable keeping track of
  // AllocationState by the MemoryOptimizer.
  Reduction ReduceAllocateRaw(Node* node, AllocationType allocation_type,
                              AllowLargeObjects allow_large_objects,
                              AllocationState const** state);
  Reduction ReduceLoadFromObject(Node* node);
  Reduction ReduceLoadElement(Node* node);
  Reduction ReduceLoadField(Node* node);
  Reduction ReduceStoreToObject(Node* node,
                                AllocationState const* state = nullptr);
  Reduction ReduceStoreElement(Node* node,
                               AllocationState const* state = nullptr);
  Reduction ReduceStoreField(Node* node,
                             AllocationState const* state = nullptr);
  Reduction ReduceStore(Node* node, AllocationState const* state = nullptr);

 private:
  Reduction ReduceAllocateRaw(Node* node);
  WriteBarrierKind ComputeWriteBarrierKind(Node* node, Node* object,
                                           Node* value,
                                           AllocationState const* state,
                                           WriteBarrierKind);
  Node* ComputeIndex(ElementAccess const& access, Node* node);
  bool NeedsPoisoning(LoadSensitivity load_sensitivity) const;

  Graph* graph() const;
  Isolate* isolate() const;
  Zone* zone() const { return zone_; }
  JSGraph* jsgraph() const { return jsgraph_; }
  CommonOperatorBuilder* common() const;
  MachineOperatorBuilder* machine() const;
  GraphAssembler* gasm() { return &graph_assembler_; }

  SetOncePointer<const Operator> allocate_operator_;
  JSGraph* const jsgraph_;
  Zone* zone_;
  GraphAssembler graph_assembler_;
  AllocationFolding allocation_folding_;
  PoisoningMitigationLevel poisoning_level_;
  WriteBarrierAssertFailedCallback write_barrier_assert_failed_;
  const char* function_debug_name_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MemoryLowering);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_MEMORY_LOWERING_H_

// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_MEMORY_OPTIMIZER_H_
#define V8_COMPILER_MEMORY_OPTIMIZER_H_

#include "src/compiler/graph-assembler.h"
#include "src/zone/zone-containers.h"

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

// NodeIds are identifying numbers for nodes that can be used to index auxiliary
// out-of-line data associated with each node.
typedef uint32_t NodeId;

// Lowers all simplified memory access and allocation related nodes (i.e.
// Allocate, LoadField, StoreField and friends) to machine operators.
// Performs allocation folding and store write barrier elimination
// implicitly.
class MemoryOptimizer final {
 public:
  enum class AllocationFolding { kDoAllocationFolding, kDontAllocationFolding };

  MemoryOptimizer(JSGraph* jsgraph, Zone* zone,
                  PoisoningMitigationLevel poisoning_enabled,
                  AllocationFolding allocation_folding);
  ~MemoryOptimizer() {}

  void Optimize();

 private:
  // An allocation group represents a set of allocations that have been folded
  // together.
  class AllocationGroup final : public ZoneObject {
   public:
    AllocationGroup(Node* node, PretenureFlag pretenure, Zone* zone);
    AllocationGroup(Node* node, PretenureFlag pretenure, Node* size,
                    Zone* zone);
    ~AllocationGroup() {}

    void Add(Node* object);
    bool Contains(Node* object) const;
    bool IsNewSpaceAllocation() const { return pretenure() == NOT_TENURED; }

    PretenureFlag pretenure() const { return pretenure_; }
    Node* size() const { return size_; }

   private:
    ZoneSet<NodeId> node_ids_;
    PretenureFlag const pretenure_;
    Node* const size_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(AllocationGroup);
  };

  // An allocation state is propagated on the effect paths through the graph.
  class AllocationState final : public ZoneObject {
   public:
    static AllocationState const* Empty(Zone* zone) {
      return new (zone) AllocationState();
    }
    static AllocationState const* Closed(AllocationGroup* group, Zone* zone) {
      return new (zone) AllocationState(group);
    }
    static AllocationState const* Open(AllocationGroup* group, int size,
                                       Node* top, Zone* zone) {
      return new (zone) AllocationState(group, size, top);
    }

    bool IsNewSpaceAllocation() const;

    AllocationGroup* group() const { return group_; }
    Node* top() const { return top_; }
    int size() const { return size_; }

   private:
    AllocationState();
    explicit AllocationState(AllocationGroup* group);
    AllocationState(AllocationGroup* group, int size, Node* top);

    AllocationGroup* const group_;
    // The upper bound of the combined allocated object size on the current path
    // (max int if allocation folding is impossible on this path).
    int const size_;
    Node* const top_;

    DISALLOW_COPY_AND_ASSIGN(AllocationState);
  };

  // An array of allocation states used to collect states on merges.
  typedef ZoneVector<AllocationState const*> AllocationStates;

  // We thread through tokens to represent the current state on a given effect
  // path through the graph.
  struct Token {
    Node* node;
    AllocationState const* state;
  };

  void VisitNode(Node*, AllocationState const*);
  void VisitAllocateRaw(Node*, AllocationState const*);
  void VisitCall(Node*, AllocationState const*);
  void VisitCallWithCallerSavedRegisters(Node*, AllocationState const*);
  void VisitLoadElement(Node*, AllocationState const*);
  void VisitLoadField(Node*, AllocationState const*);
  void VisitStoreElement(Node*, AllocationState const*);
  void VisitStoreField(Node*, AllocationState const*);
  void VisitOtherEffect(Node*, AllocationState const*);

  Node* ComputeIndex(ElementAccess const&, Node*);
  WriteBarrierKind ComputeWriteBarrierKind(Node* object,
                                           AllocationState const* state,
                                           WriteBarrierKind);

  AllocationState const* MergeStates(AllocationStates const& states);

  void EnqueueMerge(Node*, int, AllocationState const*);
  void EnqueueUses(Node*, AllocationState const*);
  void EnqueueUse(Node*, int, AllocationState const*);

  AllocationState const* empty_state() const { return empty_state_; }
  Graph* graph() const;
  Isolate* isolate() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  CommonOperatorBuilder* common() const;
  MachineOperatorBuilder* machine() const;
  Zone* zone() const { return zone_; }
  GraphAssembler* gasm() { return &graph_assembler_; }

  SetOncePointer<const Operator> allocate_operator_;
  JSGraph* const jsgraph_;
  AllocationState const* const empty_state_;
  ZoneMap<NodeId, AllocationStates> pending_;
  ZoneQueue<Token> tokens_;
  Zone* const zone_;
  GraphAssembler graph_assembler_;
  PoisoningMitigationLevel poisoning_enabled_;
  AllocationFolding allocation_folding_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MemoryOptimizer);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_MEMORY_OPTIMIZER_H_

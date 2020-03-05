// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_MEMORY_OPTIMIZER_H_
#define V8_COMPILER_MEMORY_OPTIMIZER_H_

#include "src/compiler/graph-assembler.h"
#include "src/compiler/memory-lowering.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

class TickCounter;

namespace compiler {

class JSGraph;
class Graph;

// NodeIds are identifying numbers for nodes that can be used to index auxiliary
// out-of-line data associated with each node.
using NodeId = uint32_t;

// Performs allocation folding and store write barrier elimination
// implicitly, while lowering all simplified memory access and allocation
// related nodes (i.e. Allocate, LoadField, StoreField and friends) to machine
// operators.
class MemoryOptimizer final {
 public:
  MemoryOptimizer(JSGraph* jsgraph, Zone* zone,
                  PoisoningMitigationLevel poisoning_level,
                  MemoryLowering::AllocationFolding allocation_folding,
                  const char* function_debug_name, TickCounter* tick_counter);
  ~MemoryOptimizer() = default;

  void Optimize();

 private:
  using AllocationState = MemoryLowering::AllocationState;

  // An array of allocation states used to collect states on merges.
  using AllocationStates = ZoneVector<AllocationState const*>;

  // We thread through tokens to represent the current state on a given effect
  // path through the graph.
  struct Token {
    Node* node;
    AllocationState const* state;
  };

  void VisitNode(Node*, AllocationState const*);
  void VisitAllocateRaw(Node*, AllocationState const*);
  void VisitCall(Node*, AllocationState const*);
  void VisitLoadFromObject(Node*, AllocationState const*);
  void VisitLoadElement(Node*, AllocationState const*);
  void VisitLoadField(Node*, AllocationState const*);
  void VisitStoreToObject(Node*, AllocationState const*);
  void VisitStoreElement(Node*, AllocationState const*);
  void VisitStoreField(Node*, AllocationState const*);
  void VisitStore(Node*, AllocationState const*);
  void VisitOtherEffect(Node*, AllocationState const*);

  AllocationState const* MergeStates(AllocationStates const& states);

  void EnqueueMerge(Node*, int, AllocationState const*);
  void EnqueueUses(Node*, AllocationState const*);
  void EnqueueUse(Node*, int, AllocationState const*);

  // Returns true if the AllocationType of the current AllocateRaw node that we
  // are visiting needs to be updated to kOld, due to propagation of tenuring
  // from outer to inner allocations.
  bool AllocationTypeNeedsUpdateToOld(Node* const user, const Edge edge);

  AllocationState const* empty_state() const { return empty_state_; }
  MemoryLowering* memory_lowering() { return &memory_lowering_; }
  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  Zone* zone() const { return zone_; }

  JSGraphAssembler graph_assembler_;
  MemoryLowering memory_lowering_;
  JSGraph* jsgraph_;
  AllocationState const* const empty_state_;
  ZoneMap<NodeId, AllocationStates> pending_;
  ZoneQueue<Token> tokens_;
  Zone* const zone_;
  TickCounter* const tick_counter_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MemoryOptimizer);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_MEMORY_OPTIMIZER_H_

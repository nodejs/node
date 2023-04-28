// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_DECOMPRESSION_OPTIMIZER_H_
#define V8_COMPILER_DECOMPRESSION_OPTIMIZER_H_

#include "src/compiler/common-operator.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-marker.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declare.
class Graph;

// DecompressionOptimizer purpose is to hide the distinction between 32 bit and
// 64 bit tagged values, while being able to use the compressed version of nodes
// whenever possible. Its scope is narrowed down to loads of TaggedPointer and
// AnyTagged (since TaggedSigned avoids full decompression always), and
// HeapConstants.

// DecompressionOptimizer will run only when pointer compression is enabled.

// The phase needs to be run when Machine are present in the graph, i.e
// at the very end of the pipeline. Also, since this phase may change
// the load's MachineRepresentation from Tagged to Compressed, it's best
// to run it as late as possible in order to keep the phases that know
// about Compressed MachineRepresentation to a minimum.

// As an example, if we Load a Tagged value only to Store it back again (i.e
// Load -> Store nodes, with the Load's value being the Store's value) we don't
// need to fully decompress it since the Store will ignore the top bits.
class V8_EXPORT_PRIVATE DecompressionOptimizer final {
 public:
  DecompressionOptimizer(Zone* zone, Graph* graph,
                         CommonOperatorBuilder* common,
                         MachineOperatorBuilder* machine);
  ~DecompressionOptimizer() = default;
  DecompressionOptimizer(const DecompressionOptimizer&) = delete;
  DecompressionOptimizer& operator=(const DecompressionOptimizer&) = delete;

  // Assign States to the nodes, and then change the node's Operator to use the
  // compressed version if possible.
  void Reduce();

 private:
  // State refers to the node's state as follows:
  // * kUnvisited === This node has yet to be visited.
  // * kOnly32BitsObserved === This node either has been visited, or is on
  // to_visit_. We couldn't find a node that observes the upper bits.
  // * kEverythingObserved === This node either has been visited, or is on
  // to_visit_. We found at least one node that observes the upper bits.
  enum class State : uint8_t {
    kUnvisited = 0,
    kOnly32BitsObserved,
    kEverythingObserved,
    kNumberOfStates
  };

  // Change node's op from HeapConstant to CompressedHeapConstant.
  void ChangeHeapConstant(Node* const node);

  // Change the phi's representation from Tagged to Compressed.
  void ChangePhi(Node* const node);

  // Change node's load into a compressed one.
  void ChangeLoad(Node* const node);

  // Go through the already marked nodes and changed the operation for the nodes
  // that can use compressed outputs.
  void ChangeNodes();

  // Goes through the nodes to mark them all as appropriate. It will visit each
  // node at most twice: only when the node was unvisited, then marked as
  // kOnly32BitsObserved and visited, and finally marked as kEverythingObserved
  // and visited.
  void MarkNodes();

  // Mark node's input as appropriate, according to node's opcode. Some input
  // State may be updated, and therefore has to be revisited.
  void MarkNodeInputs(Node* node);

  void MarkAddressingBase(Node* base);

  // Mark node's State to be state. We only do this if we have new information,
  // i.e either if:
  // * We are marking an unvisited node, or
  // * We are marking a node as needing 64 bits when we previously had the
  // information that it could output 32 bits. Also, we store the HeapConstant
  // and TaggedPointer and AnyTagged loads that have their state set as
  // kOnly32BitsObserved. If the node's state changes, we queue it for revisit.
  void MaybeMarkAndQueueForRevisit(Node* const node, State state);

  bool IsEverythingObserved(Node* const node) {
    return states_.Get(node) == State::kEverythingObserved;
  }

  bool IsOnly32BitsObserved(Node* const node) {
    return states_.Get(node) == State::kOnly32BitsObserved;
  }

  Graph* graph() const { return graph_; }
  CommonOperatorBuilder* common() const { return common_; }
  MachineOperatorBuilder* machine() const { return machine_; }

  Graph* const graph_;
  CommonOperatorBuilder* const common_;
  MachineOperatorBuilder* const machine_;
  NodeMarker<State> states_;
  // to_visit_ is a Deque but it's used as if it were a Queue. The reason why we
  // are using NodeDeque is because it attempts to reuse 'freed' zone memory
  // instead of always allocating a new region.
  NodeDeque to_visit_;
  // Contains the nodes that can be changed into a compressed version of
  // themselves. In a way, it functions as a NodeSet since each node will be
  // contained at most once. It's a Vector since we care about insertion speed.
  NodeVector compressed_candidate_nodes_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_DECOMPRESSION_OPTIMIZER_H_

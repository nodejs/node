// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_PHI_REPRESENTATION_SELECTOR_H_
#define V8_MAGLEV_MAGLEV_PHI_REPRESENTATION_SELECTOR_H_

#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph-builder.h"

namespace v8 {
namespace internal {
namespace maglev {

class Graph;

class MaglevPhiRepresentationSelector {
 public:
  explicit MaglevPhiRepresentationSelector(MaglevGraphBuilder* builder)
      : builder_(builder),
        new_nodes_current_block_start_(builder->zone()),
        new_nodes_current_block_end_(builder->zone()) {}

  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) { MergeNewNodesInBlock(current_block_); }
  void PreProcessBasicBlock(BasicBlock* block) {
    MergeNewNodesInBlock(current_block_);
    current_block_ = block;
  }

  void Process(Phi* node, const ProcessingState&);

  void Process(JumpLoop* node, const ProcessingState&) {
    FixLoopPhisBackedge(node->target());
  }

  template <class NodeT>
  void Process(NodeT* node, const ProcessingState&) {
    UpdateNodeInputs(node);
  }

 private:
  // Update the inputs of {phi} so that they all have {repr} representation, and
  // updates {phi}'s representation to {repr}.
  void ConvertTaggedPhiTo(Phi* phi, ValueRepresentation repr);

  // Since this pass changes the representation of Phis, it makes some untagging
  // operations outdated: if we've decided that a Phi should have Int32
  // representation, then we don't need to do a kCheckedSmiUntag before using
  // it. UpdateNodeInputs(n) removes such untagging from {n}'s input (and insert
  // new conversions if needed, from Int32 to Float64 for instance).
  template <class NodeT>
  void UpdateNodeInputs(NodeT* n) {
    NodeBase* node = static_cast<NodeBase*>(n);
    for (int i = 0; i < node->input_count(); i++) {
      ValueNode* input = node->input(i).node();
      if (IsUntagging(input->opcode()) && input->input(0).node()->Is<Phi>() &&
          input->input(0).node()->value_representation() !=
              ValueRepresentation::kTagged) {
        // Note that we're using `IsUntagging(opcode)` rather than
        // `input->properties().is_conversion` because truncations also need to
        // be updated, and they don't have the is_conversion property.
        node->change_input(i, GetInputReplacement(input));
      } else if (Phi* phi = input->TryCast<Phi>()) {
        // If the input is a Phi and it was used without any untagging, then we
        // need to retag it.
        if (!IsUntagging(n->opcode()) &&
            phi->value_representation() != ValueRepresentation::kTagged) {
          // If {n} is a conversion that isn't an untagging, then it has to have
          // been inserted during this phase, because it knows that {phi} isn't
          // tagged. As such, we don't do anything in that case.
          if (!n->properties().is_conversion()) {
            node->change_input(
                i, TagPhi(phi, current_block_, NewNodePosition::kStart));
          }
        }
      }
    }
  }

  void EnsurePhiInputsTagged(Phi* phi);

  // Returns true if {op} is an untagging node.
  bool IsUntagging(Opcode op);

  // GetInputReplacement should be called with {old_input} being an untagging
  // operation whose input is a Phi that we've decided shouldn't be tagged. Now
  // that the Phi isn't tagged, the untagging isn't needed anymore and should
  // instead be either the Phi directly, or a conversion of the Phi to something
  // else (a Int32ToFloat64 for instance).
  ValueNode* GetInputReplacement(ValueNode* old_conversion);

  // NewNodePosition is used to represent where a new node should be inserted:
  // at the start of a block (kStart), at the end of a block (kEnd).
  enum class NewNodePosition { kStart, kEnd };

  // Returns a tagged node that represents a tagged version of {phi}.
  ValueNode* TagPhi(Phi* phi, BasicBlock* block, NewNodePosition pos);

  ValueNode* AddNode(ValueNode* node, BasicBlock* block, NewNodePosition pos);

  // Merges the nodes from {new_nodes_current_block_start_} and
  // {new_nodes_current_block_end_} into their destinations.
  void MergeNewNodesInBlock(BasicBlock* block);

  // If {block} jumps back to the start of a loop header, FixLoopPhisBackedge
  // inserts the necessary tagging on the backedge of the loop Phis of the loop
  // header.
  void FixLoopPhisBackedge(BasicBlock* block);

  MaglevGraphBuilder* builder_ = nullptr;
  BasicBlock* current_block_ = nullptr;

  // {new_nodes_current_block_start_}, {new_nodes_current_block_end_} and
  // are used to store new nodes added by this pass, but to delay their
  // insertion into their destination, in order to not mutate the linked list of
  // nodes of the current block while also iterating it. Nodes in
  // {new_nodes_current_block_start_} and {new_nodes_current_block_end_} will be
  // inserted respectively at the begining and the end of the current block.
  ZoneVector<Node*> new_nodes_current_block_start_;
  ZoneVector<Node*> new_nodes_current_block_end_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_PHI_REPRESENTATION_SELECTOR_H_

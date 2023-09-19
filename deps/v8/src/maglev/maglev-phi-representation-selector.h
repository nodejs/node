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
  void Process(NodeT* node, const ProcessingState& state) {
    UpdateNodeInputs(node, state);
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
  void UpdateNodeInputs(NodeT* n, const ProcessingState& state) {
    NodeBase* node = static_cast<NodeBase*>(n);

    if (IsUntagging(n->opcode())) {
      if (node->input(0).node()->Is<Phi>() &&
          node->input(0).node()->value_representation() !=
              ValueRepresentation::kTagged) {
        DCHECK_EQ(node->input_count(), 1);
        // This untagging conversion is outdated, since its input has been
        // untagged. Depending on the conversion, it might need to be replaced
        // by another untagged->untagged conversion, or it might need to be
        // removed alltogether (or rather, replaced by an identity node).
        UpdateUntaggingOfPhi(n->template Cast<ValueNode>());
        return;
      }
    } else {
      UpdateNonUntaggingNodeInputs(n, state);
    }

    // It's important to check the properties of {node} rather than the static
    // properties of `NodeT`, because `UpdateUntaggingOfPhi` could have changed
    // the opcode of {node}, potentially converting a deopting node into a
    // non-deopting one.
    if (node->properties().can_eager_deopt()) {
      BypassIdentities(node->eager_deopt_info());
    }
    if (node->properties().can_lazy_deopt()) {
      BypassIdentities(node->lazy_deopt_info());
    }
  }

  template <class NodeT>
  void UpdateNonUntaggingNodeInputs(NodeT* n, const ProcessingState& state) {
    NodeBase* node = static_cast<NodeBase*>(n);

    // It would be bad to re-tag the input of an untagging node, so this
    // function should never be called on untagging nodes.
    DCHECK(!IsUntagging(n->opcode()));

    for (int i = 0; i < n->input_count(); i++) {
      ValueNode* input = node->input(i).node();
      if (input->Is<Identity>()) {
        // Bypassing the identity
        node->change_input(i, input->input(0).node());
      } else if (Phi* phi = input->TryCast<Phi>()) {
        // If the input is a Phi and it was used without any untagging, then
        // we need to retag it (with some additional checks/changes for some
        // nodes, cf the overload of UpdateNodePhiInput).
        UpdateNodePhiInput(n, phi, i, state);
      }
    }
  }

  void UpdateNodePhiInput(StoreTaggedFieldNoWriteBarrier* node, Phi* phi,
                          int input_index, const ProcessingState& state);
  void UpdateNodePhiInput(CheckedStoreSmiField* node, Phi* phi, int input_index,
                          const ProcessingState& state);
  void UpdateNodePhiInput(StoreFixedArrayElementNoWriteBarrier* node, Phi* phi,
                          int input_index, const ProcessingState& state);
  void UpdateNodePhiInput(CheckedStoreFixedArraySmiElement* node, Phi* phi,
                          int input_index, const ProcessingState& state);
  void UpdateNodePhiInput(NodeBase* node, Phi* phi, int input_index,
                          const ProcessingState& state);

  void EnsurePhiInputsTagged(Phi* phi);

  // Returns true if {op} is an untagging node.
  bool IsUntagging(Opcode op);

  // Updates {old_untagging} to reflect that its Phi input has been untagged and
  // that a different conversion is now needed.
  void UpdateUntaggingOfPhi(ValueNode* old_untagging);

  // NewNodePosition is used to represent where a new node should be inserted:
  // at the start of a block (kStart), or at the end of a block (kEnd).
  enum class NewNodePosition { kStart, kEnd };

  // Tags {phi} as a Smi.
  ValueNode* SmiTagPhi(Phi* phi, CheckedStoreSmiField* user_node,
                       const ProcessingState& state);
  ValueNode* SmiTagPhi(Phi* phi, CheckedStoreFixedArraySmiElement* user_node,
                       const ProcessingState& state);
  template <class ToNodeT, class FromNodeT>
  ValueNode* SmiTagPhi(Phi*, FromNodeT* user_node,
                       const ProcessingState& state);

  // Returns a tagged node that represents a tagged version of {phi}.
  ValueNode* EnsurePhiTagged(Phi* phi, BasicBlock* block, NewNodePosition pos);

  ValueNode* AddNode(ValueNode* node, BasicBlock* block, NewNodePosition pos);

  // Merges the nodes from {new_nodes_current_block_start_} and
  // {new_nodes_current_block_end_} into their destinations.
  void MergeNewNodesInBlock(BasicBlock* block);

  // If {block} is the start of a loop header, FixLoopPhisBackedge inserts the
  // necessary tagging on the backedge of the loop Phis of the loop header.
  // Additionally, if {block} contains untagged loop phis whose backedges have
  // been updated to Identity, FixLoopPhisBackedge unwraps those Identity.
  void FixLoopPhisBackedge(BasicBlock* block);

  // Replaces Identity nodes by their inputs in {deopt_info}
  template <typename DeoptInfoT>
  void BypassIdentities(const DeoptInfoT* deopt_info);

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

#ifdef DEBUG
  std::unordered_set<NodeBase*> new_nodes_;
#endif
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_PHI_REPRESENTATION_SELECTOR_H_

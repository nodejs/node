// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_PHI_REPRESENTATION_SELECTOR_H_
#define V8_MAGLEV_MAGLEV_PHI_REPRESENTATION_SELECTOR_H_

#include <optional>

#include "src/base/logging.h"
#include "src/base/small-vector.h"
#include "src/compiler/turboshaft/snapshot-table.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-reducer.h"

namespace v8 {
namespace internal {
namespace maglev {

class Graph;

// Returns true if {op} is an untagging node.
constexpr bool IsUntagging(Opcode op) {
  switch (op) {
    case Opcode::kCheckedSmiUntag:
    case Opcode::kUnsafeSmiUntag:
    case Opcode::kCheckedNumberToInt32:
    case Opcode::kCheckedObjectToIndex:
    case Opcode::kTruncateCheckedNumberOrOddballToInt32:
    case Opcode::kTruncateUnsafeNumberOrOddballToInt32:
    case Opcode::kCheckedNumberOrOddballToFloat64:
    case Opcode::kUncheckedNumberOrOddballToFloat64:
    case Opcode::kCheckedNumberOrOddballToHoleyFloat64:
      return true;
    default:
      return false;
  }
}

template <typename T>
concept IsUntaggingT = IsUntagging(NodeBase::opcode_of<T>);

class MaglevPhiRepresentationSelector {
  template <class Value>
  using SnapshotTable = compiler::turboshaft::SnapshotTable<Value>;
  using Key = SnapshotTable<ValueNode*>::Key;
  using Snapshot = SnapshotTable<ValueNode*>::Snapshot;

 public:
  explicit MaglevPhiRepresentationSelector(Graph* graph);

  void PreProcessGraph(Graph* graph) {
    if (v8_flags.trace_maglev_phi_untagging) {
      StdoutStream{} << "\nMaglevPhiRepresentationSelector\n";
    }
    graph_ = graph;
  }
  void PostProcessGraph(Graph* graph) {
    if (v8_flags.trace_maglev_phi_untagging) {
      StdoutStream{} << "\n";
    }
  }
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block);
  void PostProcessBasicBlock(BasicBlock* block);
  void PostPhiProcessing() {}

  enum ProcessPhiResult { kNone, kRetryOnChange, kChanged };
  ProcessPhiResult ProcessPhi(Phi* node);

  // The visitor method is a no-op since phis are processed in
  // PreProcessBasicBlock.
  ProcessResult Process(Phi* node, const ProcessingState&) {
    return ProcessResult::kContinue;
  }

  ProcessResult Process(JumpLoop* node, const ProcessingState&) {
    FixLoopPhisBackedge(node->target());
    return ProcessResult::kContinue;
  }

  ProcessResult Process(Dead* node, const ProcessingState&) {
    return ProcessResult::kRemove;
  }

  template <class NodeT>
  ProcessResult Process(NodeT* node, const ProcessingState& state) {
    return UpdateNodeInputs(node, &state);
  }

  DeoptFrame* GetDeoptFrameForEagerDeopt() {
    DCHECK_NOT_NULL(eager_deopt_frame_);
    return eager_deopt_frame_;
  }

 private:
  enum class HoistType : uint8_t {
    kNone,
    kLoopEntry,
    kLoopEntryUnchecked,
    kPrologue,
  };
  using HoistTypeList = base::SmallVector<HoistType, 8>;

  // Update the inputs of {phi} so that they all have {repr} representation, and
  // updates {phi}'s representation to {repr}.
  void ConvertTaggedPhiTo(Phi* phi, ValueRepresentation repr,
                          const HoistTypeList& hoist_untagging);
  template <class NodeT>
  ValueNode* GetReplacementForPhiInputConversion(ValueNode* conversion_node,
                                                 Phi* phi,
                                                 uint32_t input_index);

  // Since this pass changes the representation of Phis, it makes some untagging
  // operations outdated: if we've decided that a Phi should have Int32
  // representation, then we don't need to do a kCheckedSmiUntag before using
  // it. UpdateNodeInputs(n) removes such untagging from {n}'s input (and insert
  // new conversions if needed, from Int32 to Float64 for instance).
  template <IsUntaggingT UntaggingNodeT>
  ProcessResult UpdateNodeInputs(UntaggingNodeT* node,
                                 const ProcessingState* state) {
    if (node->NodeBase::input(0).node()->template Is<Phi>() &&
        node->NodeBase::input(0).node()->value_representation() !=
            ValueRepresentation::kTagged) {
      DCHECK_EQ(node->input_count(), 1);
      // This untagging conversion is outdated, since its input has been
      // untagged. Depending on the conversion, it might need to be replaced
      // by another untagged->untagged conversion, or it might need to be
      // removed altogether (or rather, replaced by an identity node).
      return UpdateUntaggingOfPhi(
          node->NodeBase::input(0).node()->template Cast<Phi>(),
          node->template Cast<ValueNode>());
    }
    return ProcessResult::kContinue;
  }

  template <typename NodeT>
  ProcessResult UpdateNodeInputs(NodeT* node, const ProcessingState* state) {
    return UpdateNonUntaggingNodeInputs(node, state);
  }

  template <class NodeT>
  ProcessResult UpdateNonUntaggingNodeInputs(NodeT* node,
                                             const ProcessingState* state) {
    // It would be bad to re-tag the input of an untagging node, so this
    // function should never be called on untagging nodes.
    static_assert(!IsUntagging(NodeBase::opcode_of<NodeT>));

    for (int i = 0; i < node->input_count(); i++) {
      ValueNode* input = node->NodeBase::input(i).node();
      if (input->Is<Identity>()) {
        // Bypassing the identity
        node->change_input(i, input->NodeBase::input(0).node());
      } else if (Phi* phi = input->TryCast<Phi>()) {
        // If the input is a Phi and it was used without any untagging, then
        // we need to retag it (with some additional checks/changes for some
        // nodes, cf the overload of UpdateNodePhiInput).
        ProcessResult result = UpdateNodePhiInput(node, phi, i, state);
        if (V8_UNLIKELY(result == ProcessResult::kRemove)) {
          return ProcessResult::kRemove;
        }
      }
    }

    return ProcessResult::kContinue;
  }

  ProcessResult UpdateNodePhiInput(CheckSmi* node, Phi* phi, int input_index,
                                   const ProcessingState* state);
  ProcessResult UpdateNodePhiInput(CheckNumber* node, Phi* phi, int input_index,
                                   const ProcessingState* state);
  ProcessResult UpdateNodePhiInput(StoreTaggedFieldNoWriteBarrier* node,
                                   Phi* phi, int input_index,
                                   const ProcessingState* state);
  ProcessResult UpdateNodePhiInput(StoreFixedArrayElementNoWriteBarrier* node,
                                   Phi* phi, int input_index,
                                   const ProcessingState* state);
  ProcessResult UpdateNodePhiInput(BranchIfToBooleanTrue* node, Phi* phi,
                                   int input_index,
                                   const ProcessingState* state);
  ProcessResult UpdateNodePhiInput(NodeBase* node, Phi* phi, int input_index,
                                   const ProcessingState* state);

  void EnsurePhiInputsTagged(Phi* phi);


  // Updates {old_untagging} to reflect that its Phi input has been untagged and
  // that a different conversion is now needed.
  ProcessResult UpdateUntaggingOfPhi(Phi* phi, ValueNode* old_untagging);

  // Returns a tagged node that represents a tagged version of {phi}.
  // If we are calling EnsurePhiTagged to ensure a Phi input of a Phi is tagged,
  // then {predecessor_index} should be set to the id of this input (ie, 0 for
  // the 1st input, 1 for the 2nd, etc.), so that we can use the SnapshotTable
  // to find existing tagging for {phi} in the {predecessor_index}th predecessor
  // of the current block.
  ValueNode* EnsurePhiTagged(
      Phi* phi, BasicBlock* block, BasicBlockPosition pos,
      const ProcessingState* state,
      std::optional<int> predecessor_index = std::nullopt);

  template <typename NodeT, typename... Args>
  NodeT* AddNewNodeNoInputConversion(BasicBlock* block, BasicBlockPosition pos,
                                     std::initializer_list<ValueNode*> inputs,
                                     Args&&... args);

  template <typename NodeT, typename... Args>
  NodeT* AddNewNodeNoInputConversionAtBlockEnd(
      BasicBlock* block, std::initializer_list<ValueNode*> inputs,
      Args&&... args) {
    return AddNewNodeNoInputConversion<NodeT>(
        block, BasicBlockPosition::End(), inputs, std::forward<Args>(args)...);
  }

  // If {block} is the start of a loop header, FixLoopPhisBackedge inserts the
  // necessary tagging on the backedge of the loop Phis of the loop header.
  // Additionally, if {block} contains untagged loop phis whose backedges have
  // been updated to Identity, FixLoopPhisBackedge unwraps those Identity.
  void FixLoopPhisBackedge(BasicBlock* block);

  void PreparePhiTaggings(BasicBlock* old_block, const BasicBlock* new_block);

  MaglevGraphLabeller* graph_labeller() const {
    return graph_->graph_labeller();
  }

  bool CanHoistUntaggingTo(BasicBlock* block);

  Zone* zone() const { return graph_->zone(); }

  Graph* graph_;

  MaglevReducer<MaglevPhiRepresentationSelector> reducer_;

  // {phi_taggings_} is a SnapshotTable containing mappings from untagged Phis
  // to Tagged alternatives for those phis.
  SnapshotTable<ValueNode*> phi_taggings_;
  // {predecessors_} is used during merging, but we use an instance variable for
  // it, in order to save memory and not reallocate it for each merge.
  ZoneVector<Snapshot> predecessors_;

  absl::flat_hash_map<BasicBlock::Id, Snapshot> snapshots_;

#ifdef DEBUG
  std::unordered_set<NodeBase*> new_nodes_;
#endif

  DeoptFrame* eager_deopt_frame_ = nullptr;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_PHI_REPRESENTATION_SELECTOR_H_

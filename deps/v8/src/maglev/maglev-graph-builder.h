// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_BUILDER_H_
#define V8_MAGLEV_MAGLEV_GRAPH_BUILDER_H_

#include <type_traits>

#include "src/base/optional.h"
#include "src/compiler/bytecode-analysis.h"
#include "src/compiler/bytecode-liveness-map.h"
#include "src/compiler/heap-refs.h"
#include "src/compiler/js-heap-broker.h"
#include "src/interpreter/bytecode-register.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir.h"
#include "src/utils/memcopy.h"

namespace v8 {
namespace internal {
namespace maglev {

class MaglevGraphBuilder {
 public:
  explicit MaglevGraphBuilder(LocalIsolate* local_isolate,
                              MaglevCompilationUnit* compilation_unit);

  void Build() {
    for (iterator_.Reset(); !iterator_.done(); iterator_.Advance()) {
      VisitSingleBytecode();
      // TODO(v8:7700): Clean up after all bytecodes are supported.
      if (found_unsupported_bytecode()) break;
    }

    // During InterpreterFrameState merge points, we might emit CheckedSmiTags
    // and add them unsafely to the basic blocks. This addition might break a
    // list invariant (namely `tail_` might not point to the last element).
    // We revalidate this invariant here in all basic blocks.
    for (BasicBlock* block : *graph_) {
      block->nodes().RevalidateTail();
    }
  }

  Graph* graph() const { return graph_; }

  // TODO(v8:7700): Clean up after all bytecodes are supported.
  bool found_unsupported_bytecode() const {
    return found_unsupported_bytecode_;
  }

 private:
  BasicBlock* CreateEmptyBlock(int offset, BasicBlock* predecessor) {
    DCHECK_NULL(current_block_);
    current_block_ = zone()->New<BasicBlock>(nullptr);
    BasicBlock* result = CreateBlock<Jump>({}, &jump_targets_[offset]);
    result->set_empty_block_predecessor(predecessor);
    return result;
  }

  void ProcessMergePoint(int offset) {
    // First copy the merge state to be the current state.
    MergePointInterpreterFrameState& merge_state = *merge_states_[offset];
    current_interpreter_frame_.CopyFrom(*compilation_unit_, merge_state);

    if (merge_state.predecessor_count() == 1) return;

    // Set up edge-split.
    int predecessor_index = merge_state.predecessor_count() - 1;
    BasicBlockRef* old_jump_targets = jump_targets_[offset].Reset();
    while (old_jump_targets != nullptr) {
      BasicBlock* predecessor = merge_state.predecessor_at(predecessor_index);
      if (predecessor == nullptr) {
        // We can have null predecessors if the predecessor is dead.
        predecessor_index--;
        continue;
      }
      ControlNode* control = predecessor->control_node();
      if (control->Is<ConditionalControlNode>()) {
        // CreateEmptyBlock automatically registers itself with the offset.
        predecessor = CreateEmptyBlock(offset, predecessor);
        // Set the old predecessor's (the conditional block) reference to
        // point to the new empty predecessor block.
        old_jump_targets =
            old_jump_targets->SetToBlockAndReturnNext(predecessor);
      } else {
        // Re-register the block in the offset's ref list.
        old_jump_targets =
            old_jump_targets->MoveToRefList(&jump_targets_[offset]);
      }
      predecessor->set_predecessor_id(predecessor_index--);
    }
#ifdef DEBUG
    if (bytecode_analysis().IsLoopHeader(offset)) {
      // For loops, the JumpLoop block hasn't been generated yet, and so isn't
      // in the list of jump targets. It's defined to be at index 0, so once
      // we've processed all the jump targets, the 0 index should be the one
      // remaining.
      DCHECK_EQ(predecessor_index, 0);
    } else {
      DCHECK_EQ(predecessor_index, -1);
    }
#endif
    if (has_graph_labeller()) {
      for (Phi* phi : *merge_states_[offset]->phis()) {
        graph_labeller()->RegisterNode(phi);
      }
    }
  }

  // Return true if the given offset is a merge point, i.e. there are jumps
  // targetting it.
  bool IsOffsetAMergePoint(int offset) {
    return merge_states_[offset] != nullptr;
  }

  // Called when a block is killed by an unconditional eager deopt.
  void EmitUnconditionalDeopt() {
    // Create a block rather than calling finish, since we don't yet know the
    // next block's offset before the loop skipping the rest of the bytecodes.
    BasicBlock* block = CreateBlock<Deopt>({});
    ResolveJumpsToBlockAtOffset(block, block_offset_);

    // Skip any bytecodes remaining in the block, up to the next merge point.
    while (!IsOffsetAMergePoint(iterator_.next_offset())) {
      iterator_.Advance();
      if (iterator_.done()) break;
    }

    // If there is control flow out of this block, we need to kill the merges
    // into the control flow targets.
    interpreter::Bytecode bytecode = iterator_.current_bytecode();
    if (interpreter::Bytecodes::IsForwardJump(bytecode)) {
      // Jumps merge into their target, and conditional jumps also merge into
      // the fallthrough.
      merge_states_[iterator_.GetJumpTargetOffset()]->MergeDead();
      if (interpreter::Bytecodes::IsConditionalJump(bytecode)) {
        merge_states_[iterator_.next_offset()]->MergeDead();
      }
    } else if (bytecode == interpreter::Bytecode::kJumpLoop) {
      // JumpLoop merges into its loop header, which has to be treated specially
      // by the merge..
      merge_states_[iterator_.GetJumpTargetOffset()]->MergeDeadLoop();
    } else if (interpreter::Bytecodes::IsSwitch(bytecode)) {
      // Switches merge into their targets, and into the fallthrough.
      for (auto offset : iterator_.GetJumpTableTargetOffsets()) {
        merge_states_[offset.target_offset]->MergeDead();
      }
      merge_states_[iterator_.next_offset()]->MergeDead();
    } else if (!interpreter::Bytecodes::Returns(bytecode) &&
               !interpreter::Bytecodes::UnconditionallyThrows(bytecode)) {
      // Any other bytecode that doesn't return or throw will merge into the
      // fallthrough.
      merge_states_[iterator_.next_offset()]->MergeDead();
    }
  }

  void VisitSingleBytecode() {
    int offset = iterator_.current_offset();
    if (V8_UNLIKELY(merge_states_[offset] != nullptr)) {
      if (current_block_ != nullptr) {
        // TODO(leszeks): Re-evaluate this DCHECK, we might hit it if the only
        // bytecodes in this basic block were only register juggling.
        // DCHECK(!current_block_->nodes().is_empty());
        FinishBlock<Jump>(offset, {}, &jump_targets_[offset]);

        merge_states_[offset]->Merge(*compilation_unit_,
                                     current_interpreter_frame_,
                                     graph()->last_block(), offset);
      }
      ProcessMergePoint(offset);
      StartNewBlock(offset);
    }
    DCHECK_NOT_NULL(current_block_);
#ifdef DEBUG
    // Clear new nodes for the next VisitFoo
    new_nodes_.clear();
#endif
    switch (iterator_.current_bytecode()) {
#define BYTECODE_CASE(name, ...)       \
  case interpreter::Bytecode::k##name: \
    Visit##name();                     \
    break;
      BYTECODE_LIST(BYTECODE_CASE)
#undef BYTECODE_CASE
    }
  }

#define BYTECODE_VISITOR(name, ...) void Visit##name();
  BYTECODE_LIST(BYTECODE_VISITOR)
#undef BYTECODE_VISITOR

  template <typename NodeT>
  NodeT* AddNode(NodeT* node) {
    if (node->properties().is_required_when_unused()) {
      MarkPossibleSideEffect();
    }
    current_block_->nodes().Add(node);
    if (has_graph_labeller()) graph_labeller()->RegisterNode(node);
#ifdef DEBUG
    new_nodes_.insert(node);
#endif
    return node;
  }

  template <typename NodeT, typename... Args>
  NodeT* AddNewNode(size_t input_count, Args&&... args) {
    return AddNode(
        CreateNewNode<NodeT>(input_count, std::forward<Args>(args)...));
  }

  template <typename NodeT, typename... Args>
  NodeT* AddNewNode(std::initializer_list<ValueNode*> inputs, Args&&... args) {
    return AddNode(CreateNewNode<NodeT>(inputs, std::forward<Args>(args)...));
  }

  template <typename NodeT, typename... Args>
  NodeT* CreateNewNode(Args&&... args) {
    if constexpr (NodeT::kProperties.can_eager_deopt()) {
      return NodeBase::New<NodeT>(zone(), *compilation_unit_,
                                  GetLatestCheckpointedState(),
                                  std::forward<Args>(args)...);
    } else if constexpr (NodeT::kProperties.can_lazy_deopt()) {
      return NodeBase::New<NodeT>(zone(), *compilation_unit_,
                                  GetCheckpointedStateForLazyDeopt(),
                                  std::forward<Args>(args)...);
    } else {
      return NodeBase::New<NodeT>(zone(), std::forward<Args>(args)...);
    }
  }

  ValueNode* GetContext() const {
    return current_interpreter_frame_.get(
        interpreter::Register::current_context());
  }

  FeedbackSlot GetSlotOperand(int operand_index) const {
    return iterator_.GetSlotOperand(operand_index);
  }

  template <class T, typename = std::enable_if_t<
                         std::is_convertible<T*, Object*>::value>>
  typename compiler::ref_traits<T>::ref_type GetRefOperand(int operand_index) {
    // The BytecodeArray itself was fetched by using a barrier so all reads
    // from the constant pool are safe.
    return MakeRefAssumeMemoryFence(
        broker(), broker()->CanonicalPersistentHandle(
                      Handle<T>::cast(iterator_.GetConstantForIndexOperand(
                          operand_index, local_isolate()))));
  }

  ValueNode* GetConstant(const compiler::ObjectRef& ref) {
    if (ref.IsSmi()) {
      return AddNewNode<SmiConstant>({}, Smi::FromInt(ref.AsSmi()));
    }
    // TODO(leszeks): Detect roots and use RootConstant.
    return AddNewNode<Constant>({}, ref.AsHeapObject());
  }

  // Move an existing ValueNode between two registers. You can pass
  // virtual_accumulator as the src or dst to move in or out of the accumulator.
  void MoveNodeBetweenRegisters(interpreter::Register src,
                                interpreter::Register dst) {
    // We shouldn't be moving newly created nodes between registers.
    DCHECK_EQ(0, new_nodes_.count(current_interpreter_frame_.get(src)));
    DCHECK_NOT_NULL(current_interpreter_frame_.get(src));

    current_interpreter_frame_.set(dst, current_interpreter_frame_.get(src));
  }

  ValueNode* GetTaggedValue(interpreter::Register reg) {
    // TODO(victorgomes): Add the representation (Tagged/Untagged) in the
    // InterpreterFrameState, so that we don't need to derefence a node.
    ValueNode* value = current_interpreter_frame_.get(reg);
    if (!value->is_untagged_value()) return value;
    if (value->Is<CheckedSmiUntag>()) {
      return value->input(0).node();
    }
    DCHECK(value->Is<Int32AddWithOverflow>() || value->Is<Int32Constant>());
    ValueNode* tagged = AddNewNode<CheckedSmiTag>({value});
    current_interpreter_frame_.set(reg, tagged);
    return tagged;
  }

  ValueNode* GetSmiUntaggedValue(interpreter::Register reg) {
    // TODO(victorgomes): Add the representation (Tagged/Untagged) in the
    // InterpreterFrameState, so that we don't need to derefence a node.
    ValueNode* value = current_interpreter_frame_.get(reg);
    if (value->is_untagged_value()) return value;
    if (value->Is<CheckedSmiTag>()) return value->input(0).node();
    // Untag any other value.
    ValueNode* untagged = AddNewNode<CheckedSmiUntag>({value});
    current_interpreter_frame_.set(reg, untagged);
    return untagged;
  }

  ValueNode* GetAccumulatorTaggedValue() {
    return GetTaggedValue(interpreter::Register::virtual_accumulator());
  }

  ValueNode* GetAccumulatorSmiUntaggedValue() {
    return GetSmiUntaggedValue(interpreter::Register::virtual_accumulator());
  }

  bool IsRegisterEqualToAccumulator(int operand_index) {
    interpreter::Register source = iterator_.GetRegisterOperand(operand_index);
    return current_interpreter_frame_.get(source) ==
           current_interpreter_frame_.accumulator();
  }

  ValueNode* LoadRegisterTaggedValue(int operand_index) {
    return GetTaggedValue(iterator_.GetRegisterOperand(operand_index));
  }

  ValueNode* LoadRegisterSmiUntaggedValue(int operand_index) {
    return GetSmiUntaggedValue(iterator_.GetRegisterOperand(operand_index));
  }

  template <typename NodeT>
  void SetAccumulator(NodeT* node) {
    // Accumulator stores are equivalent to stores to the virtual accumulator
    // register.
    StoreRegister(interpreter::Register::virtual_accumulator(), node);
  }

  template <typename NodeT>
  void StoreRegister(interpreter::Register target, NodeT* value) {
    // We should only set register values to nodes that were newly created in
    // this Visit. Existing nodes should be moved between registers with
    // MoveNodeBetweenRegisters.
    DCHECK_NE(0, new_nodes_.count(value));
    MarkAsLazyDeoptResult(value, target);
    current_interpreter_frame_.set(target, value);
  }

  CheckpointedInterpreterState GetLatestCheckpointedState() {
    if (!latest_checkpointed_state_) {
      latest_checkpointed_state_.emplace(
          BytecodeOffset(iterator_.current_offset()),
          zone()->New<CompactInterpreterFrameState>(
              *compilation_unit_, GetInLiveness(), current_interpreter_frame_));
    }
    return *latest_checkpointed_state_;
  }

  CheckpointedInterpreterState GetCheckpointedStateForLazyDeopt() {
    return CheckpointedInterpreterState(
        BytecodeOffset(iterator_.current_offset()),
        zone()->New<CompactInterpreterFrameState>(
            *compilation_unit_, GetOutLiveness(), current_interpreter_frame_));
  }

  template <typename NodeT>
  void MarkAsLazyDeoptResult(NodeT* value,
                             interpreter::Register result_location) {
    DCHECK_EQ(NodeT::kProperties.can_lazy_deopt(),
              value->properties().can_lazy_deopt());
    if constexpr (NodeT::kProperties.can_lazy_deopt()) {
      DCHECK(result_location.is_valid());
      DCHECK(!value->lazy_deopt_info()->result_location.is_valid());
      value->lazy_deopt_info()->result_location = result_location;
    }
  }

  void MarkPossibleSideEffect() {
    // If there was a potential side effect, invalidate the previous checkpoint.
    latest_checkpointed_state_.reset();
  }

  int next_offset() const {
    return iterator_.current_offset() + iterator_.current_bytecode_size();
  }
  const compiler::BytecodeLivenessState* GetInLiveness() const {
    return bytecode_analysis().GetInLivenessFor(iterator_.current_offset());
  }
  const compiler::BytecodeLivenessState* GetOutLiveness() const {
    return bytecode_analysis().GetOutLivenessFor(iterator_.current_offset());
  }

  void StartNewBlock(int offset) {
    DCHECK_NULL(current_block_);
    current_block_ = zone()->New<BasicBlock>(merge_states_[offset]);
    block_offset_ = offset;
  }

  template <typename ControlNodeT, typename... Args>
  BasicBlock* CreateBlock(std::initializer_list<ValueNode*> control_inputs,
                          Args&&... args) {
    current_block_->set_control_node(CreateNewNode<ControlNodeT>(
        control_inputs, std::forward<Args>(args)...));

    BasicBlock* block = current_block_;
    current_block_ = nullptr;

    graph()->Add(block);
    if (has_graph_labeller()) {
      graph_labeller()->RegisterBasicBlock(block);
    }
    return block;
  }

  // Update all jumps which were targetting the not-yet-created block at the
  // given `block_offset`, to now point to the given `block`.
  void ResolveJumpsToBlockAtOffset(BasicBlock* block, int block_offset) const {
    BasicBlockRef* jump_target_refs_head =
        jump_targets_[block_offset].SetToBlockAndReturnNext(block);
    while (jump_target_refs_head != nullptr) {
      jump_target_refs_head =
          jump_target_refs_head->SetToBlockAndReturnNext(block);
    }
    DCHECK_EQ(jump_targets_[block_offset].block_ptr(), block);
  }

  template <typename ControlNodeT, typename... Args>
  BasicBlock* FinishBlock(int next_block_offset,
                          std::initializer_list<ValueNode*> control_inputs,
                          Args&&... args) {
    BasicBlock* block =
        CreateBlock<ControlNodeT>(control_inputs, std::forward<Args>(args)...);
    ResolveJumpsToBlockAtOffset(block, block_offset_);

    // If the next block has merge states, then it's not a simple fallthrough,
    // and we should reset the checkpoint validity.
    if (merge_states_[next_block_offset] != nullptr) {
      latest_checkpointed_state_.reset();
    }
    // Start a new block for the fallthrough path, unless it's a merge point, in
    // which case we merge our state into it. That merge-point could also be a
    // loop header, in which case the merge state might not exist yet (if the
    // only predecessors are this path and the JumpLoop).
    DCHECK_NULL(current_block_);
    if (std::is_base_of<ConditionalControlNode, ControlNodeT>::value) {
      if (NumPredecessors(next_block_offset) == 1) {
        StartNewBlock(next_block_offset);
      } else {
        MergeIntoFrameState(block, next_block_offset);
      }
    }
    return block;
  }

  void BuildCallFromRegisterList(ConvertReceiverMode receiver_mode);
  void BuildCallFromRegisters(int argc_count,
                              ConvertReceiverMode receiver_mode);

  void BuildPropertyCellAccess(const compiler::PropertyCellRef& property_cell);

  template <Operation kOperation>
  void BuildGenericUnaryOperationNode();
  template <Operation kOperation>
  void BuildGenericBinaryOperationNode();
  template <Operation kOperation>
  void BuildGenericBinarySmiOperationNode();

  template <Operation kOperation>
  void VisitUnaryOperation();
  template <Operation kOperation>
  void VisitBinaryOperation();
  template <Operation kOperation>
  void VisitBinarySmiOperation();

  void MergeIntoFrameState(BasicBlock* block, int target);
  void BuildBranchIfTrue(ValueNode* node, int true_target, int false_target);
  void BuildBranchIfToBooleanTrue(ValueNode* node, int true_target,
                                  int false_target);

  void CalculatePredecessorCounts() {
    // Add 1 after the end of the bytecode so we can always write to the offset
    // after the last bytecode.
    size_t array_length = bytecode().length() + 1;
    predecessors_ = zone()->NewArray<uint32_t>(array_length);
    MemsetUint32(predecessors_, 1, array_length);

    interpreter::BytecodeArrayIterator iterator(bytecode().object());
    for (; !iterator.done(); iterator.Advance()) {
      interpreter::Bytecode bytecode = iterator.current_bytecode();
      if (interpreter::Bytecodes::IsJump(bytecode)) {
        predecessors_[iterator.GetJumpTargetOffset()]++;
        if (!interpreter::Bytecodes::IsConditionalJump(bytecode)) {
          predecessors_[iterator.next_offset()]--;
        }
      } else if (interpreter::Bytecodes::IsSwitch(bytecode)) {
        for (auto offset : iterator.GetJumpTableTargetOffsets()) {
          predecessors_[offset.target_offset]++;
        }
      } else if (interpreter::Bytecodes::Returns(bytecode) ||
                 interpreter::Bytecodes::UnconditionallyThrows(bytecode)) {
        predecessors_[iterator.next_offset()]--;
      }
      // TODO(leszeks): Also consider handler entries (the bytecode analysis)
      // will do this automatically I guess if we merge this into that.
    }
    DCHECK_EQ(0, predecessors_[bytecode().length()]);
  }

  int NumPredecessors(int offset) { return predecessors_[offset]; }

  compiler::JSHeapBroker* broker() const { return compilation_unit_->broker(); }
  const compiler::FeedbackVectorRef& feedback() const {
    return compilation_unit_->feedback();
  }
  const FeedbackNexus FeedbackNexusForOperand(int slot_operand_index) const {
    return FeedbackNexus(feedback().object(),
                         GetSlotOperand(slot_operand_index),
                         broker()->feedback_nexus_config());
  }
  const FeedbackNexus FeedbackNexusForSlot(FeedbackSlot slot) const {
    return FeedbackNexus(feedback().object(), slot,
                         broker()->feedback_nexus_config());
  }
  const compiler::BytecodeArrayRef& bytecode() const {
    return compilation_unit_->bytecode();
  }
  const compiler::BytecodeAnalysis& bytecode_analysis() const {
    return compilation_unit_->bytecode_analysis();
  }
  LocalIsolate* local_isolate() const { return local_isolate_; }
  Zone* zone() const { return compilation_unit_->zone(); }
  int parameter_count() const { return compilation_unit_->parameter_count(); }
  int register_count() const { return compilation_unit_->register_count(); }
  bool has_graph_labeller() const {
    return compilation_unit_->has_graph_labeller();
  }
  MaglevGraphLabeller* graph_labeller() const {
    return compilation_unit_->graph_labeller();
  }

  LocalIsolate* const local_isolate_;
  MaglevCompilationUnit* const compilation_unit_;
  interpreter::BytecodeArrayIterator iterator_;
  uint32_t* predecessors_;

  // Current block information.
  BasicBlock* current_block_ = nullptr;
  int block_offset_ = 0;
  base::Optional<CheckpointedInterpreterState> latest_checkpointed_state_;

  BasicBlockRef* jump_targets_;
  MergePointInterpreterFrameState** merge_states_;

  Graph* const graph_;
  InterpreterFrameState current_interpreter_frame_;

  // Allow marking some bytecodes as unsupported during graph building, so that
  // we can test maglev incrementally.
  // TODO(v8:7700): Clean up after all bytecodes are supported.
  bool found_unsupported_bytecode_ = false;
  bool this_field_will_be_unused_once_all_bytecodes_are_supported_;

#ifdef DEBUG
  std::unordered_set<Node*> new_nodes_;
#endif
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_BUILDER_H_

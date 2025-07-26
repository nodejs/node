// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_BASIC_BLOCK_H_
#define V8_MAGLEV_MAGLEV_BASIC_BLOCK_H_

#include <vector>

#include "src/base/small-vector.h"
#include "src/codegen/label.h"
#include "src/compiler/turboshaft/snapshot-table.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"
#include "src/zone/zone-list.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace maglev {

using NodeIterator = ZoneVector<Node*>::iterator;
using NodeConstIterator = ZoneVector<Node*>::const_iterator;

class BasicBlock {
 public:
  explicit BasicBlock(MergePointInterpreterFrameState* state, Zone* zone)
      : type_(state ? kMerge : kOther),
        nodes_(zone),
        control_node_(nullptr),
        state_(state) {}

  NodeIdT first_id() const {
    if (has_phi()) return phis()->first()->id();
    return first_non_phi_id();
  }

  // For GDB: Print any basic block with `print bb->Print()`.
  void Print() const;

  NodeIdT first_non_phi_id() const {
    for (const Node* node : nodes_) {
      if (node == nullptr) continue;
      if (!node->Is<Identity>()) return node->id();
    }
    return control_node()->id();
  }

  NodeIdT FirstNonGapMoveId() const {
    if (has_phi()) return phis()->first()->id();
    for (const Node* node : nodes_) {
      if (node == nullptr) continue;
      if (IsGapMoveNode(node->opcode())) continue;
      if (node->Is<Identity>()) continue;
      return node->id();
    }
    return control_node()->id();
  }

  ZoneVector<Node*>& nodes() { return nodes_; }

  ControlNode* control_node() const { return control_node_; }
  void set_control_node(ControlNode* control_node) {
    DCHECK_NULL(control_node_);
    control_node_ = control_node;
  }

  ControlNode* reset_control_node() {
    DCHECK_NOT_NULL(control_node_);
    ControlNode* control = control_node_;
    control_node_ = nullptr;
    return control;
  }

  // Moves all nodes after |node| to the resulting ZoneVector, while keeping all
  // nodes before |node| in the basic block. |node| itself is dropped.
  ZoneVector<Node*> Split(Node* node, Zone* zone) {
    size_t split = 0;
    for (; split < nodes_.size(); split++) {
      if (nodes_[split] == node) break;
    }
    DCHECK_NE(split, nodes_.size());
    size_t after_split = split + 1;
    ZoneVector<Node*> result(nodes_.size() - after_split, zone);
    for (size_t i = 0; i < result.size(); i++) {
      result[i] = nodes_[i + after_split];
    }
    nodes_.resize(split);
    return result;
  }

  bool has_phi() const { return has_state() && state_->has_phi(); }

  bool is_merge_block() const { return type_ == kMerge; }
  bool is_edge_split_block() const { return type_ == kEdgeSplit; }

  bool is_loop() const { return has_state() && state()->is_loop(); }

  MergePointRegisterState& edge_split_block_register_state() {
    DCHECK_EQ(type_, kEdgeSplit);
    DCHECK_NOT_NULL(edge_split_block_register_state_);
    return *edge_split_block_register_state_;
  }

  bool contains_node_id(NodeIdT id) const {
    return id >= first_id() && id <= control_node()->id();
  }

  void set_edge_split_block_register_state(
      MergePointRegisterState* register_state) {
    DCHECK_EQ(type_, kEdgeSplit);
    edge_split_block_register_state_ = register_state;
  }

  void set_edge_split_block(BasicBlock* predecessor) {
    DCHECK_EQ(type_, kOther);
    DCHECK(nodes_.empty());
    DCHECK(control_node()->Is<Jump>());
    type_ = kEdgeSplit;
    predecessor_ = predecessor;
  }

  BasicBlock* predecessor() const {
    DCHECK(type_ == kEdgeSplit || type_ == kOther);
    return predecessor_;
  }
  void set_predecessor(BasicBlock* predecessor) {
    DCHECK(type_ == kEdgeSplit || type_ == kOther);
    DCHECK_NULL(edge_split_block_register_state_);
    predecessor_ = predecessor;
  }

  bool is_start_block_of_switch_case() const {
    return is_start_block_of_switch_case_;
  }
  void set_start_block_of_switch_case(bool value) {
    is_start_block_of_switch_case_ = value;
  }

  bool is_dead() const { return is_dead_; }

  void mark_dead() { is_dead_ = true; }

  Phi::List* phis() const {
    DCHECK(has_phi());
    return state_->phis();
  }
  void AddPhi(Phi* phi) const {
    DCHECK(has_state());
    state_->phis()->Add(phi);
  }

  ExceptionHandlerInfo::List& exception_handlers() {
    return exception_handlers_;
  }

  void AddExceptionHandler(ExceptionHandlerInfo* handler) {
    exception_handlers_.Add(handler);
  }

  int predecessor_count() const {
    DCHECK(has_state());
    return state()->predecessor_count();
  }

  BasicBlock* predecessor_at(int i) const {
    DCHECK(has_state());
    return state_->predecessor_at(i);
  }

  BasicBlock* backedge_predecessor() const {
    DCHECK(is_loop());
    return predecessor_at(predecessor_count() - 1);
  }

  int predecessor_id() const {
    return control_node()->Cast<UnconditionalControlNode>()->predecessor_id();
  }
  void set_predecessor_id(int id) {
    control_node()->Cast<UnconditionalControlNode>()->set_predecessor_id(id);
  }

  base::SmallVector<BasicBlock*, 2> successors() const;

  template <typename Func>
  void ForEachPredecessor(Func&& functor) const {
    if (type_ == kEdgeSplit || type_ == kOther) {
      BasicBlock* predecessor_block = predecessor();
      if (predecessor_block) {
        functor(predecessor_block);
      }
    } else {
      for (int i = 0; i < predecessor_count(); i++) {
        functor(predecessor_at(i));
      }
    }
  }

  template <typename Func>
  static void ForEachSuccessorFollowing(ControlNode* control, Func&& functor) {
    if (auto unconditional_control =
            control->TryCast<UnconditionalControlNode>()) {
      functor(unconditional_control->target());
    } else if (auto branch = control->TryCast<BranchControlNode>()) {
      functor(branch->if_true());
      functor(branch->if_false());
    } else if (auto switch_node = control->TryCast<Switch>()) {
      for (int i = 0; i < switch_node->size(); i++) {
        functor(switch_node->targets()[i].block_ptr());
      }
      if (switch_node->has_fallthrough()) {
        functor(switch_node->fallthrough());
      }
    }
  }

  template <typename Func>
  void ForEachSuccessor(Func&& functor) const {
    ControlNode* control = control_node();
    ForEachSuccessorFollowing(control, functor);
  }

  Label* label() {
    // If this fails, jump threading is missing for the node. See
    // MaglevCodeGeneratingNodeProcessor::PatchJumps.
    DCHECK_EQ(this, ComputeRealJumpTarget());
    return &label_;
  }
  MergePointInterpreterFrameState* state() const {
    DCHECK(has_state());
    return state_;
  }
  bool has_state() const { return type_ == kMerge && state_ != nullptr; }

  bool is_exception_handler_block() const {
    return has_state() && state_->is_exception_handler();
  }

  // If the basic block is an empty (unnecessary) block containing only an
  // unconditional jump to the successor block, return the successor block.
  BasicBlock* ComputeRealJumpTarget() {
    BasicBlock* current = this;
    while (true) {
      if (!current->nodes_.empty() || current->is_loop() ||
          current->is_exception_handler_block() ||
          current->HasPhisOrRegisterMerges()) {
        break;
      }
      Jump* control = current->control_node()->TryCast<Jump>();
      if (!control) {
        break;
      }
      BasicBlock* next = control->target();
      if (next->HasPhisOrRegisterMerges()) {
        break;
      }
      current = next;
    }
    return current;
  }

  bool is_deferred() const { return deferred_; }
  void set_deferred(bool deferred) { deferred_ = deferred; }

  using Id = uint32_t;
  constexpr static Id kInvalidBlockId = 0xffffffff;

  void set_id(Id id) {
    DCHECK(!has_id());
    id_ = id;
  }
  bool has_id() const { return id_ != kInvalidBlockId; }
  Id id() const {
    DCHECK(has_id());
    return id_;
  }

 private:
  bool HasPhisOrRegisterMerges() const {
    if (!has_state()) {
      return false;
    }
    if (has_phi()) {
      return true;
    }
    bool has_register_merge = false;
#ifdef V8_ENABLE_MAGLEV
    if (!state()->register_state().is_initialized()) {
      // This can happen when the graph has disconnected blocks; bail out and
      // don't jump thread them.
      return true;
    }

    state()->register_state().ForEachGeneralRegister(
        [&](Register reg, RegisterState& state) {
          ValueNode* node;
          RegisterMerge* merge;
          if (LoadMergeState(state, &node, &merge)) {
            has_register_merge = true;
          }
        });
    state()->register_state().ForEachDoubleRegister(
        [&](DoubleRegister reg, RegisterState& state) {
          ValueNode* node;
          RegisterMerge* merge;
          if (LoadMergeState(state, &node, &merge)) {
            has_register_merge = true;
          }
        });
#endif  // V8_ENABLE_MAGLEV
    return has_register_merge;
  }

  enum : uint8_t { kMerge, kEdgeSplit, kOther } type_;
  bool deferred_ : 1 = false;
  bool is_start_block_of_switch_case_ : 1 = false;
  bool is_dead_ : 1 = false;

  Id id_ = kInvalidBlockId;

  ZoneVector<Node*> nodes_;
  ControlNode* control_node_;
  ExceptionHandlerInfo::List exception_handlers_;

  union {
    MergePointInterpreterFrameState* state_;
    MergePointRegisterState* edge_split_block_register_state_;
  };
  // For kEdgeSplit and kOther blocks.
  BasicBlock* predecessor_ = nullptr;
  Label label_;

  inline void check_layout();
};

void BasicBlock::check_layout() {
  // Ensure non pointer sized values are nicely packed.
  static_assert(offsetof(BasicBlock, nodes_) == 8);
}

inline base::SmallVector<BasicBlock*, 2> BasicBlock::successors() const {
  ControlNode* control = control_node();
  if (auto unconditional_control =
          control->TryCast<UnconditionalControlNode>()) {
    return {unconditional_control->target()};
  } else if (auto branch = control->TryCast<BranchControlNode>()) {
    return {branch->if_true(), branch->if_false()};
  } else if (auto switch_node = control->TryCast<Switch>()) {
    base::SmallVector<BasicBlock*, 2> succs;
    for (int i = 0; i < switch_node->size(); i++) {
      succs.push_back(switch_node->targets()[i].block_ptr());
    }
    if (switch_node->has_fallthrough()) {
      succs.push_back(switch_node->fallthrough());
    }
    return succs;
  } else {
    return base::SmallVector<BasicBlock*, 2>();
  }
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_BASIC_BLOCK_H_

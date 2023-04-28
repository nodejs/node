// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_BASIC_BLOCK_H_
#define V8_MAGLEV_MAGLEV_BASIC_BLOCK_H_

#include <vector>

#include "src/base/small-vector.h"
#include "src/codegen/label.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace maglev {

using NodeIterator = Node::List::Iterator;
using NodeConstIterator = Node::List::Iterator;

class BasicBlock {
 public:
  explicit BasicBlock(MergePointInterpreterFrameState* state)
      : control_node_(nullptr), state_(state) {}

  uint32_t first_id() const {
    if (has_phi()) return phis()->first()->id();
    if (nodes_.is_empty()) {
      return control_node()->id();
    }
    auto node = nodes_.first();
    while (node && node->Is<Identity>()) {
      node = node->NextNode();
    }
    return node ? node->id() : control_node()->id();
  }

  uint32_t FirstNonGapMoveId() const {
    if (has_phi()) return phis()->first()->id();
    if (!nodes_.is_empty()) {
      for (const Node* node : nodes_) {
        if (IsGapMoveNode(node->opcode())) continue;
        if (node->Is<Identity>()) continue;
        return node->id();
      }
    }
    return control_node()->id();
  }

  Node::List& nodes() { return nodes_; }

  ControlNode* control_node() const { return control_node_; }
  void set_control_node(ControlNode* control_node) {
    DCHECK_NULL(control_node_);
    control_node_ = control_node;
  }

  bool has_phi() const { return has_state() && state_->has_phi(); }

  bool is_edge_split_block() const { return is_edge_split_block_; }

  bool is_loop() const { return has_state() && state()->is_loop(); }

  MergePointRegisterState& edge_split_block_register_state() {
    DCHECK(is_edge_split_block());
    return *edge_split_block_register_state_;
  }

  bool contains_node_id(NodeIdT id) const {
    return id >= first_id() && id <= control_node()->id();
  }

  void set_edge_split_block_register_state(
      MergePointRegisterState* register_state) {
    DCHECK(is_edge_split_block());
    edge_split_block_register_state_ = register_state;
  }

  void set_edge_split_block() {
    DCHECK_IMPLIES(!nodes_.is_empty(),
                   nodes_.LengthForTest() == 1 &&
                       nodes_.first()->Is<IncreaseInterruptBudget>());
    DCHECK(control_node()->Is<Jump>());
    DCHECK_NULL(state_);
    is_edge_split_block_ = true;
    edge_split_block_register_state_ = nullptr;
  }

  bool is_start_block_of_switch_case() const {
    return is_start_block_of_switch_case_;
  }
  void set_start_block_of_switch_case(bool value) {
    is_start_block_of_switch_case_ = value;
  }

  Phi::List* phis() const {
    DCHECK(has_phi());
    return state_->phis();
  }

  BasicBlock* predecessor_at(int i) const {
    DCHECK_NOT_NULL(state_);
    return state_->predecessor_at(i);
  }

  int predecessor_id() const {
    return control_node()->Cast<UnconditionalControlNode>()->predecessor_id();
  }
  void set_predecessor_id(int id) {
    control_node()->Cast<UnconditionalControlNode>()->set_predecessor_id(id);
  }

  base::SmallVector<BasicBlock*, 2> successors() const;

  Label* label() { return &label_; }
  MergePointInterpreterFrameState* state() const {
    DCHECK(has_state());
    return state_;
  }
  bool has_state() const { return !is_edge_split_block() && state_ != nullptr; }

  bool is_exception_handler_block() const {
    return has_state() && state_->is_exception_handler();
  }

 private:
  bool is_edge_split_block_ = false;
  bool is_start_block_of_switch_case_ = false;
  Node::List nodes_;
  ControlNode* control_node_;
  union {
    MergePointInterpreterFrameState* state_;
    MergePointRegisterState* edge_split_block_register_state_;
  };
  Label label_;
};

inline base::SmallVector<BasicBlock*, 2> BasicBlock::successors() const {
  ControlNode* control = control_node();
  if (auto node = control->TryCast<UnconditionalControlNode>()) {
    return {node->target()};
  } else if (auto node = control->TryCast<BranchControlNode>()) {
    return {node->if_true(), node->if_false()};
  } else if (auto node = control->TryCast<Switch>()) {
    base::SmallVector<BasicBlock*, 2> succs;
    for (int i = 0; i < node->size(); i++) {
      succs.push_back(node->targets()[i].block_ptr());
    }
    if (node->has_fallthrough()) {
      succs.push_back(node->fallthrough());
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

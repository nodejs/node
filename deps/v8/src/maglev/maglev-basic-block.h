// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_BASIC_BLOCK_H_
#define V8_MAGLEV_MAGLEV_BASIC_BLOCK_H_

#include <vector>

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
    return nodes_.is_empty() ? control_node()->id() : nodes_.first()->id();
  }

  uint32_t FirstNonGapMoveId() const {
    if (has_phi()) return phis()->first()->id();
    if (!nodes_.is_empty()) {
      for (const Node* node : nodes_) {
        if (IsGapMoveNode(node->opcode())) continue;
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

  bool is_empty_block() const { return is_empty_block_; }

  BasicBlock* empty_block_predecessor() const {
    DCHECK(is_empty_block());
    return empty_block_predecessor_;
  }

  MergePointRegisterState& empty_block_register_state() {
    DCHECK(is_empty_block());
    return *empty_block_register_state_;
  }

  void set_empty_block_register_state(MergePointRegisterState* register_state) {
    DCHECK(is_empty_block());
    empty_block_register_state_ = register_state;
  }

  void set_empty_block_predecessor(BasicBlock* predecessor) {
    DCHECK(nodes_.is_empty());
    DCHECK(control_node()->Is<Jump>());
    DCHECK_NULL(state_);
    is_empty_block_ = true;
    empty_block_register_state_ = nullptr;
    empty_block_predecessor_ = predecessor;
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

  Label* label() { return &label_; }
  MergePointInterpreterFrameState* state() const {
    DCHECK(has_state());
    return state_;
  }
  bool has_state() const { return !is_empty_block() && state_ != nullptr; }

  bool is_exception_handler_block() const {
    return has_state() && state_->is_exception_handler();
  }

 private:
  bool is_empty_block_ = false;
  Node::List nodes_;
  ControlNode* control_node_;
  union {
    MergePointInterpreterFrameState* state_;
    MergePointRegisterState* empty_block_register_state_;
  };
  BasicBlock* empty_block_predecessor_;
  Label label_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_BASIC_BLOCK_H_

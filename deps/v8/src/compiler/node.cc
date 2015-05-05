// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/node.h"

#include <algorithm>

namespace v8 {
namespace internal {
namespace compiler {

Node* Node::New(Zone* zone, NodeId id, const Operator* op, int input_count,
                Node** inputs, bool has_extensible_inputs) {
  size_t node_size = sizeof(Node) - sizeof(Input);
  int reserve_input_count = has_extensible_inputs ? kDefaultReservedInputs : 0;
  size_t inputs_size = std::max<size_t>(
      (input_count + reserve_input_count) * sizeof(Input), sizeof(InputDeque*));
  size_t uses_size = input_count * sizeof(Use);
  int size = static_cast<int>(node_size + inputs_size + uses_size);
  void* buffer = zone->New(size);
  Node* result = new (buffer) Node(id, op, input_count, reserve_input_count);
  Input* input = result->inputs_.static_;
  Use* use =
      reinterpret_cast<Use*>(reinterpret_cast<char*>(input) + inputs_size);

  for (int current = 0; current < input_count; ++current) {
    Node* to = *inputs++;
    input->to = to;
    input->use = use;
    use->input_index = current;
    use->from = result;
    to->AppendUse(use);
    ++use;
    ++input;
  }
  return result;
}


void Node::Kill() {
  DCHECK_NOT_NULL(op());
  NullAllInputs();
  DCHECK(uses().empty());
}


void Node::AppendInput(Zone* zone, Node* new_to) {
  DCHECK_NOT_NULL(zone);
  DCHECK_NOT_NULL(new_to);
  Use* new_use = new (zone) Use;
  Input new_input;
  new_input.to = new_to;
  new_input.use = new_use;
  if (reserved_input_count() > 0) {
    DCHECK(!has_appendable_inputs());
    set_reserved_input_count(reserved_input_count() - 1);
    inputs_.static_[input_count()] = new_input;
  } else {
    EnsureAppendableInputs(zone);
    inputs_.appendable_->push_back(new_input);
  }
  new_use->input_index = input_count();
  new_use->from = this;
  new_to->AppendUse(new_use);
  set_input_count(input_count() + 1);
}


void Node::InsertInput(Zone* zone, int index, Node* new_to) {
  DCHECK_NOT_NULL(zone);
  DCHECK_LE(0, index);
  DCHECK_LT(index, InputCount());
  AppendInput(zone, InputAt(InputCount() - 1));
  for (int i = InputCount() - 1; i > index; --i) {
    ReplaceInput(i, InputAt(i - 1));
  }
  ReplaceInput(index, new_to);
}


void Node::RemoveInput(int index) {
  DCHECK_LE(0, index);
  DCHECK_LT(index, InputCount());
  for (; index < InputCount() - 1; ++index) {
    ReplaceInput(index, InputAt(index + 1));
  }
  TrimInputCount(InputCount() - 1);
}


void Node::NullAllInputs() {
  for (Edge edge : input_edges()) edge.UpdateTo(nullptr);
}


void Node::TrimInputCount(int new_input_count) {
  DCHECK_LE(new_input_count, input_count());
  if (new_input_count == input_count()) return;  // Nothing to do.
  for (int index = new_input_count; index < input_count(); ++index) {
    ReplaceInput(index, nullptr);
  }
  if (!has_appendable_inputs()) {
    set_reserved_input_count(std::min<int>(
        ReservedInputCountField::kMax,
        reserved_input_count() + (input_count() - new_input_count)));
  }
  set_input_count(new_input_count);
}


int Node::UseCount() const {
  int use_count = 0;
  for (const Use* use = first_use_; use; use = use->next) {
    ++use_count;
  }
  return use_count;
}


void Node::ReplaceUses(Node* that) {
  DCHECK(this->first_use_ == nullptr || this->first_use_->prev == nullptr);
  DCHECK(that->first_use_ == nullptr || that->first_use_->prev == nullptr);

  // Update the pointers to {this} to point to {that}.
  Use* last_use = nullptr;
  for (Use* use = this->first_use_; use; use = use->next) {
    use->from->GetInputRecordPtr(use->input_index)->to = that;
    last_use = use;
  }
  if (last_use) {
    // Concat the use list of {this} and {that}.
    last_use->next = that->first_use_;
    if (that->first_use_) that->first_use_->prev = last_use;
    that->first_use_ = this->first_use_;
  }
  first_use_ = nullptr;
}


void Node::Input::Update(Node* new_to) {
  Node* old_to = this->to;
  if (new_to == old_to) return;  // Nothing to do.
  // Snip out the use from where it used to be
  if (old_to) {
    old_to->RemoveUse(use);
  }
  to = new_to;
  // And put it into the new node's use list.
  if (new_to) {
    new_to->AppendUse(use);
  } else {
    use->next = nullptr;
    use->prev = nullptr;
  }
}


Node::Node(NodeId id, const Operator* op, int input_count,
           int reserved_input_count)
    : op_(op),
      mark_(0),
      id_(id),
      bit_field_(InputCountField::encode(input_count) |
                 ReservedInputCountField::encode(reserved_input_count) |
                 HasAppendableInputsField::encode(false)),
      first_use_(nullptr) {}


void Node::EnsureAppendableInputs(Zone* zone) {
  if (!has_appendable_inputs()) {
    void* deque_buffer = zone->New(sizeof(InputDeque));
    InputDeque* deque = new (deque_buffer) InputDeque(zone);
    for (int i = 0; i < input_count(); ++i) {
      deque->push_back(inputs_.static_[i]);
    }
    inputs_.appendable_ = deque;
    set_has_appendable_inputs(true);
  }
}


void Node::AppendUse(Use* const use) {
  DCHECK(first_use_ == nullptr || first_use_->prev == nullptr);
  use->next = first_use_;
  use->prev = nullptr;
  if (first_use_) first_use_->prev = use;
  first_use_ = use;
}


void Node::RemoveUse(Use* const use) {
  DCHECK(first_use_ == nullptr || first_use_->prev == nullptr);
  if (use->prev) {
    DCHECK_NE(first_use_, use);
    use->prev->next = use->next;
  } else {
    DCHECK_EQ(first_use_, use);
    first_use_ = use->next;
  }
  if (use->next) {
    use->next->prev = use->prev;
  }
}


std::ostream& operator<<(std::ostream& os, const Node& n) {
  os << n.id() << ": " << *n.op();
  if (n.InputCount() > 0) {
    os << "(";
    for (int i = 0; i < n.InputCount(); ++i) {
      if (i != 0) os << ", ";
      os << n.InputAt(i)->id();
    }
    os << ")";
  }
  return os;
}


Node::InputEdges::iterator Node::InputEdges::iterator::operator++(int n) {
  iterator result(*this);
  ++(*this);
  return result;
}


bool Node::InputEdges::empty() const { return begin() == end(); }


Node::Inputs::const_iterator Node::Inputs::const_iterator::operator++(int n) {
  const_iterator result(*this);
  ++(*this);
  return result;
}


bool Node::Inputs::empty() const { return begin() == end(); }


Node::UseEdges::iterator Node::UseEdges::iterator::operator++(int n) {
  iterator result(*this);
  ++(*this);
  return result;
}


bool Node::UseEdges::empty() const { return begin() == end(); }


Node::Uses::const_iterator Node::Uses::const_iterator::operator++(int n) {
  const_iterator result(*this);
  ++(*this);
  return result;
}


bool Node::Uses::empty() const { return begin() == end(); }

}  // namespace compiler
}  // namespace internal
}  // namespace v8

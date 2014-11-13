// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GENERIC_NODE_INL_H_
#define V8_COMPILER_GENERIC_NODE_INL_H_

#include "src/v8.h"

#include "src/compiler/generic-graph.h"
#include "src/compiler/generic-node.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

template <class B, class S>
GenericNode<B, S>::GenericNode(GenericGraphBase* graph, int input_count,
                               int reserve_input_count)
    : BaseClass(graph->zone()),
      input_count_(input_count),
      reserve_input_count_(reserve_input_count),
      has_appendable_inputs_(false),
      use_count_(0),
      first_use_(NULL),
      last_use_(NULL) {
  DCHECK(reserve_input_count <= kMaxReservedInputs);
  inputs_.static_ = reinterpret_cast<Input*>(this + 1);
  AssignUniqueID(graph);
}

template <class B, class S>
inline void GenericNode<B, S>::AssignUniqueID(GenericGraphBase* graph) {
  id_ = graph->NextNodeID();
}

template <class B, class S>
inline typename GenericNode<B, S>::Inputs::iterator
GenericNode<B, S>::Inputs::begin() {
  return typename GenericNode<B, S>::Inputs::iterator(this->node_, 0);
}

template <class B, class S>
inline typename GenericNode<B, S>::Inputs::iterator
GenericNode<B, S>::Inputs::end() {
  return typename GenericNode<B, S>::Inputs::iterator(
      this->node_, this->node_->InputCount());
}

template <class B, class S>
inline typename GenericNode<B, S>::Uses::iterator
GenericNode<B, S>::Uses::begin() {
  return typename GenericNode<B, S>::Uses::iterator(this->node_);
}

template <class B, class S>
inline typename GenericNode<B, S>::Uses::iterator
GenericNode<B, S>::Uses::end() {
  return typename GenericNode<B, S>::Uses::iterator();
}

template <class B, class S>
void GenericNode<B, S>::ReplaceUses(GenericNode* replace_to) {
  for (Use* use = first_use_; use != NULL; use = use->next) {
    use->from->GetInputRecordPtr(use->input_index)->to = replace_to;
  }
  if (replace_to->last_use_ == NULL) {
    DCHECK_EQ(NULL, replace_to->first_use_);
    replace_to->first_use_ = first_use_;
    replace_to->last_use_ = last_use_;
  } else if (first_use_ != NULL) {
    DCHECK_NE(NULL, replace_to->first_use_);
    replace_to->last_use_->next = first_use_;
    first_use_->prev = replace_to->last_use_;
    replace_to->last_use_ = last_use_;
  }
  replace_to->use_count_ += use_count_;
  use_count_ = 0;
  first_use_ = NULL;
  last_use_ = NULL;
}

template <class B, class S>
template <class UnaryPredicate>
void GenericNode<B, S>::ReplaceUsesIf(UnaryPredicate pred,
                                      GenericNode* replace_to) {
  for (Use* use = first_use_; use != NULL;) {
    Use* next = use->next;
    if (pred(static_cast<S*>(use->from))) {
      RemoveUse(use);
      replace_to->AppendUse(use);
      use->from->GetInputRecordPtr(use->input_index)->to = replace_to;
    }
    use = next;
  }
}

template <class B, class S>
void GenericNode<B, S>::RemoveAllInputs() {
  for (typename Inputs::iterator iter(inputs().begin()); iter != inputs().end();
       ++iter) {
    iter.GetInput()->Update(NULL);
  }
}

template <class B, class S>
void GenericNode<B, S>::TrimInputCount(int new_input_count) {
  if (new_input_count == input_count_) return;  // Nothing to do.

  DCHECK(new_input_count < input_count_);

  // Update inline inputs.
  for (int i = new_input_count; i < input_count_; i++) {
    typename GenericNode<B, S>::Input* input = GetInputRecordPtr(i);
    input->Update(NULL);
  }
  input_count_ = new_input_count;
}

template <class B, class S>
void GenericNode<B, S>::ReplaceInput(int index, GenericNode<B, S>* new_to) {
  Input* input = GetInputRecordPtr(index);
  input->Update(new_to);
}

template <class B, class S>
void GenericNode<B, S>::Input::Update(GenericNode<B, S>* new_to) {
  GenericNode* old_to = this->to;
  if (new_to == old_to) return;  // Nothing to do.
  // Snip out the use from where it used to be
  if (old_to != NULL) {
    old_to->RemoveUse(use);
  }
  to = new_to;
  // And put it into the new node's use list.
  if (new_to != NULL) {
    new_to->AppendUse(use);
  } else {
    use->next = NULL;
    use->prev = NULL;
  }
}

template <class B, class S>
void GenericNode<B, S>::EnsureAppendableInputs(Zone* zone) {
  if (!has_appendable_inputs_) {
    void* deque_buffer = zone->New(sizeof(InputDeque));
    InputDeque* deque = new (deque_buffer) InputDeque(zone);
    for (int i = 0; i < input_count_; ++i) {
      deque->push_back(inputs_.static_[i]);
    }
    inputs_.appendable_ = deque;
    has_appendable_inputs_ = true;
  }
}

template <class B, class S>
void GenericNode<B, S>::AppendInput(Zone* zone, GenericNode<B, S>* to_append) {
  Use* new_use = new (zone) Use;
  Input new_input;
  new_input.to = to_append;
  new_input.use = new_use;
  if (reserve_input_count_ > 0) {
    DCHECK(!has_appendable_inputs_);
    reserve_input_count_--;
    inputs_.static_[input_count_] = new_input;
  } else {
    EnsureAppendableInputs(zone);
    inputs_.appendable_->push_back(new_input);
  }
  new_use->input_index = input_count_;
  new_use->from = this;
  to_append->AppendUse(new_use);
  input_count_++;
}

template <class B, class S>
void GenericNode<B, S>::InsertInput(Zone* zone, int index,
                                    GenericNode<B, S>* to_insert) {
  DCHECK(index >= 0 && index < InputCount());
  // TODO(turbofan): Optimize this implementation!
  AppendInput(zone, InputAt(InputCount() - 1));
  for (int i = InputCount() - 1; i > index; --i) {
    ReplaceInput(i, InputAt(i - 1));
  }
  ReplaceInput(index, to_insert);
}

template <class B, class S>
void GenericNode<B, S>::RemoveInput(int index) {
  DCHECK(index >= 0 && index < InputCount());
  // TODO(turbofan): Optimize this implementation!
  for (; index < InputCount() - 1; ++index) {
    ReplaceInput(index, InputAt(index + 1));
  }
  TrimInputCount(InputCount() - 1);
}

template <class B, class S>
void GenericNode<B, S>::AppendUse(Use* use) {
  use->next = NULL;
  use->prev = last_use_;
  if (last_use_ == NULL) {
    first_use_ = use;
  } else {
    last_use_->next = use;
  }
  last_use_ = use;
  ++use_count_;
}

template <class B, class S>
void GenericNode<B, S>::RemoveUse(Use* use) {
  if (last_use_ == use) {
    last_use_ = use->prev;
  }
  if (use->prev != NULL) {
    use->prev->next = use->next;
  } else {
    first_use_ = use->next;
  }
  if (use->next != NULL) {
    use->next->prev = use->prev;
  }
  --use_count_;
}

template <class B, class S>
inline bool GenericNode<B, S>::OwnedBy(GenericNode* owner) const {
  return first_use_ != NULL && first_use_->from == owner &&
         first_use_->next == NULL;
}

template <class B, class S>
S* GenericNode<B, S>::New(GenericGraphBase* graph, int input_count, S** inputs,
                          bool has_extensible_inputs) {
  size_t node_size = sizeof(GenericNode);
  int reserve_input_count = has_extensible_inputs ? kDefaultReservedInputs : 0;
  size_t inputs_size = (input_count + reserve_input_count) * sizeof(Input);
  size_t uses_size = input_count * sizeof(Use);
  int size = static_cast<int>(node_size + inputs_size + uses_size);
  Zone* zone = graph->zone();
  void* buffer = zone->New(size);
  S* result = new (buffer) S(graph, input_count, reserve_input_count);
  Input* input =
      reinterpret_cast<Input*>(reinterpret_cast<char*>(buffer) + node_size);
  Use* use =
      reinterpret_cast<Use*>(reinterpret_cast<char*>(input) + inputs_size);

  for (int current = 0; current < input_count; ++current) {
    GenericNode* to = *inputs++;
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
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_GENERIC_NODE_INL_H_

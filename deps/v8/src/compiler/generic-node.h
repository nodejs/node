// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GENERIC_NODE_H_
#define V8_COMPILER_GENERIC_NODE_H_

#include <deque>

#include "src/v8.h"

#include "src/compiler/operator.h"
#include "src/zone.h"
#include "src/zone-allocator.h"

namespace v8 {
namespace internal {
namespace compiler {

class Operator;
class GenericGraphBase;

typedef int NodeId;

// A GenericNode<> is the basic primitive of graphs. GenericNode's are
// chained together by input/use chains but by default otherwise contain only an
// identifying number which specific applications of graphs and nodes can use
// to index auxiliary out-of-line data, especially transient data.
// Specializations of the templatized GenericNode<> class must provide a base
// class B that contains all of the members to be made available in each
// specialized Node instance. GenericNode uses a mixin template pattern to
// ensure that common accessors and methods expect the derived class S type
// rather than the GenericNode<B, S> type.
template <class B, class S>
class GenericNode : public B {
 public:
  typedef B BaseClass;
  typedef S DerivedClass;

  inline NodeId id() const { return id_; }

  int InputCount() const { return input_count_; }
  S* InputAt(int index) const {
    return static_cast<S*>(GetInputRecordPtr(index)->to);
  }
  void ReplaceInput(int index, GenericNode* new_input);
  void AppendInput(Zone* zone, GenericNode* new_input);
  void InsertInput(Zone* zone, int index, GenericNode* new_input);

  int UseCount() { return use_count_; }
  S* UseAt(int index) {
    DCHECK(index < use_count_);
    Use* current = first_use_;
    while (index-- != 0) {
      current = current->next;
    }
    return static_cast<S*>(current->from);
  }
  inline void ReplaceUses(GenericNode* replace_to);
  template <class UnaryPredicate>
  inline void ReplaceUsesIf(UnaryPredicate pred, GenericNode* replace_to);
  void RemoveAllInputs();

  void TrimInputCount(int input_count);

  class Inputs {
   public:
    class iterator;
    iterator begin();
    iterator end();

    explicit Inputs(GenericNode* node) : node_(node) {}

   private:
    GenericNode* node_;
  };

  Inputs inputs() { return Inputs(this); }

  class Uses {
   public:
    class iterator;
    iterator begin();
    iterator end();
    bool empty() { return begin() == end(); }

    explicit Uses(GenericNode* node) : node_(node) {}

   private:
    GenericNode* node_;
  };

  Uses uses() { return Uses(this); }

  class Edge;

  bool OwnedBy(GenericNode* owner) const;

  static S* New(GenericGraphBase* graph, int input_count, S** inputs);

 protected:
  friend class GenericGraphBase;

  class Use : public ZoneObject {
   public:
    GenericNode* from;
    Use* next;
    Use* prev;
    int input_index;
  };

  class Input {
   public:
    GenericNode* to;
    Use* use;

    void Update(GenericNode* new_to);
  };

  void EnsureAppendableInputs(Zone* zone);

  Input* GetInputRecordPtr(int index) const {
    if (has_appendable_inputs_) {
      return &((*inputs_.appendable_)[index]);
    } else {
      return inputs_.static_ + index;
    }
  }

  void AppendUse(Use* use);
  void RemoveUse(Use* use);

  void* operator new(size_t, void* location) { return location; }

  GenericNode(GenericGraphBase* graph, int input_count);

 private:
  void AssignUniqueID(GenericGraphBase* graph);

  typedef zone_allocator<Input> ZoneInputAllocator;
  typedef std::deque<Input, ZoneInputAllocator> InputDeque;

  NodeId id_;
  int input_count_ : 31;
  bool has_appendable_inputs_ : 1;
  union {
    // When a node is initially allocated, it uses a static buffer to hold its
    // inputs under the assumption that the number of outputs will not increase.
    // When the first input is appended, the static buffer is converted into a
    // deque to allow for space-efficient growing.
    Input* static_;
    InputDeque* appendable_;
  } inputs_;
  int use_count_;
  Use* first_use_;
  Use* last_use_;

  DISALLOW_COPY_AND_ASSIGN(GenericNode);
};

// An encapsulation for information associated with a single use of node as a
// input from another node, allowing access to both the defining node and
// the ndoe having the input.
template <class B, class S>
class GenericNode<B, S>::Edge {
 public:
  S* from() const { return static_cast<S*>(input_->use->from); }
  S* to() const { return static_cast<S*>(input_->to); }
  int index() const {
    int index = input_->use->input_index;
    DCHECK(index < input_->use->from->input_count_);
    return index;
  }

 private:
  friend class GenericNode<B, S>::Uses::iterator;
  friend class GenericNode<B, S>::Inputs::iterator;

  explicit Edge(typename GenericNode<B, S>::Input* input) : input_(input) {}

  typename GenericNode<B, S>::Input* input_;
};

// A forward iterator to visit the nodes which are depended upon by a node
// in the order of input.
template <class B, class S>
class GenericNode<B, S>::Inputs::iterator {
 public:
  iterator(const typename GenericNode<B, S>::Inputs::iterator& other)  // NOLINT
      : node_(other.node_),
        index_(other.index_) {}

  S* operator*() { return static_cast<S*>(GetInput()->to); }
  typename GenericNode<B, S>::Edge edge() {
    return typename GenericNode::Edge(GetInput());
  }
  bool operator==(const iterator& other) const {
    return other.index_ == index_ && other.node_ == node_;
  }
  bool operator!=(const iterator& other) const { return !(other == *this); }
  iterator& operator++() {
    DCHECK(node_ != NULL);
    DCHECK(index_ < node_->input_count_);
    ++index_;
    return *this;
  }
  int index() { return index_; }

 private:
  friend class GenericNode;

  explicit iterator(GenericNode* node, int index)
      : node_(node), index_(index) {}

  Input* GetInput() const { return node_->GetInputRecordPtr(index_); }

  GenericNode* node_;
  int index_;
};

// A forward iterator to visit the uses of a node. The uses are returned in
// the order in which they were added as inputs.
template <class B, class S>
class GenericNode<B, S>::Uses::iterator {
 public:
  iterator(const typename GenericNode<B, S>::Uses::iterator& other)  // NOLINT
      : current_(other.current_),
        index_(other.index_) {}

  S* operator*() { return static_cast<S*>(current_->from); }
  typename GenericNode<B, S>::Edge edge() {
    return typename GenericNode::Edge(CurrentInput());
  }

  bool operator==(const iterator& other) { return other.current_ == current_; }
  bool operator!=(const iterator& other) { return other.current_ != current_; }
  iterator& operator++() {
    DCHECK(current_ != NULL);
    index_++;
    current_ = current_->next;
    return *this;
  }
  iterator& UpdateToAndIncrement(GenericNode<B, S>* new_to) {
    DCHECK(current_ != NULL);
    index_++;
    typename GenericNode<B, S>::Input* input = CurrentInput();
    current_ = current_->next;
    input->Update(new_to);
    return *this;
  }
  int index() const { return index_; }

 private:
  friend class GenericNode<B, S>::Uses;

  iterator() : current_(NULL), index_(0) {}
  explicit iterator(GenericNode<B, S>* node)
      : current_(node->first_use_), index_(0) {}

  Input* CurrentInput() const {
    return current_->from->GetInputRecordPtr(current_->input_index);
  }

  typename GenericNode<B, S>::Use* current_;
  int index_;
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_GENERIC_NODE_H_

// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_NODE_H_
#define V8_COMPILER_NODE_H_

#include <deque>
#include <set>
#include <vector>

#include "src/v8.h"

#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/types.h"
#include "src/zone.h"
#include "src/zone-allocator.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class Edge;
class Graph;


// Marks are used during traversal of the graph to distinguish states of nodes.
// Each node has a mark which is a monotonically increasing integer, and a
// {NodeMarker} has a range of values that indicate states of a node.
typedef uint32_t Mark;

// NodeIds are identifying numbers for nodes that can be used to index auxiliary
// out-of-line data associated with each node.
typedef int NodeId;

// A Node is the basic primitive of graphs. Nodes are chained together by
// input/use chains but by default otherwise contain only an identifying number
// which specific applications of graphs and nodes can use to index auxiliary
// out-of-line data, especially transient data.
//
// In addition Nodes only contain a mutable Operator that may change during
// compilation, e.g. during lowering passes. Other information that needs to be
// associated with Nodes during compilation must be stored out-of-line indexed
// by the Node's id.
class Node FINAL {
 public:
  void Initialize(const Operator* op) {
    set_op(op);
    set_mark(0);
  }

  bool IsDead() const { return InputCount() > 0 && InputAt(0) == NULL; }
  void Kill();

  void CollectProjections(ZoneVector<Node*>* projections);
  Node* FindProjection(size_t projection_index);

  const Operator* op() const { return op_; }
  void set_op(const Operator* op) { op_ = op; }

  IrOpcode::Value opcode() const {
    DCHECK(op_->opcode() <= IrOpcode::kLast);
    return static_cast<IrOpcode::Value>(op_->opcode());
  }

  NodeId id() const { return id_; }

  int InputCount() const { return input_count(); }
  Node* InputAt(int index) const { return GetInputRecordPtr(index)->to; }
  inline void ReplaceInput(int index, Node* new_input);
  inline void AppendInput(Zone* zone, Node* new_input);
  inline void InsertInput(Zone* zone, int index, Node* new_input);
  inline void RemoveInput(int index);

  int UseCount() const;
  Node* UseAt(int index) const;
  inline void ReplaceUses(Node* replace_to);
  template <class UnaryPredicate>
  inline void ReplaceUsesIf(UnaryPredicate pred, Node* replace_to);
  inline void RemoveAllInputs();

  inline void TrimInputCount(int input_count);

  class InputEdges {
   public:
    class iterator;
    iterator begin() const;
    iterator end() const;
    bool empty() const;

    explicit InputEdges(Node* node) : node_(node) {}

   private:
    Node* node_;
  };

  class Inputs {
   public:
    class iterator;
    iterator begin() const;
    iterator end() const;
    bool empty() const;

    explicit Inputs(Node* node) : node_(node) {}

   private:
    Node* node_;
  };

  Inputs inputs() { return Inputs(this); }
  InputEdges input_edges() { return InputEdges(this); }

  class UseEdges {
   public:
    class iterator;
    iterator begin() const;
    iterator end() const;
    bool empty() const;

    explicit UseEdges(Node* node) : node_(node) {}

   private:
    Node* node_;
  };

  class Uses {
   public:
    class iterator;
    iterator begin() const;
    iterator end() const;
    bool empty() const;

    explicit Uses(Node* node) : node_(node) {}

   private:
    Node* node_;
  };

  Uses uses() { return Uses(this); }
  UseEdges use_edges() { return UseEdges(this); }

  bool OwnedBy(Node* owner) const;

  static Node* New(Graph* graph, int input_count, Node** inputs,
                   bool has_extensible_inputs);

 protected:
  friend class Graph;
  friend class Edge;

  class Use : public ZoneObject {
   public:
    Node* from;
    Use* next;
    Use* prev;
    int input_index;
  };

  class Input {
   public:
    Node* to;
    Use* use;

    void Update(Node* new_to);
  };

  void EnsureAppendableInputs(Zone* zone);

  Input* GetInputRecordPtr(int index) const {
    if (has_appendable_inputs()) {
      return &((*inputs_.appendable_)[index]);
    } else {
      return &inputs_.static_[index];
    }
  }

  inline void AppendUse(Use* use);
  inline void RemoveUse(Use* use);

  void* operator new(size_t, void* location) { return location; }

 private:
  inline Node(NodeId id, int input_count, int reserve_input_count);

  typedef ZoneDeque<Input> InputDeque;

  friend class NodeProperties;
  template <typename State>
  friend class NodeMarker;

  // Only NodeProperties should manipulate the bounds.
  Bounds bounds() { return bounds_; }
  void set_bounds(Bounds b) { bounds_ = b; }

  // Only NodeMarkers should manipulate the marks on nodes.
  Mark mark() { return mark_; }
  void set_mark(Mark mark) { mark_ = mark; }

  int input_count() const { return InputCountField::decode(bit_field_); }
  void set_input_count(int input_count) {
    DCHECK_LE(0, input_count);
    bit_field_ = InputCountField::update(bit_field_, input_count);
  }

  int reserved_input_count() const {
    return ReservedInputCountField::decode(bit_field_);
  }
  void set_reserved_input_count(int reserved_input_count) {
    DCHECK_LE(0, reserved_input_count);
    bit_field_ =
        ReservedInputCountField::update(bit_field_, reserved_input_count);
  }

  bool has_appendable_inputs() const {
    return HasAppendableInputsField::decode(bit_field_);
  }
  void set_has_appendable_inputs(bool has_appendable_inputs) {
    bit_field_ =
        HasAppendableInputsField::update(bit_field_, has_appendable_inputs);
  }

  typedef BitField<unsigned, 0, 29> InputCountField;
  typedef BitField<unsigned, 29, 2> ReservedInputCountField;
  typedef BitField<unsigned, 31, 1> HasAppendableInputsField;
  static const int kDefaultReservedInputs = ReservedInputCountField::kMax;

  const Operator* op_;
  Bounds bounds_;
  Mark mark_;
  NodeId id_;
  unsigned bit_field_;
  union {
    // When a node is initially allocated, it uses a static buffer to hold its
    // inputs under the assumption that the number of outputs will not increase.
    // When the first input is appended, the static buffer is converted into a
    // deque to allow for space-efficient growing.
    Input* static_;
    InputDeque* appendable_;
  } inputs_;
  Use* first_use_;
  Use* last_use_;

  DISALLOW_COPY_AND_ASSIGN(Node);
};


// An encapsulation for information associated with a single use of node as a
// input from another node, allowing access to both the defining node and
// the node having the input.
class Edge {
 public:
  Node* from() const { return input_->use->from; }
  Node* to() const { return input_->to; }
  int index() const {
    int index = input_->use->input_index;
    DCHECK(index < input_->use->from->input_count());
    return index;
  }

  bool operator==(const Edge& other) { return input_ == other.input_; }
  bool operator!=(const Edge& other) { return !(*this == other); }

  void UpdateTo(Node* new_to) { input_->Update(new_to); }

 private:
  friend class Node::Uses::iterator;
  friend class Node::Inputs::iterator;
  friend class Node::UseEdges::iterator;
  friend class Node::InputEdges::iterator;

  explicit Edge(Node::Input* input) : input_(input) {}

  Node::Input* input_;
};


// A forward iterator to visit the edges for the input dependencies of a node..
class Node::InputEdges::iterator {
 public:
  typedef std::forward_iterator_tag iterator_category;
  typedef int difference_type;
  typedef Edge value_type;
  typedef Edge* pointer;
  typedef Edge& reference;
  iterator(const Node::InputEdges::iterator& other)  // NOLINT
      : input_(other.input_) {}
  iterator() : input_(NULL) {}

  Edge operator*() const { return Edge(input_); }
  bool operator==(const iterator& other) const { return Equals(other); }
  bool operator!=(const iterator& other) const { return !Equals(other); }
  iterator& operator++() {
    DCHECK(input_ != NULL);
    Edge edge(input_);
    Node* from = edge.from();
    SetInput(from, input_->use->input_index + 1);
    return *this;
  }
  iterator operator++(int) {
    iterator result(*this);
    ++(*this);
    return result;
  }

 private:
  friend class Node;

  explicit iterator(Node* from, int index = 0) : input_(NULL) {
    SetInput(from, index);
  }

  bool Equals(const iterator& other) const { return other.input_ == input_; }
  void SetInput(Node* from, int index) {
    DCHECK(index >= 0 && index <= from->InputCount());
    if (index < from->InputCount()) {
      input_ = from->GetInputRecordPtr(index);
    } else {
      input_ = NULL;
    }
  }

  Input* input_;
};


// A forward iterator to visit the inputs of a node.
class Node::Inputs::iterator {
 public:
  typedef std::forward_iterator_tag iterator_category;
  typedef int difference_type;
  typedef Node* value_type;
  typedef Node** pointer;
  typedef Node*& reference;

  iterator(const Node::Inputs::iterator& other)  // NOLINT
      : iter_(other.iter_) {}

  Node* operator*() const { return (*iter_).to(); }
  bool operator==(const iterator& other) const { return Equals(other); }
  bool operator!=(const iterator& other) const { return !Equals(other); }
  iterator& operator++() {
    ++iter_;
    return *this;
  }
  iterator operator++(int) {
    iterator result(*this);
    ++(*this);
    return result;
  }


 private:
  friend class Node::Inputs;

  explicit iterator(Node* node, int index) : iter_(node, index) {}

  bool Equals(const iterator& other) const { return other.iter_ == iter_; }

  Node::InputEdges::iterator iter_;
};

// A forward iterator to visit the uses edges of a node. The edges are returned
// in
// the order in which they were added as inputs.
class Node::UseEdges::iterator {
 public:
  iterator(const Node::UseEdges::iterator& other)  // NOLINT
      : current_(other.current_),
        next_(other.next_) {}

  Edge operator*() const { return Edge(CurrentInput()); }

  bool operator==(const iterator& other) { return Equals(other); }
  bool operator!=(const iterator& other) { return !Equals(other); }
  iterator& operator++() {
    DCHECK(current_ != NULL);
    current_ = next_;
    next_ = (current_ == NULL) ? NULL : current_->next;
    return *this;
  }
  iterator operator++(int) {
    iterator result(*this);
    ++(*this);
    return result;
  }

 private:
  friend class Node::UseEdges;

  iterator() : current_(NULL), next_(NULL) {}
  explicit iterator(Node* node)
      : current_(node->first_use_),
        next_(current_ == NULL ? NULL : current_->next) {}

  bool Equals(const iterator& other) const {
    return other.current_ == current_;
  }

  Input* CurrentInput() const {
    return current_->from->GetInputRecordPtr(current_->input_index);
  }

  Node::Use* current_;
  Node::Use* next_;
};


// A forward iterator to visit the uses of a node. The uses are returned in
// the order in which they were added as inputs.
class Node::Uses::iterator {
 public:
  iterator(const Node::Uses::iterator& other)  // NOLINT
      : current_(other.current_) {}

  Node* operator*() { return current_->from; }

  bool operator==(const iterator& other) { return other.current_ == current_; }
  bool operator!=(const iterator& other) { return other.current_ != current_; }
  iterator& operator++() {
    DCHECK(current_ != NULL);
    current_ = current_->next;
    return *this;
  }

 private:
  friend class Node::Uses;

  iterator() : current_(NULL) {}
  explicit iterator(Node* node) : current_(node->first_use_) {}

  Input* CurrentInput() const {
    return current_->from->GetInputRecordPtr(current_->input_index);
  }

  Node::Use* current_;
};


std::ostream& operator<<(std::ostream& os, const Node& n);

typedef std::set<Node*, std::less<Node*>, zone_allocator<Node*> > NodeSet;
typedef NodeSet::iterator NodeSetIter;
typedef NodeSet::reverse_iterator NodeSetRIter;

typedef ZoneDeque<Node*> NodeDeque;

typedef ZoneVector<Node*> NodeVector;
typedef NodeVector::iterator NodeVectorIter;
typedef NodeVector::const_iterator NodeVectorConstIter;
typedef NodeVector::reverse_iterator NodeVectorRIter;

typedef ZoneVector<NodeVector> NodeVectorVector;
typedef NodeVectorVector::iterator NodeVectorVectorIter;
typedef NodeVectorVector::reverse_iterator NodeVectorVectorRIter;

typedef Node::Uses::iterator UseIter;
typedef Node::Inputs::iterator InputIter;

// Helper to extract parameters from Operator1<*> nodes.
template <typename T>
static inline const T& OpParameter(const Node* node) {
  return OpParameter<T>(node->op());
}

inline Node::InputEdges::iterator Node::InputEdges::begin() const {
  return Node::InputEdges::iterator(this->node_, 0);
}

inline Node::InputEdges::iterator Node::InputEdges::end() const {
  return Node::InputEdges::iterator(this->node_, this->node_->InputCount());
}

inline Node::Inputs::iterator Node::Inputs::begin() const {
  return Node::Inputs::iterator(this->node_, 0);
}

inline Node::Inputs::iterator Node::Inputs::end() const {
  return Node::Inputs::iterator(this->node_, this->node_->InputCount());
}

inline Node::UseEdges::iterator Node::UseEdges::begin() const {
  return Node::UseEdges::iterator(this->node_);
}

inline Node::UseEdges::iterator Node::UseEdges::end() const {
  return Node::UseEdges::iterator();
}

inline Node::Uses::iterator Node::Uses::begin() const {
  return Node::Uses::iterator(this->node_);
}

inline Node::Uses::iterator Node::Uses::end() const {
  return Node::Uses::iterator();
}

inline bool Node::InputEdges::empty() const { return begin() == end(); }
inline bool Node::Uses::empty() const { return begin() == end(); }
inline bool Node::UseEdges::empty() const { return begin() == end(); }
inline bool Node::Inputs::empty() const { return begin() == end(); }

inline void Node::ReplaceUses(Node* replace_to) {
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
  first_use_ = NULL;
  last_use_ = NULL;
}

template <class UnaryPredicate>
inline void Node::ReplaceUsesIf(UnaryPredicate pred, Node* replace_to) {
  for (Use* use = first_use_; use != NULL;) {
    Use* next = use->next;
    if (pred(use->from)) {
      RemoveUse(use);
      replace_to->AppendUse(use);
      use->from->GetInputRecordPtr(use->input_index)->to = replace_to;
    }
    use = next;
  }
}

inline void Node::RemoveAllInputs() {
  for (Edge edge : input_edges()) {
    edge.UpdateTo(NULL);
  }
}

inline void Node::TrimInputCount(int new_input_count) {
  if (new_input_count == input_count()) return;  // Nothing to do.

  DCHECK(new_input_count < input_count());

  // Update inline inputs.
  for (int i = new_input_count; i < input_count(); i++) {
    Node::Input* input = GetInputRecordPtr(i);
    input->Update(NULL);
  }
  set_input_count(new_input_count);
}

inline void Node::ReplaceInput(int index, Node* new_to) {
  Input* input = GetInputRecordPtr(index);
  input->Update(new_to);
}

inline void Node::Input::Update(Node* new_to) {
  Node* old_to = this->to;
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

inline void Node::EnsureAppendableInputs(Zone* zone) {
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

inline void Node::AppendInput(Zone* zone, Node* to_append) {
  Use* new_use = new (zone) Use;
  Input new_input;
  new_input.to = to_append;
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
  to_append->AppendUse(new_use);
  set_input_count(input_count() + 1);
}

inline void Node::InsertInput(Zone* zone, int index, Node* to_insert) {
  DCHECK(index >= 0 && index < InputCount());
  // TODO(turbofan): Optimize this implementation!
  AppendInput(zone, InputAt(InputCount() - 1));
  for (int i = InputCount() - 1; i > index; --i) {
    ReplaceInput(i, InputAt(i - 1));
  }
  ReplaceInput(index, to_insert);
}

inline void Node::RemoveInput(int index) {
  DCHECK(index >= 0 && index < InputCount());
  // TODO(turbofan): Optimize this implementation!
  for (; index < InputCount() - 1; ++index) {
    ReplaceInput(index, InputAt(index + 1));
  }
  TrimInputCount(InputCount() - 1);
}

inline void Node::AppendUse(Use* use) {
  use->next = NULL;
  use->prev = last_use_;
  if (last_use_ == NULL) {
    first_use_ = use;
  } else {
    last_use_->next = use;
  }
  last_use_ = use;
}

inline void Node::RemoveUse(Use* use) {
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
}

inline bool Node::OwnedBy(Node* owner) const {
  return first_use_ != NULL && first_use_->from == owner &&
         first_use_->next == NULL;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_NODE_H_

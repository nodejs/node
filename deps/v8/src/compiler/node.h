// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_NODE_H_
#define V8_COMPILER_NODE_H_

#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/types-inl.h"
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
typedef int32_t NodeId;


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
  static Node* New(Zone* zone, NodeId id, const Operator* op, int input_count,
                   Node** inputs, bool has_extensible_inputs);

  bool IsDead() const { return InputCount() > 0 && !InputAt(0); }
  void Kill();

  const Operator* op() const { return op_; }
  void set_op(const Operator* op) { op_ = op; }

  IrOpcode::Value opcode() const {
    DCHECK(op_->opcode() <= IrOpcode::kLast);
    return static_cast<IrOpcode::Value>(op_->opcode());
  }

  NodeId id() const { return id_; }

  int InputCount() const { return input_count(); }
  Node* InputAt(int index) const { return GetInputRecordPtr(index)->to; }
  inline void ReplaceInput(int index, Node* new_to);
  void AppendInput(Zone* zone, Node* new_to);
  void InsertInput(Zone* zone, int index, Node* new_to);
  void RemoveInput(int index);
  void RemoveAllInputs();
  void TrimInputCount(int new_input_count);

  int UseCount() const;
  Node* UseAt(int index) const;
  void ReplaceUses(Node* replace_to);

  class InputEdges FINAL {
   public:
    typedef Edge value_type;

    class iterator;
    inline iterator begin() const;
    inline iterator end() const;

    bool empty() const;

    explicit InputEdges(Node* node) : node_(node) {}

   private:
    Node* node_;
  };

  InputEdges input_edges() { return InputEdges(this); }

  class Inputs FINAL {
   public:
    typedef Node* value_type;

    class const_iterator;
    inline const_iterator begin() const;
    inline const_iterator end() const;

    bool empty() const;

    explicit Inputs(Node* node) : node_(node) {}

   private:
    Node* node_;
  };

  Inputs inputs() { return Inputs(this); }

  class UseEdges FINAL {
   public:
    typedef Edge value_type;

    class iterator;
    inline iterator begin() const;
    inline iterator end() const;

    bool empty() const;

    explicit UseEdges(Node* node) : node_(node) {}

   private:
    Node* node_;
  };

  UseEdges use_edges() { return UseEdges(this); }

  class Uses FINAL {
   public:
    typedef Node* value_type;

    class const_iterator;
    inline const_iterator begin() const;
    inline const_iterator end() const;

    bool empty() const;

    explicit Uses(Node* node) : node_(node) {}

   private:
    Node* node_;
  };

  Uses uses() { return Uses(this); }

  // Returns true if {owner} is the user of {this} node.
  bool OwnedBy(Node* owner) const {
    return first_use_ && first_use_->from == owner && !first_use_->next;
  }

 private:
  struct Use FINAL : public ZoneObject {
    Node* from;
    Use* next;
    Use* prev;
    int input_index;
  };

  class Input FINAL {
   public:
    Node* to;
    Use* use;

    void Update(Node* new_to);
  };

  inline Node(NodeId id, const Operator* op, int input_count,
              int reserve_input_count);

  inline void EnsureAppendableInputs(Zone* zone);

  Input* GetInputRecordPtr(int index) {
    return has_appendable_inputs() ? &((*inputs_.appendable_)[index])
                                   : &inputs_.static_[index];
  }
  const Input* GetInputRecordPtr(int index) const {
    return has_appendable_inputs() ? &((*inputs_.appendable_)[index])
                                   : &inputs_.static_[index];
  }

  inline void AppendUse(Use* const use);
  inline void RemoveUse(Use* const use);

  void* operator new(size_t, void* location) { return location; }

  typedef ZoneDeque<Input> InputDeque;

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
  NodeId const id_;
  unsigned bit_field_;
  Use* first_use_;
  Use* last_use_;
  union {
    // When a node is initially allocated, it uses a static buffer to hold its
    // inputs under the assumption that the number of outputs will not increase.
    // When the first input is appended, the static buffer is converted into a
    // deque to allow for space-efficient growing.
    Input static_[1];
    InputDeque* appendable_;
  } inputs_;

  friend class Edge;
  friend class NodeMarkerBase;
  friend class NodeProperties;

  DISALLOW_COPY_AND_ASSIGN(Node);
};


std::ostream& operator<<(std::ostream& os, const Node& n);


// Typedefs to shorten commonly used Node containers.
typedef ZoneDeque<Node*> NodeDeque;
typedef ZoneVector<Node*> NodeVector;
typedef ZoneVector<NodeVector> NodeVectorVector;


// Helper to extract parameters from Operator1<*> nodes.
template <typename T>
static inline const T& OpParameter(const Node* node) {
  return OpParameter<T>(node->op());
}


// An encapsulation for information associated with a single use of node as a
// input from another node, allowing access to both the defining node and
// the node having the input.
class Edge FINAL {
 public:
  Node* from() const { return input_->use->from; }
  Node* to() const { return input_->to; }
  int index() const {
    int const index = input_->use->input_index;
    DCHECK_LT(index, input_->use->from->input_count());
    return index;
  }

  bool operator==(const Edge& other) { return input_ == other.input_; }
  bool operator!=(const Edge& other) { return !(*this == other); }

  void UpdateTo(Node* new_to) { input_->Update(new_to); }

 private:
  friend class Node::UseEdges::iterator;
  friend class Node::InputEdges::iterator;

  explicit Edge(Node::Input* input) : input_(input) { DCHECK_NOT_NULL(input); }

  Node::Input* input_;
};


// A forward iterator to visit the edges for the input dependencies of a node.
class Node::InputEdges::iterator FINAL {
 public:
  typedef std::forward_iterator_tag iterator_category;
  typedef int difference_type;
  typedef Edge value_type;
  typedef Edge* pointer;
  typedef Edge& reference;

  iterator() : input_(nullptr) {}
  iterator(const iterator& other) : input_(other.input_) {}

  Edge operator*() const { return Edge(input_); }
  bool operator==(const iterator& other) const {
    return input_ == other.input_;
  }
  bool operator!=(const iterator& other) const { return !(*this == other); }
  iterator& operator++() {
    SetInput(Edge(input_).from(), input_->use->input_index + 1);
    return *this;
  }
  iterator operator++(int);

 private:
  friend class Node;

  explicit iterator(Node* from, int index = 0) : input_(nullptr) {
    SetInput(from, index);
  }

  void SetInput(Node* from, int index) {
    DCHECK(index >= 0 && index <= from->InputCount());
    if (index < from->InputCount()) {
      input_ = from->GetInputRecordPtr(index);
    } else {
      input_ = nullptr;
    }
  }

  Input* input_;
};


Node::InputEdges::iterator Node::InputEdges::begin() const {
  return Node::InputEdges::iterator(this->node_, 0);
}


Node::InputEdges::iterator Node::InputEdges::end() const {
  return Node::InputEdges::iterator(this->node_, this->node_->InputCount());
}


// A forward iterator to visit the inputs of a node.
class Node::Inputs::const_iterator FINAL {
 public:
  typedef std::forward_iterator_tag iterator_category;
  typedef int difference_type;
  typedef Node* value_type;
  typedef Node** pointer;
  typedef Node*& reference;

  const_iterator(const const_iterator& other) : iter_(other.iter_) {}

  Node* operator*() const { return (*iter_).to(); }
  bool operator==(const const_iterator& other) const {
    return iter_ == other.iter_;
  }
  bool operator!=(const const_iterator& other) const {
    return !(*this == other);
  }
  const_iterator& operator++() {
    ++iter_;
    return *this;
  }
  const_iterator operator++(int);

 private:
  friend class Node::Inputs;

  const_iterator(Node* node, int index) : iter_(node, index) {}

  Node::InputEdges::iterator iter_;
};


Node::Inputs::const_iterator Node::Inputs::begin() const {
  return const_iterator(this->node_, 0);
}


Node::Inputs::const_iterator Node::Inputs::end() const {
  return const_iterator(this->node_, this->node_->InputCount());
}


// A forward iterator to visit the uses edges of a node. The edges are returned
// in
// the order in which they were added as inputs.
class Node::UseEdges::iterator FINAL {
 public:
  iterator(const iterator& other)
      : current_(other.current_), next_(other.next_) {}

  Edge operator*() const {
    return Edge(current_->from->GetInputRecordPtr(current_->input_index));
  }

  bool operator==(const iterator& other) const {
    return current_ == other.current_;
  }
  bool operator!=(const iterator& other) const { return !(*this == other); }
  iterator& operator++() {
    DCHECK_NOT_NULL(current_);
    current_ = next_;
    next_ = current_ ? current_->next : nullptr;
    return *this;
  }
  iterator operator++(int);

 private:
  friend class Node::UseEdges;

  iterator() : current_(nullptr), next_(nullptr) {}
  explicit iterator(Node* node)
      : current_(node->first_use_),
        next_(current_ ? current_->next : nullptr) {}

  Node::Use* current_;
  Node::Use* next_;
};


Node::UseEdges::iterator Node::UseEdges::begin() const {
  return Node::UseEdges::iterator(this->node_);
}


Node::UseEdges::iterator Node::UseEdges::end() const {
  return Node::UseEdges::iterator();
}


// A forward iterator to visit the uses of a node. The uses are returned in
// the order in which they were added as inputs.
class Node::Uses::const_iterator FINAL {
 public:
  typedef std::forward_iterator_tag iterator_category;
  typedef int difference_type;
  typedef Node* value_type;
  typedef Node** pointer;
  typedef Node*& reference;

  const_iterator(const const_iterator& other) : current_(other.current_) {}

  Node* operator*() const { return current_->from; }
  bool operator==(const const_iterator& other) const {
    return other.current_ == current_;
  }
  bool operator!=(const const_iterator& other) const {
    return other.current_ != current_;
  }
  const_iterator& operator++() {
    DCHECK_NOT_NULL(current_);
    current_ = current_->next;
    return *this;
  }
  const_iterator operator++(int);

 private:
  friend class Node::Uses;

  const_iterator() : current_(nullptr) {}
  explicit const_iterator(Node* node) : current_(node->first_use_) {}

  Node::Use* current_;
};


Node::Uses::const_iterator Node::Uses::begin() const {
  return const_iterator(this->node_);
}


Node::Uses::const_iterator Node::Uses::end() const { return const_iterator(); }


void Node::ReplaceInput(int index, Node* new_to) {
  GetInputRecordPtr(index)->Update(new_to);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_NODE_H_

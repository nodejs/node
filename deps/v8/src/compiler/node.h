// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_NODE_H_
#define V8_COMPILER_NODE_H_

#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/compiler/types.h"
#include "src/zone/zone-containers.h"

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
typedef uint32_t NodeId;


// A Node is the basic primitive of graphs. Nodes are chained together by
// input/use chains but by default otherwise contain only an identifying number
// which specific applications of graphs and nodes can use to index auxiliary
// out-of-line data, especially transient data.
//
// In addition Nodes only contain a mutable Operator that may change during
// compilation, e.g. during lowering passes. Other information that needs to be
// associated with Nodes during compilation must be stored out-of-line indexed
// by the Node's id.
class Node final {
 public:
  static Node* New(Zone* zone, NodeId id, const Operator* op, int input_count,
                   Node* const* inputs, bool has_extensible_inputs);
  static Node* Clone(Zone* zone, NodeId id, const Node* node);

  bool IsDead() const { return InputCount() > 0 && !InputAt(0); }
  void Kill();

  const Operator* op() const { return op_; }

  IrOpcode::Value opcode() const {
    DCHECK(op_->opcode() <= IrOpcode::kLast);
    return static_cast<IrOpcode::Value>(op_->opcode());
  }

  NodeId id() const { return IdField::decode(bit_field_); }

  int InputCount() const {
    return has_inline_inputs() ? InlineCountField::decode(bit_field_)
                               : inputs_.outline_->count_;
  }

#if DEBUG
  void Verify();
#define BOUNDS_CHECK(index)                                                  \
  do {                                                                       \
    if (index < 0 || index >= InputCount()) {                                \
      V8_Fatal(__FILE__, __LINE__, "Node #%d:%s->InputAt(%d) out of bounds", \
               id(), op()->mnemonic(), index);                               \
    }                                                                        \
  } while (false)
#else
  // No bounds checks or verification in release mode.
  inline void Verify() {}
#define BOUNDS_CHECK(index) \
  do {                      \
  } while (false)
#endif

  Node* InputAt(int index) const {
    BOUNDS_CHECK(index);
    return *GetInputPtrConst(index);
  }

  void ReplaceInput(int index, Node* new_to) {
    BOUNDS_CHECK(index);
    Node** input_ptr = GetInputPtr(index);
    Node* old_to = *input_ptr;
    if (old_to != new_to) {
      Use* use = GetUsePtr(index);
      if (old_to) old_to->RemoveUse(use);
      *input_ptr = new_to;
      if (new_to) new_to->AppendUse(use);
    }
  }

#undef BOUNDS_CHECK

  void AppendInput(Zone* zone, Node* new_to);
  void InsertInput(Zone* zone, int index, Node* new_to);
  void InsertInputs(Zone* zone, int index, int count);
  void RemoveInput(int index);
  void NullAllInputs();
  void TrimInputCount(int new_input_count);

  int UseCount() const;
  void ReplaceUses(Node* replace_to);

  class InputEdges final {
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

  class Inputs final {
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

  class UseEdges final {
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

  class Uses final {
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
    return first_use_ && first_use_->from() == owner && !first_use_->next;
  }

  // Returns true if {owner1} and {owner2} are the only users of {this} node.
  bool OwnedBy(Node const* owner1, Node const* owner2) const;
  void Print() const;

 private:
  struct Use;
  // Out of line storage for inputs when the number of inputs overflowed the
  // capacity of the inline-allocated space.
  struct OutOfLineInputs {
    Node* node_;
    int count_;
    int capacity_;
    Node* inputs_[1];

    static OutOfLineInputs* New(Zone* zone, int capacity);
    void ExtractFrom(Use* use_ptr, Node** input_ptr, int count);
  };

  // A link in the use chain for a node. Every input {i} to a node {n} has an
  // associated {Use} which is linked into the use chain of the {i} node.
  struct Use {
    Use* next;
    Use* prev;
    uint32_t bit_field_;

    int input_index() const { return InputIndexField::decode(bit_field_); }
    bool is_inline_use() const { return InlineField::decode(bit_field_); }
    Node** input_ptr() {
      int index = input_index();
      Use* start = this + 1 + index;
      Node** inputs = is_inline_use()
                          ? reinterpret_cast<Node*>(start)->inputs_.inline_
                          : reinterpret_cast<OutOfLineInputs*>(start)->inputs_;
      return &inputs[index];
    }

    Node* from() {
      Use* start = this + 1 + input_index();
      return is_inline_use() ? reinterpret_cast<Node*>(start)
                             : reinterpret_cast<OutOfLineInputs*>(start)->node_;
    }

    typedef BitField<bool, 0, 1> InlineField;
    typedef BitField<unsigned, 1, 17> InputIndexField;
    // Leaving some space in the bitset in case we ever decide to record
    // the output index.
  };

  //============================================================================
  //== Memory layout ===========================================================
  //============================================================================
  // Saving space for big graphs is important. We use a memory layout trick to
  // be able to map {Node} objects to {Use} objects and vice-versa in a
  // space-efficient manner.
  //
  // {Use} links are laid out in memory directly before a {Node}, followed by
  // direct pointers to input {Nodes}.
  //
  // inline case:
  // |Use #N  |Use #N-1|...|Use #1  |Use #0  |Node xxxx |I#0|I#1|...|I#N-1|I#N|
  //          ^                              ^                  ^
  //          + Use                          + Node             + Input
  //
  // Since every {Use} instance records its {input_index}, pointer arithmetic
  // can compute the {Node}.
  //
  // out-of-line case:
  //     |Node xxxx |
  //     ^       + outline ------------------+
  //     +----------------------------------------+
  //                                         |    |
  //                                         v    | node
  // |Use #N  |Use #N-1|...|Use #1  |Use #0  |OOL xxxxx |I#0|I#1|...|I#N-1|I#N|
  //          ^                                                 ^
  //          + Use                                             + Input
  //
  // Out-of-line storage of input lists is needed if appending an input to
  // a node exceeds the maximum inline capacity.

  Node(NodeId id, const Operator* op, int inline_count, int inline_capacity);

  Node* const* GetInputPtrConst(int input_index) const {
    return has_inline_inputs() ? &(inputs_.inline_[input_index])
                               : &inputs_.outline_->inputs_[input_index];
  }
  Node** GetInputPtr(int input_index) {
    return has_inline_inputs() ? &(inputs_.inline_[input_index])
                               : &inputs_.outline_->inputs_[input_index];
  }
  Use* GetUsePtr(int input_index) {
    Use* ptr = has_inline_inputs() ? reinterpret_cast<Use*>(this)
                                   : reinterpret_cast<Use*>(inputs_.outline_);
    return &ptr[-1 - input_index];
  }

  void AppendUse(Use* use);
  void RemoveUse(Use* use);

  void* operator new(size_t, void* location) { return location; }

  // Only NodeProperties should manipulate the op.
  void set_op(const Operator* op) { op_ = op; }

  // Only NodeProperties should manipulate the type.
  Type* type() const { return type_; }
  void set_type(Type* type) { type_ = type; }

  // Only NodeMarkers should manipulate the marks on nodes.
  Mark mark() { return mark_; }
  void set_mark(Mark mark) { mark_ = mark; }

  inline bool has_inline_inputs() const {
    return InlineCountField::decode(bit_field_) != kOutlineMarker;
  }

  void ClearInputs(int start, int count);

  typedef BitField<NodeId, 0, 24> IdField;
  typedef BitField<unsigned, 24, 4> InlineCountField;
  typedef BitField<unsigned, 28, 4> InlineCapacityField;
  static const int kOutlineMarker = InlineCountField::kMax;
  static const int kMaxInlineCount = InlineCountField::kMax - 1;
  static const int kMaxInlineCapacity = InlineCapacityField::kMax - 1;

  const Operator* op_;
  Type* type_;
  Mark mark_;
  uint32_t bit_field_;
  Use* first_use_;
  union {
    // Inline storage for inputs or out-of-line storage.
    Node* inline_[1];
    OutOfLineInputs* outline_;
  } inputs_;

  friend class Edge;
  friend class NodeMarkerBase;
  friend class NodeProperties;

  DISALLOW_COPY_AND_ASSIGN(Node);
};


std::ostream& operator<<(std::ostream& os, const Node& n);


// Typedefs to shorten commonly used Node containers.
typedef ZoneDeque<Node*> NodeDeque;
typedef ZoneSet<Node*> NodeSet;
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
class Edge final {
 public:
  Node* from() const { return use_->from(); }
  Node* to() const { return *input_ptr_; }
  int index() const {
    int const index = use_->input_index();
    DCHECK_LT(index, use_->from()->InputCount());
    return index;
  }

  bool operator==(const Edge& other) { return input_ptr_ == other.input_ptr_; }
  bool operator!=(const Edge& other) { return !(*this == other); }

  void UpdateTo(Node* new_to) {
    Node* old_to = *input_ptr_;
    if (old_to != new_to) {
      if (old_to) old_to->RemoveUse(use_);
      *input_ptr_ = new_to;
      if (new_to) new_to->AppendUse(use_);
    }
  }

 private:
  friend class Node::UseEdges::iterator;
  friend class Node::InputEdges::iterator;

  Edge(Node::Use* use, Node** input_ptr) : use_(use), input_ptr_(input_ptr) {
    DCHECK_NOT_NULL(use);
    DCHECK_NOT_NULL(input_ptr);
    DCHECK_EQ(input_ptr, use->input_ptr());
  }

  Node::Use* use_;
  Node** input_ptr_;
};


// A forward iterator to visit the edges for the input dependencies of a node.
class Node::InputEdges::iterator final {
 public:
  typedef std::forward_iterator_tag iterator_category;
  typedef int difference_type;
  typedef Edge value_type;
  typedef Edge* pointer;
  typedef Edge& reference;

  iterator() : use_(nullptr), input_ptr_(nullptr) {}
  iterator(const iterator& other)
      : use_(other.use_), input_ptr_(other.input_ptr_) {}

  Edge operator*() const { return Edge(use_, input_ptr_); }
  bool operator==(const iterator& other) const {
    return input_ptr_ == other.input_ptr_;
  }
  bool operator!=(const iterator& other) const { return !(*this == other); }
  iterator& operator++() {
    input_ptr_++;
    use_--;
    return *this;
  }
  iterator operator++(int);

 private:
  friend class Node;

  explicit iterator(Node* from, int index = 0)
      : use_(from->GetUsePtr(index)), input_ptr_(from->GetInputPtr(index)) {}

  Use* use_;
  Node** input_ptr_;
};


Node::InputEdges::iterator Node::InputEdges::begin() const {
  return Node::InputEdges::iterator(this->node_, 0);
}


Node::InputEdges::iterator Node::InputEdges::end() const {
  return Node::InputEdges::iterator(this->node_, this->node_->InputCount());
}


// A forward iterator to visit the inputs of a node.
class Node::Inputs::const_iterator final {
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


// A forward iterator to visit the uses edges of a node.
class Node::UseEdges::iterator final {
 public:
  iterator(const iterator& other)
      : current_(other.current_), next_(other.next_) {}

  Edge operator*() const { return Edge(current_, current_->input_ptr()); }
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


// A forward iterator to visit the uses of a node.
class Node::Uses::const_iterator final {
 public:
  typedef std::forward_iterator_tag iterator_category;
  typedef int difference_type;
  typedef Node* value_type;
  typedef Node** pointer;
  typedef Node*& reference;

  const_iterator(const const_iterator& other) : current_(other.current_) {}

  Node* operator*() const { return current_->from(); }
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

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_NODE_H_

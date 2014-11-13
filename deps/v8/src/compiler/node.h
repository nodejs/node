// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_NODE_H_
#define V8_COMPILER_NODE_H_

#include <deque>
#include <set>
#include <vector>

#include "src/compiler/generic-algorithm.h"
#include "src/compiler/generic-node.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/types.h"
#include "src/zone.h"
#include "src/zone-allocator.h"

namespace v8 {
namespace internal {
namespace compiler {

class NodeData {
 public:
  const Operator* op() const { return op_; }
  void set_op(const Operator* op) { op_ = op; }

  IrOpcode::Value opcode() const {
    DCHECK(op_->opcode() <= IrOpcode::kLast);
    return static_cast<IrOpcode::Value>(op_->opcode());
  }

 protected:
  const Operator* op_;
  Bounds bounds_;
  explicit NodeData(Zone* zone) {}

  friend class NodeProperties;
  Bounds bounds() { return bounds_; }
  void set_bounds(Bounds b) { bounds_ = b; }
};

// A Node is the basic primitive of an IR graph. In addition to the members
// inherited from Vector, Nodes only contain a mutable Operator that may change
// during compilation, e.g. during lowering passes.  Other information that
// needs to be associated with Nodes during compilation must be stored
// out-of-line indexed by the Node's id.
class Node FINAL : public GenericNode<NodeData, Node> {
 public:
  Node(GenericGraphBase* graph, int input_count, int reserve_input_count)
      : GenericNode<NodeData, Node>(graph, input_count, reserve_input_count) {}

  void Initialize(const Operator* op) { set_op(op); }

  bool IsDead() const { return InputCount() > 0 && InputAt(0) == NULL; }
  void Kill();

  void CollectProjections(ZoneVector<Node*>* projections);
  Node* FindProjection(size_t projection_index);
};

std::ostream& operator<<(std::ostream& os, const Node& n);

typedef GenericGraphVisit::NullNodeVisitor<NodeData, Node> NullNodeVisitor;

typedef std::set<Node*, std::less<Node*>, zone_allocator<Node*> > NodeSet;
typedef NodeSet::iterator NodeSetIter;
typedef NodeSet::reverse_iterator NodeSetRIter;

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

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_NODE_H_

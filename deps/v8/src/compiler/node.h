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
  Operator* op() const { return op_; }
  void set_op(Operator* op) { op_ = op; }

  IrOpcode::Value opcode() const {
    DCHECK(op_->opcode() <= IrOpcode::kLast);
    return static_cast<IrOpcode::Value>(op_->opcode());
  }

  Bounds bounds() { return bounds_; }

 protected:
  Operator* op_;
  Bounds bounds_;
  explicit NodeData(Zone* zone) : bounds_(Bounds(Type::None(zone))) {}

  friend class NodeProperties;
  void set_bounds(Bounds b) { bounds_ = b; }
};

// A Node is the basic primitive of an IR graph. In addition to the members
// inherited from Vector, Nodes only contain a mutable Operator that may change
// during compilation, e.g. during lowering passes.  Other information that
// needs to be associated with Nodes during compilation must be stored
// out-of-line indexed by the Node's id.
class Node : public GenericNode<NodeData, Node> {
 public:
  Node(GenericGraphBase* graph, int input_count)
      : GenericNode<NodeData, Node>(graph, input_count) {}

  void Initialize(Operator* op) { set_op(op); }

  void CollectProjections(int projection_count, Node** projections);
  Node* FindProjection(int32_t projection_index);
};

OStream& operator<<(OStream& os, const Node& n);

typedef GenericGraphVisit::NullNodeVisitor<NodeData, Node> NullNodeVisitor;

typedef zone_allocator<Node*> NodePtrZoneAllocator;

typedef std::set<Node*, std::less<Node*>, NodePtrZoneAllocator> NodeSet;
typedef NodeSet::iterator NodeSetIter;
typedef NodeSet::reverse_iterator NodeSetRIter;

typedef std::deque<Node*, NodePtrZoneAllocator> NodeDeque;
typedef NodeDeque::iterator NodeDequeIter;

typedef std::vector<Node*, NodePtrZoneAllocator> NodeVector;
typedef NodeVector::iterator NodeVectorIter;
typedef NodeVector::reverse_iterator NodeVectorRIter;

typedef zone_allocator<NodeVector> ZoneNodeVectorAllocator;
typedef std::vector<NodeVector, ZoneNodeVectorAllocator> NodeVectorVector;
typedef NodeVectorVector::iterator NodeVectorVectorIter;
typedef NodeVectorVector::reverse_iterator NodeVectorVectorRIter;

typedef Node::Uses::iterator UseIter;
typedef Node::Inputs::iterator InputIter;

// Helper to extract parameters from Operator1<*> nodes.
template <typename T>
static inline T OpParameter(Node* node) {
  return reinterpret_cast<Operator1<T>*>(node->op())->parameter();
}
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_NODE_H_

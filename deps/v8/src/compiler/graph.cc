// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/graph.h"

#include <algorithm>

#include "src/base/bits.h"
#include "src/compiler/node.h"

namespace v8 {
namespace internal {
namespace compiler {

Graph::Graph(Zone* zone)
    : zone_(zone),
      start_(nullptr),
      end_(nullptr),
      mark_max_(0),
      next_node_id_(0),
      decorators_(zone) {}


void Graph::Decorate(Node* node, bool incomplete) {
  for (auto const decorator : decorators_) {
    decorator->Decorate(node, incomplete);
  }
}


void Graph::AddDecorator(GraphDecorator* decorator) {
  decorators_.push_back(decorator);
}


void Graph::RemoveDecorator(GraphDecorator* decorator) {
  auto const it = std::find(decorators_.begin(), decorators_.end(), decorator);
  DCHECK(it != decorators_.end());
  decorators_.erase(it);
}


Node* Graph::NewNode(const Operator* op, int input_count, Node** inputs,
                     bool incomplete) {
  DCHECK_LE(op->ValueInputCount(), input_count);
  Node* const node =
      Node::New(zone(), NextNodeId(), op, input_count, inputs, incomplete);
  Decorate(node, incomplete);
  return node;
}


NodeId Graph::NextNodeId() {
  NodeId const id = next_node_id_;
  CHECK(!base::bits::SignedAddOverflow32(id, 1, &next_node_id_));
  return id;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

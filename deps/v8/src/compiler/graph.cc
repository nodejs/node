// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/graph.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/graph-inl.h"
#include "src/compiler/node.h"
#include "src/compiler/node-aux-data-inl.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

Graph::Graph(Zone* zone)
    : zone_(zone),
      start_(NULL),
      end_(NULL),
      mark_max_(0),
      next_node_id_(0),
      decorators_(zone) {}


void Graph::Decorate(Node* node) {
  for (ZoneVector<GraphDecorator*>::iterator i = decorators_.begin();
       i != decorators_.end(); ++i) {
    (*i)->Decorate(node);
  }
}


Node* Graph::NewNode(const Operator* op, int input_count, Node** inputs,
                     bool incomplete) {
  DCHECK_LE(op->ValueInputCount(), input_count);
  Node* result = Node::New(this, input_count, inputs, incomplete);
  result->Initialize(op);
  if (!incomplete) {
    Decorate(result);
  }
  return result;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/node.h"

#include "src/compiler/generic-node-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

void Node::Kill() {
  DCHECK_NOT_NULL(op());
  RemoveAllInputs();
  DCHECK(uses().empty());
}


void Node::CollectProjections(NodeVector* projections) {
  for (size_t i = 0; i < projections->size(); i++) {
    (*projections)[i] = NULL;
  }
  for (UseIter i = uses().begin(); i != uses().end(); ++i) {
    if ((*i)->opcode() != IrOpcode::kProjection) continue;
    size_t index = OpParameter<size_t>(*i);
    DCHECK_LT(index, projections->size());
    DCHECK_EQ(NULL, (*projections)[index]);
    (*projections)[index] = *i;
  }
}


Node* Node::FindProjection(size_t projection_index) {
  for (UseIter i = uses().begin(); i != uses().end(); ++i) {
    if ((*i)->opcode() == IrOpcode::kProjection &&
        OpParameter<size_t>(*i) == projection_index) {
      return *i;
    }
  }
  return NULL;
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

}  // namespace compiler
}  // namespace internal
}  // namespace v8

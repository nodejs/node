// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/node.h"

#include "src/compiler/generic-node-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

void Node::CollectProjections(int projection_count, Node** projections) {
  for (int i = 0; i < projection_count; ++i) projections[i] = NULL;
  for (UseIter i = uses().begin(); i != uses().end(); ++i) {
    if ((*i)->opcode() != IrOpcode::kProjection) continue;
    int32_t index = OpParameter<int32_t>(*i);
    DCHECK_GE(index, 0);
    DCHECK_LT(index, projection_count);
    DCHECK_EQ(NULL, projections[index]);
    projections[index] = *i;
  }
}


Node* Node::FindProjection(int32_t projection_index) {
  for (UseIter i = uses().begin(); i != uses().end(); ++i) {
    if ((*i)->opcode() == IrOpcode::kProjection &&
        OpParameter<int32_t>(*i) == projection_index) {
      return *i;
    }
  }
  return NULL;
}


OStream& operator<<(OStream& os, const Operator& op) { return op.PrintTo(os); }


OStream& operator<<(OStream& os, const Node& n) {
  os << n.id() << ": " << *n.op();
  if (n.op()->InputCount() != 0) {
    os << "(";
    for (int i = 0; i < n.op()->InputCount(); ++i) {
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

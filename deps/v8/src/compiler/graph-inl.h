// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GRAPH_INL_H_
#define V8_COMPILER_GRAPH_INL_H_

#include "src/compiler/generic-algorithm-inl.h"
#include "src/compiler/graph.h"

namespace v8 {
namespace internal {
namespace compiler {

template <class Visitor>
void Graph::VisitNodeUsesFrom(Node* node, Visitor* visitor) {
  GenericGraphVisit::Visit<Visitor, NodeUseIterationTraits<Node> >(this, node,
                                                                   visitor);
}


template <class Visitor>
void Graph::VisitNodeUsesFromStart(Visitor* visitor) {
  VisitNodeUsesFrom(start(), visitor);
}


template <class Visitor>
void Graph::VisitNodeInputsFromEnd(Visitor* visitor) {
  GenericGraphVisit::Visit<Visitor, NodeInputIterationTraits<Node> >(
      this, end(), visitor);
}
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_GRAPH_INL_H_

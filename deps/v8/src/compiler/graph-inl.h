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
  Zone tmp_zone(zone()->isolate());
  GenericGraphVisit::Visit<Visitor, NodeUseIterationTraits<Node> >(
      this, &tmp_zone, node, visitor);
}


template <class Visitor>
void Graph::VisitNodeUsesFromStart(Visitor* visitor) {
  VisitNodeUsesFrom(start(), visitor);
}


template <class Visitor>
void Graph::VisitNodeInputsFromEnd(Visitor* visitor) {
  Zone tmp_zone(zone()->isolate());
  GenericGraphVisit::Visit<Visitor, NodeInputIterationTraits<Node> >(
      this, &tmp_zone, end(), visitor);
}
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_GRAPH_INL_H_

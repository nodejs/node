// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GRAPH_INL_H_
#define V8_COMPILER_GRAPH_INL_H_

#include "src/compiler/generic-algorithm.h"
#include "src/compiler/graph.h"

namespace v8 {
namespace internal {
namespace compiler {

template <class Visitor>
void Graph::VisitNodeInputsFromEnd(Visitor* visitor) {
  Zone tmp_zone;
  GenericGraphVisit::Visit<Visitor>(this, &tmp_zone, end(), visitor);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_GRAPH_INL_H_

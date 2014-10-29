// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GENERIC_ALGORITHM_INL_H_
#define V8_COMPILER_GENERIC_ALGORITHM_INL_H_

#include <vector>

#include "src/compiler/generic-algorithm.h"
#include "src/compiler/generic-graph.h"
#include "src/compiler/generic-node.h"
#include "src/compiler/generic-node-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

template <class N>
class NodeInputIterationTraits {
 public:
  typedef N Node;
  typedef typename N::Inputs::iterator Iterator;

  static Iterator begin(Node* node) { return node->inputs().begin(); }
  static Iterator end(Node* node) { return node->inputs().end(); }
  static int max_id(GenericGraphBase* graph) { return graph->NodeCount(); }
  static Node* to(Iterator iterator) { return *iterator; }
  static Node* from(Iterator iterator) { return iterator.edge().from(); }
};

template <class N>
class NodeUseIterationTraits {
 public:
  typedef N Node;
  typedef typename N::Uses::iterator Iterator;

  static Iterator begin(Node* node) { return node->uses().begin(); }
  static Iterator end(Node* node) { return node->uses().end(); }
  static int max_id(GenericGraphBase* graph) { return graph->NodeCount(); }
  static Node* to(Iterator iterator) { return *iterator; }
  static Node* from(Iterator iterator) { return iterator.edge().to(); }
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_GENERIC_ALGORITHM_INL_H_

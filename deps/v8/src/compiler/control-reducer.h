// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CONTROL_REDUCER_H_
#define V8_COMPILER_CONTROL_REDUCER_H_

namespace v8 {
namespace internal {

// Forward declarations.
class Zone;


namespace compiler {

// Forward declarations.
class JSGraph;
class CommonOperatorBuilder;
class Node;

class ControlReducer {
 public:
  // Perform branch folding and dead code elimination on the graph.
  static void ReduceGraph(Zone* zone, JSGraph* graph,
                          int max_phis_for_select = 0);

  // Trim nodes in the graph that are not reachable from end.
  static void TrimGraph(Zone* zone, JSGraph* graph);

  // Reduces a single merge node and attached phis.
  static Node* ReduceMerge(JSGraph* graph, Node* node,
                           int max_phis_for_select = 0);

  // Testing interface.
  static Node* ReducePhiForTesting(JSGraph* graph, Node* node);
  static Node* ReduceIfNodeForTesting(JSGraph* graph, Node* node);
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_CONTROL_REDUCER_H_

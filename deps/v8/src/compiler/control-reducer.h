// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CONTROL_REDUCER_H_
#define V8_COMPILER_CONTROL_REDUCER_H_

namespace v8 {
namespace internal {
namespace compiler {

class JSGraph;
class CommonOperatorBuilder;
class Node;

class ControlReducer {
 public:
  // Perform branch folding and dead code elimination on the graph.
  static void ReduceGraph(Zone* zone, JSGraph* graph,
                          CommonOperatorBuilder* builder);

  // Trim nodes in the graph that are not reachable from end.
  static void TrimGraph(Zone* zone, JSGraph* graph);

  // Testing interface.
  static Node* ReducePhiForTesting(JSGraph* graph,
                                   CommonOperatorBuilder* builder, Node* node);
  static Node* ReduceBranchForTesting(JSGraph* graph,
                                      CommonOperatorBuilder* builder,
                                      Node* node);
  static Node* ReduceMergeForTesting(JSGraph* graph,
                                     CommonOperatorBuilder* builder,
                                     Node* node);
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_CONTROL_REDUCER_H_

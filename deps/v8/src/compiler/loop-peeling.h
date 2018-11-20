// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_LOOP_PEELING_H_
#define V8_COMPILER_LOOP_PEELING_H_

#include "src/compiler/loop-analysis.h"

namespace v8 {
namespace internal {
namespace compiler {

// Represents the output of peeling a loop, which is basically the mapping
// from the body of the loop to the corresponding nodes in the peeled
// iteration.
class PeeledIteration : public ZoneObject {
 public:
  // Maps {node} to its corresponding copy in the peeled iteration, if
  // the node was part of the body of the loop. Returns {node} otherwise.
  Node* map(Node* node);

 protected:
  PeeledIteration() {}
};

class CommonOperatorBuilder;

// Implements loop peeling.
class LoopPeeler {
 public:
  static PeeledIteration* Peel(Graph* graph, CommonOperatorBuilder* common,
                               LoopTree* loop_tree, LoopTree::Loop* loop,
                               Zone* tmp_zone);
};


}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_LOOP_PEELING_H_

// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_LOOP_PEELING_H_
#define V8_COMPILER_LOOP_PEELING_H_

#include "src/base/compiler-specific.h"
#include "src/compiler/loop-analysis.h"
#include "src/globals.h"

namespace v8 {
namespace internal {
namespace compiler {

// Represents the output of peeling a loop, which is basically the mapping
// from the body of the loop to the corresponding nodes in the peeled
// iteration.
class V8_EXPORT_PRIVATE PeeledIteration : public NON_EXPORTED_BASE(ZoneObject) {
 public:
  // Maps {node} to its corresponding copy in the peeled iteration, if
  // the node was part of the body of the loop. Returns {node} otherwise.
  Node* map(Node* node);

 protected:
  PeeledIteration() {}
};

class CommonOperatorBuilder;

// Implements loop peeling.
class V8_EXPORT_PRIVATE LoopPeeler {
 public:
  static bool CanPeel(LoopTree* loop_tree, LoopTree::Loop* loop);
  static PeeledIteration* Peel(Graph* graph, CommonOperatorBuilder* common,
                               LoopTree* loop_tree, LoopTree::Loop* loop,
                               Zone* tmp_zone);
  static void PeelInnerLoopsOfTree(Graph* graph, CommonOperatorBuilder* common,
                                   LoopTree* loop_tree, Zone* tmp_zone);

  static void EliminateLoopExits(Graph* graph, Zone* temp_zone);
  static const size_t kMaxPeeledNodes = 1000;
};


}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_LOOP_PEELING_H_

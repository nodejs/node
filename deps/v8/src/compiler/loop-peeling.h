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

class SourcePositionTable;

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
  LoopPeeler(Graph* graph, CommonOperatorBuilder* common, LoopTree* loop_tree,
             Zone* tmp_zone, SourcePositionTable* source_positions)
      : graph_(graph),
        common_(common),
        loop_tree_(loop_tree),
        tmp_zone_(tmp_zone),
        source_positions_(source_positions) {}
  bool CanPeel(LoopTree::Loop* loop);
  PeeledIteration* Peel(LoopTree::Loop* loop);
  void PeelInnerLoopsOfTree();

  static void EliminateLoopExits(Graph* graph, Zone* tmp_zone);
  static const size_t kMaxPeeledNodes = 1000;

 private:
  Graph* const graph_;
  CommonOperatorBuilder* const common_;
  LoopTree* const loop_tree_;
  Zone* const tmp_zone_;
  SourcePositionTable* const source_positions_;

  void PeelInnerLoops(LoopTree::Loop* loop);
};


}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_LOOP_PEELING_H_

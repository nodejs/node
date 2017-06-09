// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_DEAD_CODE_ELIMINATION_H_
#define V8_COMPILER_DEAD_CODE_ELIMINATION_H_

#include "src/base/compiler-specific.h"
#include "src/compiler/graph-reducer.h"
#include "src/globals.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;


// Propagates {Dead} control through the graph and thereby removes dead code.
// Note that this does not include trimming dead uses from the graph, and it
// also does not include detecting dead code by any other means than seeing a
// {Dead} control input; that is left to other reducers.
class V8_EXPORT_PRIVATE DeadCodeElimination final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  DeadCodeElimination(Editor* editor, Graph* graph,
                      CommonOperatorBuilder* common);
  ~DeadCodeElimination() final {}

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceEnd(Node* node);
  Reduction ReduceLoopOrMerge(Node* node);
  Reduction ReduceLoopExit(Node* node);
  Reduction ReduceNode(Node* node);

  Reduction RemoveLoopExit(Node* node);

  void TrimMergeOrPhi(Node* node, int size);

  Graph* graph() const { return graph_; }
  CommonOperatorBuilder* common() const { return common_; }
  Node* dead() const { return dead_; }

  Graph* const graph_;
  CommonOperatorBuilder* const common_;
  Node* const dead_;

  DISALLOW_COPY_AND_ASSIGN(DeadCodeElimination);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_DEAD_CODE_ELIMINATION_H_

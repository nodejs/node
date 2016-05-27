// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_LOAD_ELIMINATION_H_
#define V8_COMPILER_LOAD_ELIMINATION_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

class CommonOperatorBuilder;
class Graph;

class LoadElimination final : public AdvancedReducer {
 public:
  explicit LoadElimination(Editor* editor, Graph* graph,
                           CommonOperatorBuilder* common)
      : AdvancedReducer(editor), graph_(graph), common_(common) {}
  ~LoadElimination() final;

  Reduction Reduce(Node* node) final;

 private:
  CommonOperatorBuilder* common() const { return common_; }
  Graph* graph() { return graph_; }

  Reduction ReduceLoadField(Node* node);
  Graph* graph_;
  CommonOperatorBuilder* common_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_LOAD_ELIMINATION_H_

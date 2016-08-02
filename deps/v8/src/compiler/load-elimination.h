// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_LOAD_ELIMINATION_H_
#define V8_COMPILER_LOAD_ELIMINATION_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

class Graph;
class SimplifiedOperatorBuilder;

class LoadElimination final : public AdvancedReducer {
 public:
  explicit LoadElimination(Editor* editor, Graph* graph,
                           SimplifiedOperatorBuilder* simplified)
      : AdvancedReducer(editor), graph_(graph), simplified_(simplified) {}
  ~LoadElimination() final;

  Reduction Reduce(Node* node) final;

 private:
  SimplifiedOperatorBuilder* simplified() const { return simplified_; }
  Graph* graph() const { return graph_; }

  Reduction ReduceLoadField(Node* node);

  Graph* const graph_;
  SimplifiedOperatorBuilder* const simplified_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_LOAD_ELIMINATION_H_

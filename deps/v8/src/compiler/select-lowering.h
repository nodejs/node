// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SELECT_LOWERING_H_
#define V8_COMPILER_SELECT_LOWERING_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class Graph;


// Lowers Select nodes to diamonds.
class SelectLowering final : public Reducer {
 public:
  SelectLowering(Graph* graph, CommonOperatorBuilder* common);
  ~SelectLowering();

  Reduction Reduce(Node* node) override;

 private:
  CommonOperatorBuilder* common() const { return common_; }
  Graph* graph() const { return graph_; }

  CommonOperatorBuilder* common_;
  Graph* graph_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_SELECT_LOWERING_H_

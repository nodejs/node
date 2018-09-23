// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TAIL_CALL_OPTIMIZATION_H_
#define V8_COMPILER_TAIL_CALL_OPTIMIZATION_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class Graph;


// Performs tail call optimization by replacing certain combinations of Return
// and Call nodes with a single TailCall.
class TailCallOptimization final : public Reducer {
 public:
  TailCallOptimization(CommonOperatorBuilder* common, Graph* graph)
      : common_(common), graph_(graph) {}

  Reduction Reduce(Node* node) final;

 private:
  CommonOperatorBuilder* common() const { return common_; }
  Graph* graph() const { return graph_; }

  CommonOperatorBuilder* const common_;
  Graph* const graph_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_TAIL_CALL_OPTIMIZATION_H_

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_BUILTIN_REDUCER_H_
#define V8_COMPILER_JS_BUILTIN_REDUCER_H_

#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

class JSBuiltinReducer FINAL : public Reducer {
 public:
  explicit JSBuiltinReducer(JSGraph* jsgraph)
      : jsgraph_(jsgraph), simplified_(jsgraph->zone()) {}
  virtual ~JSBuiltinReducer() {}

  virtual Reduction Reduce(Node* node) OVERRIDE;

 private:
  JSGraph* jsgraph() const { return jsgraph_; }
  Graph* graph() const { return jsgraph_->graph(); }
  CommonOperatorBuilder* common() const { return jsgraph_->common(); }
  MachineOperatorBuilder* machine() const { return jsgraph_->machine(); }
  SimplifiedOperatorBuilder* simplified() { return &simplified_; }

  Reduction ReduceMathAbs(Node* node);
  Reduction ReduceMathSqrt(Node* node);
  Reduction ReduceMathMax(Node* node);
  Reduction ReduceMathImul(Node* node);
  Reduction ReduceMathFround(Node* node);

  JSGraph* jsgraph_;
  SimplifiedOperatorBuilder simplified_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_BUILTIN_REDUCER_H_

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_BUILTIN_REDUCER_H_
#define V8_COMPILER_JS_BUILTIN_REDUCER_H_

#include "src/compiler/graph-reducer.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class JSGraph;
class MachineOperatorBuilder;


class JSBuiltinReducer FINAL : public Reducer {
 public:
  explicit JSBuiltinReducer(JSGraph* jsgraph);
  ~JSBuiltinReducer() FINAL {}

  Reduction Reduce(Node* node) FINAL;

 private:
  Reduction ReduceMathMax(Node* node);
  Reduction ReduceMathImul(Node* node);
  Reduction ReduceMathFround(Node* node);

  JSGraph* jsgraph() const { return jsgraph_; }
  Graph* graph() const;
  CommonOperatorBuilder* common() const;
  MachineOperatorBuilder* machine() const;
  SimplifiedOperatorBuilder* simplified() { return &simplified_; }

  JSGraph* jsgraph_;
  SimplifiedOperatorBuilder simplified_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_BUILTIN_REDUCER_H_

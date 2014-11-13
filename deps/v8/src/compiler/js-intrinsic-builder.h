// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_INTRINSIC_BUILDER_H_
#define V8_COMPILER_JS_INTRINSIC_BUILDER_H_

#include "src/compiler/js-graph.h"
#include "src/v8.h"

namespace v8 {
namespace internal {
namespace compiler {

typedef std::pair<Node*, Node*> ResultAndEffect;

class JSIntrinsicBuilder {
 public:
  explicit JSIntrinsicBuilder(JSGraph* jsgraph) : jsgraph_(jsgraph) {}

  ResultAndEffect BuildGraphFor(Runtime::FunctionId id,
                                const NodeVector& arguments);

 private:
  ResultAndEffect BuildMapCheck(Node* object, Node* effect,
                                InstanceType map_type);
  ResultAndEffect BuildGraphFor_IsSmi(const NodeVector& arguments);
  ResultAndEffect BuildGraphFor_IsNonNegativeSmi(const NodeVector& arguments);
  ResultAndEffect BuildGraphFor_ValueOf(const NodeVector& arguments);


  Graph* graph() const { return jsgraph_->graph(); }
  CommonOperatorBuilder* common() const { return jsgraph_->common(); }
  JSGraph* jsgraph_;
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_JS_INTRINSIC_BUILDER_H_

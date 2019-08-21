// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_ADD_TYPE_ASSERTIONS_REDUCER_H_
#define V8_COMPILER_ADD_TYPE_ASSERTIONS_REDUCER_H_

#include "src/common/globals.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-aux-data.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {

namespace compiler {

class V8_EXPORT_PRIVATE AddTypeAssertionsReducer final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  AddTypeAssertionsReducer(Editor* editor, JSGraph* jsgraph, Zone* zone);
  ~AddTypeAssertionsReducer() final;

  const char* reducer_name() const override {
    return "AddTypeAssertionsReducer";
  }

  Reduction Reduce(Node* node) final;

 private:
  JSGraph* const jsgraph_;
  NodeAuxData<bool> visited_;

  Graph* graph() { return jsgraph_->graph(); }
  SimplifiedOperatorBuilder* simplified() { return jsgraph_->simplified(); }

  DISALLOW_COPY_AND_ASSIGN(AddTypeAssertionsReducer);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_ADD_TYPE_ASSERTIONS_REDUCER_H_

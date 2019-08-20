// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CONSTANT_FOLDING_REDUCER_H_
#define V8_COMPILER_CONSTANT_FOLDING_REDUCER_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class JSGraph;

class V8_EXPORT_PRIVATE ConstantFoldingReducer final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  ConstantFoldingReducer(Editor* editor, JSGraph* jsgraph,
                         JSHeapBroker* broker);
  ~ConstantFoldingReducer() final;

  const char* reducer_name() const override { return "ConstantFoldingReducer"; }

  Reduction Reduce(Node* node) final;

 private:
  JSGraph* jsgraph() const { return jsgraph_; }
  JSHeapBroker* broker() const { return broker_; }

  JSGraph* const jsgraph_;
  JSHeapBroker* const broker_;

  DISALLOW_COPY_AND_ASSIGN(ConstantFoldingReducer);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CONSTANT_FOLDING_REDUCER_H_

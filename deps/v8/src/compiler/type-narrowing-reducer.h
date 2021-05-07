// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TYPE_NARROWING_REDUCER_H_
#define V8_COMPILER_TYPE_NARROWING_REDUCER_H_

#include "src/base/compiler-specific.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/operation-typer.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class JSGraph;

class V8_EXPORT_PRIVATE TypeNarrowingReducer final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  TypeNarrowingReducer(Editor* editor, JSGraph* jsgraph, JSHeapBroker* broker);
  ~TypeNarrowingReducer() final;
  TypeNarrowingReducer(const TypeNarrowingReducer&) = delete;
  TypeNarrowingReducer& operator=(const TypeNarrowingReducer&) = delete;

  const char* reducer_name() const override { return "TypeNarrowingReducer"; }

  Reduction Reduce(Node* node) final;

 private:
  JSGraph* jsgraph() const { return jsgraph_; }
  Graph* graph() const;
  Zone* zone() const;

  JSGraph* const jsgraph_;
  OperationTyper op_typer_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_TYPE_NARROWING_REDUCER_H_

// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CHECK_ELIMINATION_H_
#define V8_COMPILER_CHECK_ELIMINATION_H_

#include "src/base/compiler-specific.h"
#include "src/compiler/graph-reducer.h"
#include "src/globals.h"

namespace v8 {
namespace internal {
namespace compiler {

class JSGraph;

// Performs elimination of redundant checks within the graph due to inlined
// constants.
class V8_EXPORT_PRIVATE CheckElimination final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  explicit CheckElimination(Editor* editor, JSGraph* jsgraph)
      : AdvancedReducer(editor), jsgraph_(jsgraph) {}
  ~CheckElimination() final {}

  const char* reducer_name() const override { return "CheckElimination"; }

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceCheckHeapObject(Node* node);
  Reduction ReduceCheckString(Node* node);
  Reduction ReduceCheckSeqString(Node* node);
  Reduction ReduceCheckNonEmptyString(Node* node);

  JSGraph* jsgraph() const { return jsgraph_; }

  JSGraph* jsgraph_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CHECK_ELIMINATION_H_

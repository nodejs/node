// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_INLINING_H_
#define V8_COMPILER_JS_INLINING_H_

#include "src/compiler/js-graph.h"
#include "src/v8.h"

namespace v8 {
namespace internal {
namespace compiler {

class JSCallFunctionAccessor;

class JSInliner {
 public:
  JSInliner(Zone* local_zone, CompilationInfo* info, JSGraph* jsgraph)
      : local_zone_(local_zone), info_(info), jsgraph_(jsgraph) {}

  void Inline();
  void TryInlineJSCall(Node* node);
  void TryInlineRuntimeCall(Node* node);

 private:
  friend class InlinerVisitor;
  Zone* local_zone_;
  CompilationInfo* info_;
  JSGraph* jsgraph_;

  Node* CreateArgumentsAdaptorFrameState(JSCallFunctionAccessor* call,
                                         Handle<JSFunction> jsfunction,
                                         Zone* temp_zone);
  void AddClosureToFrameState(Node* frame_state, Handle<JSFunction> jsfunction);
  static void UnifyReturn(Graph* graph);
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_JS_INLINING_H_

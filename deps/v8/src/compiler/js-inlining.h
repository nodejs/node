// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_INLINING_H_
#define V8_COMPILER_JS_INLINING_H_

#include "src/compiler/js-graph.h"
#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {

// Forward declarations.
class CompilationInfo;

namespace compiler {

// The JSInliner provides the core graph inlining machinery. Note that this
// class only deals with the mechanics of how to inline one graph into another,
// heuristics that decide what and how much to inline are beyond its scope.
class JSInliner final : public AdvancedReducer {
 public:
  JSInliner(Editor* editor, Zone* local_zone, CompilationInfo* info,
            JSGraph* jsgraph)
      : AdvancedReducer(editor),
        local_zone_(local_zone),
        info_(info),
        jsgraph_(jsgraph) {}

  // Reducer interface, eagerly inlines everything.
  Reduction Reduce(Node* node) final;

  // Can be used by inlining heuristics or by testing code directly, without
  // using the above generic reducer interface of the inlining machinery.
  Reduction ReduceJSCall(Node* node, Handle<JSFunction> function);

 private:
  Zone* local_zone_;
  CompilationInfo* info_;
  JSGraph* jsgraph_;

  Node* CreateArtificialFrameState(Node* node, Node* outer_frame_state,
                                   int parameter_count,
                                   FrameStateType frame_state_type,
                                   Handle<SharedFunctionInfo> shared);

  Reduction InlineCall(Node* call, Node* new_target, Node* context,
                       Node* frame_state, Node* start, Node* end);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_INLINING_H_

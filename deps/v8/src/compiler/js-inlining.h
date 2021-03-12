// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_INLINING_H_
#define V8_COMPILER_JS_INLINING_H_

#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-graph.h"

namespace v8 {
namespace internal {

class BytecodeOffset;
class OptimizedCompilationInfo;

namespace compiler {

class SourcePositionTable;

// The JSInliner provides the core graph inlining machinery. Note that this
// class only deals with the mechanics of how to inline one graph into another,
// heuristics that decide what and how much to inline are beyond its scope.
class JSInliner final : public AdvancedReducer {
 public:
  JSInliner(Editor* editor, Zone* local_zone, OptimizedCompilationInfo* info,
            JSGraph* jsgraph, JSHeapBroker* broker,
            SourcePositionTable* source_positions)
      : AdvancedReducer(editor),
        local_zone_(local_zone),
        info_(info),
        jsgraph_(jsgraph),
        broker_(broker),
        source_positions_(source_positions) {}

  const char* reducer_name() const override { return "JSInliner"; }

  Reduction Reduce(Node* node) final { UNREACHABLE(); }

  // Can be used by inlining heuristics or by testing code directly, without
  // using the above generic reducer interface of the inlining machinery.
  Reduction ReduceJSCall(Node* node);

  Reduction ReduceJSWasmCall(Node* node);

 private:
  Zone* zone() const { return local_zone_; }
  CommonOperatorBuilder* common() const;
  JSOperatorBuilder* javascript() const;
  SimplifiedOperatorBuilder* simplified() const;
  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  // TODO(neis): Make heap broker a component of JSGraph?
  JSHeapBroker* broker() const { return broker_; }
  Isolate* isolate() const { return jsgraph_->isolate(); }

  Zone* const local_zone_;
  OptimizedCompilationInfo* info_;
  JSGraph* const jsgraph_;
  JSHeapBroker* const broker_;
  SourcePositionTable* const source_positions_;

  base::Optional<SharedFunctionInfoRef> DetermineCallTarget(Node* node);
  FeedbackCellRef DetermineCallContext(Node* node, Node** context_out);

  FrameState CreateArtificialFrameState(
      Node* node, FrameState outer_frame_state, int parameter_count,
      BytecodeOffset bailout_id, FrameStateType frame_state_type,
      SharedFunctionInfoRef shared, Node* context = nullptr);

  Reduction InlineCall(Node* call, Node* new_target, Node* context,
                       Node* frame_state, StartNode start, Node* end,
                       Node* exception_target,
                       const NodeVector& uncaught_subcalls, int argument_count);

  Reduction InlineJSWasmCall(Node* call, Node* new_target, Node* context,
                             Node* frame_state, StartNode start, Node* end,
                             Node* exception_target,
                             const NodeVector& uncaught_subcalls);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_INLINING_H_

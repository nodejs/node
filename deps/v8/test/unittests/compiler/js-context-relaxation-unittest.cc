// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-context-relaxation.h"
#include "src/compiler/js-graph.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

class JSContextRelaxationTest : public GraphTest {
 public:
  JSContextRelaxationTest() : GraphTest(3), javascript_(zone()) {}
  ~JSContextRelaxationTest() override {}

 protected:
  Reduction Reduce(Node* node, MachineOperatorBuilder::Flags flags =
                                   MachineOperatorBuilder::kNoFlags) {
    MachineOperatorBuilder machine(zone(), kMachPtr, flags);
    JSGraph jsgraph(isolate(), graph(), common(), javascript(), &machine);
    // TODO(titzer): mock the GraphReducer here for better unit testing.
    GraphReducer graph_reducer(zone(), graph());
    JSContextRelaxation reducer;
    return reducer.Reduce(node);
  }

  Node* EmptyFrameState() {
    MachineOperatorBuilder machine(zone());
    JSGraph jsgraph(isolate(), graph(), common(), javascript(), &machine);
    return jsgraph.EmptyFrameState();
  }

  Node* ShallowFrameStateChain(Node* outer_context,
                               ContextCallingMode context_calling_mode) {
    const FrameStateFunctionInfo* const frame_state_function_info =
        common()->CreateFrameStateFunctionInfo(
            FrameStateType::kJavaScriptFunction, 3, 0,
            Handle<SharedFunctionInfo>(), context_calling_mode);
    const Operator* op = common()->FrameState(BailoutId::None(),
                                              OutputFrameStateCombine::Ignore(),
                                              frame_state_function_info);
    return graph()->NewNode(op, graph()->start(), graph()->start(),
                            graph()->start(), outer_context, graph()->start(),
                            graph()->start());
  }

  Node* DeepFrameStateChain(Node* outer_context,
                            ContextCallingMode context_calling_mode) {
    const FrameStateFunctionInfo* const frame_state_function_info =
        common()->CreateFrameStateFunctionInfo(
            FrameStateType::kJavaScriptFunction, 3, 0,
            Handle<SharedFunctionInfo>(), context_calling_mode);
    const Operator* op = common()->FrameState(BailoutId::None(),
                                              OutputFrameStateCombine::Ignore(),
                                              frame_state_function_info);
    Node* shallow_frame_state =
        ShallowFrameStateChain(outer_context, CALL_MAINTAINS_NATIVE_CONTEXT);
    return graph()->NewNode(op, graph()->start(), graph()->start(),
                            graph()->start(), graph()->start(),
                            graph()->start(), shallow_frame_state);
  }

  JSOperatorBuilder* javascript() { return &javascript_; }

 private:
  JSOperatorBuilder javascript_;
};


TEST_F(JSContextRelaxationTest,
       RelaxJSCallFunctionShallowFrameStateChainNoCrossCtx) {
  Node* const input0 = Parameter(0);
  Node* const input1 = Parameter(1);
  Node* const context = Parameter(2);
  Node* const outer_context = Parameter(3);
  Node* const frame_state =
      ShallowFrameStateChain(outer_context, CALL_MAINTAINS_NATIVE_CONTEXT);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Node* node =
      graph()->NewNode(javascript()->CallFunction(2, NO_CALL_FUNCTION_FLAGS,
                                                  STRICT, VectorSlotPair()),
                       input0, input1, context, frame_state, effect, control);
  Reduction const r = Reduce(node);
  EXPECT_TRUE(r.Changed());
  EXPECT_EQ(outer_context, NodeProperties::GetContextInput(node));
}

TEST_F(JSContextRelaxationTest,
       RelaxJSCallFunctionShallowFrameStateChainCrossCtx) {
  Node* const input0 = Parameter(0);
  Node* const input1 = Parameter(1);
  Node* const context = Parameter(2);
  Node* const outer_context = Parameter(3);
  Node* const frame_state =
      ShallowFrameStateChain(outer_context, CALL_CHANGES_NATIVE_CONTEXT);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Node* node =
      graph()->NewNode(javascript()->CallFunction(2, NO_CALL_FUNCTION_FLAGS,
                                                  STRICT, VectorSlotPair()),
                       input0, input1, context, frame_state, effect, control);
  Reduction const r = Reduce(node);
  EXPECT_FALSE(r.Changed());
  EXPECT_EQ(context, NodeProperties::GetContextInput(node));
}

TEST_F(JSContextRelaxationTest,
       RelaxJSCallFunctionDeepFrameStateChainNoCrossCtx) {
  Node* const input0 = Parameter(0);
  Node* const input1 = Parameter(1);
  Node* const context = Parameter(2);
  Node* const outer_context = Parameter(3);
  Node* const frame_state =
      DeepFrameStateChain(outer_context, CALL_MAINTAINS_NATIVE_CONTEXT);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Node* node =
      graph()->NewNode(javascript()->CallFunction(2, NO_CALL_FUNCTION_FLAGS,
                                                  STRICT, VectorSlotPair()),
                       input0, input1, context, frame_state, effect, control);
  Reduction const r = Reduce(node);
  EXPECT_TRUE(r.Changed());
  EXPECT_EQ(outer_context, NodeProperties::GetContextInput(node));
}

TEST_F(JSContextRelaxationTest,
       RelaxJSCallFunctionDeepFrameStateChainCrossCtx) {
  Node* const input0 = Parameter(0);
  Node* const input1 = Parameter(1);
  Node* const context = Parameter(2);
  Node* const outer_context = Parameter(3);
  Node* const frame_state =
      DeepFrameStateChain(outer_context, CALL_CHANGES_NATIVE_CONTEXT);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Node* node =
      graph()->NewNode(javascript()->CallFunction(2, NO_CALL_FUNCTION_FLAGS,
                                                  STRICT, VectorSlotPair()),
                       input0, input1, context, frame_state, effect, control);
  Reduction const r = Reduce(node);
  EXPECT_FALSE(r.Changed());
  EXPECT_EQ(context, NodeProperties::GetContextInput(node));
}

TEST_F(JSContextRelaxationTest,
       RelaxJSCallFunctionDeepContextChainFullRelaxForCatch) {
  Node* const input0 = Parameter(0);
  Node* const input1 = Parameter(1);
  Node* const context = Parameter(2);
  Node* const outer_context = Parameter(3);
  const Operator* op = javascript()->CreateCatchContext(Unique<String>());
  Node* const frame_state_1 =
      ShallowFrameStateChain(outer_context, CALL_MAINTAINS_NATIVE_CONTEXT);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Node* nested_context =
      graph()->NewNode(op, graph()->start(), graph()->start(), outer_context,
                       frame_state_1, effect, control);
  Node* const frame_state_2 =
      ShallowFrameStateChain(nested_context, CALL_MAINTAINS_NATIVE_CONTEXT);
  Node* node =
      graph()->NewNode(javascript()->CallFunction(2, NO_CALL_FUNCTION_FLAGS,
                                                  STRICT, VectorSlotPair()),
                       input0, input1, context, frame_state_2, effect, control);
  Reduction const r = Reduce(node);
  EXPECT_TRUE(r.Changed());
  EXPECT_EQ(outer_context, NodeProperties::GetContextInput(node));
}


TEST_F(JSContextRelaxationTest,
       RelaxJSCallFunctionDeepContextChainFullRelaxForWith) {
  Node* const input0 = Parameter(0);
  Node* const input1 = Parameter(1);
  Node* const context = Parameter(2);
  Node* const outer_context = Parameter(3);
  const Operator* op = javascript()->CreateWithContext();
  Node* const frame_state_1 =
      ShallowFrameStateChain(outer_context, CALL_MAINTAINS_NATIVE_CONTEXT);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Node* nested_context =
      graph()->NewNode(op, graph()->start(), graph()->start(), outer_context,
                       frame_state_1, effect, control);
  Node* const frame_state_2 =
      ShallowFrameStateChain(nested_context, CALL_MAINTAINS_NATIVE_CONTEXT);
  Node* node =
      graph()->NewNode(javascript()->CallFunction(2, NO_CALL_FUNCTION_FLAGS,
                                                  STRICT, VectorSlotPair()),
                       input0, input1, context, frame_state_2, effect, control);
  Reduction const r = Reduce(node);
  EXPECT_TRUE(r.Changed());
  EXPECT_EQ(outer_context, NodeProperties::GetContextInput(node));
}


TEST_F(JSContextRelaxationTest,
       RelaxJSCallFunctionDeepContextChainFullRelaxForBlock) {
  Node* const input0 = Parameter(0);
  Node* const input1 = Parameter(1);
  Node* const context = Parameter(2);
  Node* const outer_context = Parameter(3);
  const Operator* op = javascript()->CreateBlockContext();
  Node* const frame_state_1 =
      ShallowFrameStateChain(outer_context, CALL_MAINTAINS_NATIVE_CONTEXT);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Node* nested_context =
      graph()->NewNode(op, graph()->start(), graph()->start(), outer_context,
                       frame_state_1, effect, control);
  Node* const frame_state_2 =
      ShallowFrameStateChain(nested_context, CALL_MAINTAINS_NATIVE_CONTEXT);
  Node* node =
      graph()->NewNode(javascript()->CallFunction(2, NO_CALL_FUNCTION_FLAGS,
                                                  STRICT, VectorSlotPair()),
                       input0, input1, context, frame_state_2, effect, control);
  Reduction const r = Reduce(node);
  EXPECT_TRUE(r.Changed());
  EXPECT_EQ(outer_context, NodeProperties::GetContextInput(node));
}


TEST_F(JSContextRelaxationTest,
       RelaxJSCallFunctionDeepContextChainPartialRelaxForScript) {
  Node* const input0 = Parameter(0);
  Node* const input1 = Parameter(1);
  Node* const context = Parameter(2);
  Node* const outer_context = Parameter(3);
  const Operator* op = javascript()->CreateScriptContext();
  Node* const frame_state_1 =
      ShallowFrameStateChain(outer_context, CALL_MAINTAINS_NATIVE_CONTEXT);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Node* nested_context =
      graph()->NewNode(op, graph()->start(), graph()->start(), outer_context,
                       frame_state_1, effect, control);
  Node* const frame_state_2 =
      ShallowFrameStateChain(nested_context, CALL_MAINTAINS_NATIVE_CONTEXT);
  Node* node =
      graph()->NewNode(javascript()->CallFunction(2, NO_CALL_FUNCTION_FLAGS,
                                                  STRICT, VectorSlotPair()),
                       input0, input1, context, frame_state_2, effect, control);
  Reduction const r = Reduce(node);
  EXPECT_TRUE(r.Changed());
  EXPECT_EQ(nested_context, NodeProperties::GetContextInput(node));
}


TEST_F(JSContextRelaxationTest,
       RelaxJSCallFunctionDeepContextChainPartialRelaxForModule) {
  Node* const input0 = Parameter(0);
  Node* const input1 = Parameter(1);
  Node* const context = Parameter(2);
  Node* const outer_context = Parameter(3);
  const Operator* op = javascript()->CreateModuleContext();
  Node* const frame_state_1 =
      ShallowFrameStateChain(outer_context, CALL_MAINTAINS_NATIVE_CONTEXT);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Node* nested_context =
      graph()->NewNode(op, graph()->start(), graph()->start(), outer_context,
                       frame_state_1, effect, control);
  Node* const frame_state_2 =
      ShallowFrameStateChain(nested_context, CALL_MAINTAINS_NATIVE_CONTEXT);
  Node* node =
      graph()->NewNode(javascript()->CallFunction(2, NO_CALL_FUNCTION_FLAGS,
                                                  STRICT, VectorSlotPair()),
                       input0, input1, context, frame_state_2, effect, control);
  Reduction const r = Reduce(node);
  EXPECT_TRUE(r.Changed());
  EXPECT_EQ(nested_context, NodeProperties::GetContextInput(node));
}


TEST_F(JSContextRelaxationTest,
       RelaxJSCallFunctionDeepContextChainPartialNoRelax) {
  Node* const input0 = Parameter(0);
  Node* const input1 = Parameter(1);
  Node* const context = Parameter(2);
  Node* const outer_context = Parameter(3);
  const Operator* op = javascript()->CreateFunctionContext();
  Node* const frame_state_1 =
      ShallowFrameStateChain(outer_context, CALL_MAINTAINS_NATIVE_CONTEXT);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Node* nested_context =
      graph()->NewNode(op, graph()->start(), graph()->start(), outer_context,
                       frame_state_1, effect, control);
  Node* const frame_state_2 =
      ShallowFrameStateChain(nested_context, CALL_MAINTAINS_NATIVE_CONTEXT);
  Node* node =
      graph()->NewNode(javascript()->CallFunction(2, NO_CALL_FUNCTION_FLAGS,
                                                  STRICT, VectorSlotPair()),
                       input0, input1, context, frame_state_2, effect, control);
  Reduction const r = Reduce(node);
  EXPECT_FALSE(r.Changed());
  EXPECT_EQ(context, NodeProperties::GetContextInput(node));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

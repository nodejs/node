// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-create-lowering.h"
#include "src/code-factory.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/feedback-vector.h"
#include "src/isolate-inl.h"
#include "src/objects/arguments.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::_;
using testing::BitEq;
using testing::IsNaN;

namespace v8 {
namespace internal {
namespace compiler {

class JSCreateLoweringTest : public TypedGraphTest {
 public:
  JSCreateLoweringTest()
      : TypedGraphTest(3),
        javascript_(zone()),
        deps_(broker(), zone()),
        handle_scope_(isolate()) {}
  ~JSCreateLoweringTest() override = default;

 protected:
  Reduction Reduce(Node* node) {
    MachineOperatorBuilder machine(zone());
    SimplifiedOperatorBuilder simplified(zone());
    JSGraph jsgraph(isolate(), graph(), common(), javascript(), &simplified,
                    &machine);
    // TODO(titzer): mock the GraphReducer here for better unit testing.
    GraphReducer graph_reducer(zone(), graph());
    JSCreateLowering reducer(&graph_reducer, &deps_, &jsgraph, broker(),
                             zone());
    return reducer.Reduce(node);
  }

  Node* FrameState(Handle<SharedFunctionInfo> shared, Node* outer_frame_state) {
    Node* state_values =
        graph()->NewNode(common()->StateValues(0, SparseInputMask::Dense()));
    return graph()->NewNode(
        common()->FrameState(
            BailoutId::None(), OutputFrameStateCombine::Ignore(),
            common()->CreateFrameStateFunctionInfo(
                FrameStateType::kInterpretedFunction, 1, 0, shared)),
        state_values, state_values, state_values, NumberConstant(0),
        UndefinedConstant(), outer_frame_state);
  }

  JSOperatorBuilder* javascript() { return &javascript_; }

 private:
  JSOperatorBuilder javascript_;
  CompilationDependencies deps_;
  CanonicalHandleScope handle_scope_;
};

// -----------------------------------------------------------------------------
// JSCreate

TEST_F(JSCreateLoweringTest, JSCreate) {
  Handle<JSFunction> function = isolate()->object_function();
  Node* const target = graph()->NewNode(common()->HeapConstant(function));
  Node* const context = Parameter(Type::Any());
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r =
      Reduce(graph()->NewNode(javascript()->Create(), target, target, context,
                              EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(
      r.replacement(),
      IsFinishRegion(
          IsAllocate(IsNumberConstant(function->initial_map()->instance_size()),
                     IsBeginRegion(effect), control),
          _));
}

// -----------------------------------------------------------------------------
// JSCreateArguments

TEST_F(JSCreateLoweringTest, JSCreateArgumentsInlinedMapped) {
  Node* const closure = Parameter(Type::Any());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Handle<SharedFunctionInfo> shared(isolate()->regexp_function()->shared(),
                                    isolate());
  Node* const frame_state_outer = FrameState(shared, graph()->start());
  Node* const frame_state_inner = FrameState(shared, frame_state_outer);
  Reduction r = Reduce(graph()->NewNode(
      javascript()->CreateArguments(CreateArgumentsType::kMappedArguments),
      closure, context, frame_state_inner, effect));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(
      r.replacement(),
      IsFinishRegion(
          IsAllocate(IsNumberConstant(JSSloppyArgumentsObject::kSize), _, _),
          _));
}

TEST_F(JSCreateLoweringTest, JSCreateArgumentsInlinedUnmapped) {
  Node* const closure = Parameter(Type::Any());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Handle<SharedFunctionInfo> shared(isolate()->regexp_function()->shared(),
                                    isolate());
  Node* const frame_state_outer = FrameState(shared, graph()->start());
  Node* const frame_state_inner = FrameState(shared, frame_state_outer);
  Reduction r = Reduce(graph()->NewNode(
      javascript()->CreateArguments(CreateArgumentsType::kUnmappedArguments),
      closure, context, frame_state_inner, effect));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(
      r.replacement(),
      IsFinishRegion(
          IsAllocate(IsNumberConstant(JSStrictArgumentsObject::kSize), _, _),
          _));
}

TEST_F(JSCreateLoweringTest, JSCreateArgumentsInlinedRestArray) {
  Node* const closure = Parameter(Type::Any());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Handle<SharedFunctionInfo> shared(isolate()->regexp_function()->shared(),
                                    isolate());
  Node* const frame_state_outer = FrameState(shared, graph()->start());
  Node* const frame_state_inner = FrameState(shared, frame_state_outer);
  Reduction r = Reduce(graph()->NewNode(
      javascript()->CreateArguments(CreateArgumentsType::kRestParameter),
      closure, context, frame_state_inner, effect));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(
      r.replacement(),
      IsFinishRegion(IsAllocate(IsNumberConstant(JSArray::kSize), _, _), _));
}

// -----------------------------------------------------------------------------
// JSCreateFunctionContext

TEST_F(JSCreateLoweringTest, JSCreateFunctionContextViaInlinedAllocation) {
  Node* const context = Parameter(Type::Any());
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction const r = Reduce(graph()->NewNode(
      javascript()->CreateFunctionContext(
          handle(ScopeInfo::Empty(isolate()), isolate()), 8, FUNCTION_SCOPE),
      context, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsFinishRegion(IsAllocate(IsNumberConstant(Context::SizeFor(
                                            8 + Context::MIN_CONTEXT_SLOTS)),
                                        IsBeginRegion(_), control),
                             _));
}

// -----------------------------------------------------------------------------
// JSCreateWithContext

TEST_F(JSCreateLoweringTest, JSCreateWithContext) {
  Handle<ScopeInfo> scope_info = ScopeInfo::CreateForEmptyFunction(isolate());
  Node* const object = Parameter(Type::Receiver());
  Node* const context = Parameter(Type::Any());
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r =
      Reduce(graph()->NewNode(javascript()->CreateWithContext(scope_info),
                              object, context, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsFinishRegion(IsAllocate(IsNumberConstant(Context::SizeFor(
                                            Context::MIN_CONTEXT_SLOTS)),
                                        IsBeginRegion(_), control),
                             _));
}

// -----------------------------------------------------------------------------
// JSCreateCatchContext

TEST_F(JSCreateLoweringTest, JSCreateCatchContext) {
  Handle<ScopeInfo> scope_info = ScopeInfo::CreateForEmptyFunction(isolate());
  Node* const exception = Parameter(Type::Receiver());
  Node* const context = Parameter(Type::Any());
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r =
      Reduce(graph()->NewNode(javascript()->CreateCatchContext(scope_info),
                              exception, context, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsFinishRegion(IsAllocate(IsNumberConstant(Context::SizeFor(
                                            Context::MIN_CONTEXT_SLOTS + 1)),
                                        IsBeginRegion(_), control),
                             _));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

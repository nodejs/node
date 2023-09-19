// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-intrinsic-lowering.h"

#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/simplified-operator.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "testing/gmock-support.h"

using testing::_;
using testing::AllOf;
using testing::BitEq;
using testing::Capture;
using testing::CaptureEq;


namespace v8 {
namespace internal {
namespace compiler {

class JSIntrinsicLoweringTest : public GraphTest {
 public:
  JSIntrinsicLoweringTest() : GraphTest(3), javascript_(zone()) {}
  ~JSIntrinsicLoweringTest() override = default;

 protected:
  Reduction Reduce(Node* node) {
    MachineOperatorBuilder machine(zone(),
                                   MachineType::PointerRepresentation());
    SimplifiedOperatorBuilder simplified(zone());
    JSGraph jsgraph(isolate(), graph(), common(), javascript(), &simplified,
                    &machine);
    GraphReducer graph_reducer(zone(), graph(), tick_counter(), broker());
    JSIntrinsicLowering reducer(&graph_reducer, &jsgraph, broker());
    return reducer.Reduce(node);
  }

  JSOperatorBuilder* javascript() { return &javascript_; }

 private:
  JSOperatorBuilder javascript_;
};


// -----------------------------------------------------------------------------
// %_CreateJSGeneratorObject

TEST_F(JSIntrinsicLoweringTest, InlineCreateJSGeneratorObject) {
  Node* const function = Parameter(0);
  Node* const receiver = Parameter(1);
  Node* const context = Parameter(2);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction const r = Reduce(graph()->NewNode(
      javascript()->CallRuntime(Runtime::kInlineCreateJSGeneratorObject, 2),
      function, receiver, context, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(IrOpcode::kJSCreateGeneratorObject,
            r.replacement()->op()->opcode());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

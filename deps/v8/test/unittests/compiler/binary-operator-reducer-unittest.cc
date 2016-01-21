// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/binary-operator-reducer.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/machine-type.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator.h"
#include "src/compiler/simplified-operator.h"
#include "src/types-inl.h"
#include "test/unittests/compiler/graph-reducer-unittest.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"

using testing::StrictMock;

namespace v8 {
namespace internal {
namespace compiler {

class BinaryOperatorReducerTest : public TypedGraphTest {
 public:
  explicit BinaryOperatorReducerTest(int num_parameters = 1)
      : TypedGraphTest(num_parameters), machine_(zone()), simplified_(zone()) {}
  ~BinaryOperatorReducerTest() override {}

 protected:
  Reduction Reduce(AdvancedReducer::Editor* editor, Node* node) {
    BinaryOperatorReducer reducer(editor, graph(), common(), machine());
    return reducer.Reduce(node);
  }

  Reduction Reduce(Node* node) {
    StrictMock<MockAdvancedReducerEditor> editor;
    return Reduce(&editor, node);
  }

  MachineOperatorBuilder* machine() { return &machine_; }
  SimplifiedOperatorBuilder* simplified() { return &simplified_; }

 private:
  MachineOperatorBuilder machine_;
  SimplifiedOperatorBuilder simplified_;
};


TEST_F(BinaryOperatorReducerTest, Div52OfMul52) {
  // This reduction applies only to 64bit arch
  if (!machine()->Is64()) return;

  Node* p0 = Parameter(0);
  Node* p1 = Parameter(1);
  Node* t0 = graph()->NewNode(machine()->ChangeInt32ToFloat64(), p0);
  Node* t1 = graph()->NewNode(machine()->ChangeInt32ToFloat64(), p1);

  Type* mul_range = Type::Range(0x0, 0xFFFFFFFFFFFFFULL, graph()->zone());
  Node* mul = graph()->NewNode(machine()->Float64Mul(), t0, t1);
  NodeProperties::SetType(
      mul, Type::Intersect(mul_range, Type::Number(), graph()->zone()));

  Node* mul_replacement;
  auto mul_matcher = IsInt64Mul(p0, p1);
  {
    StrictMock<MockAdvancedReducerEditor> editor;

    EXPECT_CALL(editor, Revisit(mul_matcher));

    Reduction r = Reduce(&editor, mul);
    ASSERT_TRUE(r.Changed());
    mul_replacement = r.replacement();
    EXPECT_THAT(mul_replacement, IsRoundInt64ToFloat64(mul_matcher));
  }

  {
    StrictMock<MockAdvancedReducerEditor> editor;

    Node* power = Float64Constant(0x4000000);
    Node* div =
        graph()->NewNode(machine()->Float64Div(), mul_replacement, power);

    auto shr_matcher = IsWord64Shr(mul_matcher, IsInt64Constant(26));
    EXPECT_CALL(editor, Revisit(shr_matcher));

    Reduction r = Reduce(&editor, div);
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsRoundInt64ToFloat64(shr_matcher));
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

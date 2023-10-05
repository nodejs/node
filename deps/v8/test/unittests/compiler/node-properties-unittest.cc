// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/node-properties.h"

#include "src/compiler/common-operator.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace v8 {
namespace internal {
namespace compiler {
namespace node_properties_unittest {

using testing::AnyOf;
using testing::ElementsAre;
using testing::IsNull;

class NodePropertiesTest : public TestWithZone {
 public:
  NodePropertiesTest() : TestWithZone(kCompressGraphZone) {}

  Node* NewMockNode(const Operator* op) {
    return Node::New(zone(), 0, op, 0, nullptr, false);
  }
  Node* NewMockNode(const Operator* op, Node* n1) {
    Node* nodes[] = {n1};
    return Node::New(zone(), 0, op, arraysize(nodes), nodes, false);
  }
  Node* NewMockNode(const Operator* op, Node* n1, Node* n2) {
    Node* nodes[] = {n1, n2};
    return Node::New(zone(), 0, op, arraysize(nodes), nodes, false);
  }
};

namespace {

const Operator kMockOperator(IrOpcode::kDead, Operator::kNoProperties,
                             "MockOperator", 0, 0, 0, 1, 1, 2);
const Operator kMockCallOperator(IrOpcode::kCall, Operator::kNoProperties,
                                 "MockCallOperator", 0, 0, 0, 0, 0, 2);

}  // namespace


TEST_F(NodePropertiesTest, ReplaceUses) {
  CommonOperatorBuilder common(zone());
  Node* node = NewMockNode(&kMockOperator);
  Node* effect = NewMockNode(&kMockOperator);
  Node* use_value = NewMockNode(common.Return(), node);
  Node* use_effect = NewMockNode(common.EffectPhi(1), node);
  Node* use_success = NewMockNode(common.IfSuccess(), node);
  Node* use_exception = NewMockNode(common.IfException(), effect, node);
  Node* r_value = NewMockNode(&kMockOperator);
  Node* r_effect = NewMockNode(&kMockOperator);
  Node* r_success = NewMockNode(&kMockOperator);
  Node* r_exception = NewMockNode(&kMockOperator);
  NodeProperties::ReplaceUses(node, r_value, r_effect, r_success, r_exception);
  EXPECT_EQ(r_value, use_value->InputAt(0));
  EXPECT_EQ(r_effect, use_effect->InputAt(0));
  EXPECT_EQ(r_success, use_success->InputAt(0));
  EXPECT_EQ(r_exception, use_exception->InputAt(1));
  EXPECT_EQ(0, node->UseCount());
  EXPECT_EQ(1, r_value->UseCount());
  EXPECT_EQ(1, r_effect->UseCount());
  EXPECT_EQ(1, r_success->UseCount());
  EXPECT_EQ(1, r_exception->UseCount());
  EXPECT_THAT(r_value->uses(), ElementsAre(use_value));
  EXPECT_THAT(r_effect->uses(), ElementsAre(use_effect));
  EXPECT_THAT(r_success->uses(), ElementsAre(use_success));
  EXPECT_THAT(r_exception->uses(), ElementsAre(use_exception));
}


TEST_F(NodePropertiesTest, FindProjection) {
  CommonOperatorBuilder common(zone());
  Node* start = NewMockNode(common.Start(1));
  Node* proj0 = NewMockNode(common.Projection(0), start);
  Node* proj1 = NewMockNode(common.Projection(1), start);
  EXPECT_EQ(proj0, NodeProperties::FindProjection(start, 0));
  EXPECT_EQ(proj1, NodeProperties::FindProjection(start, 1));
  EXPECT_THAT(NodeProperties::FindProjection(start, 2), IsNull());
  EXPECT_THAT(NodeProperties::FindProjection(start, 1234567890), IsNull());
}


TEST_F(NodePropertiesTest, CollectControlProjections_Branch) {
  Node* result[2];
  CommonOperatorBuilder common(zone());
  Node* branch = NewMockNode(common.Branch());
  Node* if_false = NewMockNode(common.IfFalse(), branch);
  Node* if_true = NewMockNode(common.IfTrue(), branch);
  NodeProperties::CollectControlProjections(branch, result, arraysize(result));
  EXPECT_EQ(if_true, result[0]);
  EXPECT_EQ(if_false, result[1]);
}


TEST_F(NodePropertiesTest, CollectControlProjections_Call) {
  Node* result[2];
  CommonOperatorBuilder common(zone());
  Node* call = NewMockNode(&kMockCallOperator);
  Node* if_ex = NewMockNode(common.IfException(), call, call);
  Node* if_ok = NewMockNode(common.IfSuccess(), call);
  NodeProperties::CollectControlProjections(call, result, arraysize(result));
  EXPECT_EQ(if_ok, result[0]);
  EXPECT_EQ(if_ex, result[1]);
}


TEST_F(NodePropertiesTest, CollectControlProjections_Switch) {
  Node* result[3];
  CommonOperatorBuilder common(zone());
  Node* sw = NewMockNode(common.Switch(3));
  Node* if_default = NewMockNode(common.IfDefault(), sw);
  Node* if_value1 = NewMockNode(common.IfValue(1), sw);
  Node* if_value2 = NewMockNode(common.IfValue(2), sw);
  NodeProperties::CollectControlProjections(sw, result, arraysize(result));
  EXPECT_THAT(result[0], AnyOf(if_value1, if_value2));
  EXPECT_THAT(result[1], AnyOf(if_value1, if_value2));
  EXPECT_EQ(if_default, result[2]);
}

}  // namespace node_properties_unittest
}  // namespace compiler
}  // namespace internal
}  // namespace v8

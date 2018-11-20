// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator.h"
#include "src/compiler/node-properties.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::AnyOf;
using testing::IsNull;

namespace v8 {
namespace internal {
namespace compiler {

typedef TestWithZone NodePropertiesTest;


TEST_F(NodePropertiesTest, FindProjection) {
  CommonOperatorBuilder common(zone());
  Node* start = Node::New(zone(), 0, common.Start(1), 0, nullptr, false);
  Node* proj0 = Node::New(zone(), 1, common.Projection(0), 1, &start, false);
  Node* proj1 = Node::New(zone(), 2, common.Projection(1), 1, &start, false);
  EXPECT_EQ(proj0, NodeProperties::FindProjection(start, 0));
  EXPECT_EQ(proj1, NodeProperties::FindProjection(start, 1));
  EXPECT_THAT(NodeProperties::FindProjection(start, 2), IsNull());
  EXPECT_THAT(NodeProperties::FindProjection(start, 1234567890), IsNull());
}


TEST_F(NodePropertiesTest, CollectControlProjections_Branch) {
  Node* result[2];
  CommonOperatorBuilder common(zone());
  Node* branch = Node::New(zone(), 1, common.Branch(), 0, nullptr, false);
  Node* if_false = Node::New(zone(), 2, common.IfFalse(), 1, &branch, false);
  Node* if_true = Node::New(zone(), 3, common.IfTrue(), 1, &branch, false);
  NodeProperties::CollectControlProjections(branch, result, arraysize(result));
  EXPECT_EQ(if_true, result[0]);
  EXPECT_EQ(if_false, result[1]);
}


TEST_F(NodePropertiesTest, CollectControlProjections_Switch) {
  Node* result[3];
  CommonOperatorBuilder common(zone());
  Node* sw = Node::New(zone(), 1, common.Switch(3), 0, nullptr, false);
  Node* if_default = Node::New(zone(), 2, common.IfDefault(), 1, &sw, false);
  Node* if_value1 = Node::New(zone(), 3, common.IfValue(1), 1, &sw, false);
  Node* if_value2 = Node::New(zone(), 4, common.IfValue(2), 1, &sw, false);
  NodeProperties::CollectControlProjections(sw, result, arraysize(result));
  EXPECT_THAT(result[0], AnyOf(if_value1, if_value2));
  EXPECT_THAT(result[1], AnyOf(if_value1, if_value2));
  EXPECT_EQ(if_default, result[2]);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

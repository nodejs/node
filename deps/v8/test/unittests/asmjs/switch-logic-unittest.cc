// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/asmjs/switch-logic.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace wasm {
class SwitchLogicTest : public TestWithZone {};

void CheckNodeValues(CaseNode* node, int begin, int end) {
  CHECK_EQ(node->begin, begin);
  CHECK_EQ(node->end, end);
}

TEST_F(SwitchLogicTest, Single_Table_Test) {
  ZoneVector<int> values(zone());
  values.push_back(14);
  values.push_back(12);
  values.push_back(15);
  values.push_back(19);
  values.push_back(18);
  values.push_back(16);
  CaseNode* root = OrderCases(&values, zone());
  CHECK_NULL(root->left);
  CHECK_NULL(root->right);
  CheckNodeValues(root, 12, 19);
}

TEST_F(SwitchLogicTest, Balanced_Tree_Test) {
  ZoneVector<int> values(zone());
  values.push_back(5);
  values.push_back(1);
  values.push_back(6);
  values.push_back(9);
  values.push_back(-4);
  CaseNode* root = OrderCases(&values, zone());
  CheckNodeValues(root, 5, 5);
  CheckNodeValues(root->left, -4, -4);
  CHECK_NULL(root->left->left);
  CheckNodeValues(root->left->right, 1, 1);
  CHECK_NULL(root->left->right->left);
  CHECK_NULL(root->left->right->right);
  CheckNodeValues(root->right, 6, 6);
  CHECK_NULL(root->right->left);
  CheckNodeValues(root->right->right, 9, 9);
  CHECK_NULL(root->right->right->left);
  CHECK_NULL(root->right->right->right);
}

TEST_F(SwitchLogicTest, Hybrid_Test) {
  ZoneVector<int> values(zone());
  values.push_back(1);
  values.push_back(2);
  values.push_back(3);
  values.push_back(4);
  values.push_back(7);
  values.push_back(10);
  values.push_back(11);
  values.push_back(12);
  values.push_back(13);
  values.push_back(16);
  CaseNode* root = OrderCases(&values, zone());
  CheckNodeValues(root, 7, 7);
  CheckNodeValues(root->left, 1, 4);
  CheckNodeValues(root->right, 10, 13);
  CheckNodeValues(root->right->right, 16, 16);
}

TEST_F(SwitchLogicTest, Single_Case) {
  ZoneVector<int> values(zone());
  values.push_back(3);
  CaseNode* root = OrderCases(&values, zone());
  CheckNodeValues(root, 3, 3);
  CHECK_NULL(root->left);
  CHECK_NULL(root->right);
}

TEST_F(SwitchLogicTest, Empty_Case) {
  ZoneVector<int> values(zone());
  CaseNode* root = OrderCases(&values, zone());
  CHECK_NULL(root);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator.h"
#include "src/compiler/diamond.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::AllOf;
using testing::Capture;
using testing::CaptureEq;

namespace v8 {
namespace internal {
namespace compiler {

class DiamondTest : public GraphTest {
 public:
  DiamondTest() : GraphTest(5) {}
};


TEST_F(DiamondTest, SimpleDiamond) {
  Node* p = Parameter(0);
  Diamond d(graph(), common(), p);
  EXPECT_THAT(d.branch, IsBranch(p, graph()->start()));
  EXPECT_THAT(d.if_true, IsIfTrue(d.branch));
  EXPECT_THAT(d.if_false, IsIfFalse(d.branch));
  EXPECT_THAT(d.merge, IsMerge(d.if_true, d.if_false));
}


TEST_F(DiamondTest, DiamondChainDiamond) {
  Node* p0 = Parameter(0);
  Node* p1 = Parameter(1);
  Diamond d0(graph(), common(), p0);
  Diamond d1(graph(), common(), p1);
  d1.Chain(d0);
  EXPECT_THAT(d1.branch, IsBranch(p1, d0.merge));
  EXPECT_THAT(d0.branch, IsBranch(p0, graph()->start()));
}


TEST_F(DiamondTest, DiamondChainNode) {
  Node* p1 = Parameter(1);
  Diamond d1(graph(), common(), p1);
  Node* other = graph()->NewNode(common()->Merge(0));
  d1.Chain(other);
  EXPECT_THAT(d1.branch, IsBranch(p1, other));
}


TEST_F(DiamondTest, DiamondChainN) {
  Node* params[5] = {Parameter(0), Parameter(1), Parameter(2), Parameter(3),
                     Parameter(4)};
  Diamond d[5] = {Diamond(graph(), common(), params[0]),
                  Diamond(graph(), common(), params[1]),
                  Diamond(graph(), common(), params[2]),
                  Diamond(graph(), common(), params[3]),
                  Diamond(graph(), common(), params[4])};

  for (int i = 1; i < 5; i++) {
    d[i].Chain(d[i - 1]);
    EXPECT_THAT(d[i].branch, IsBranch(params[i], d[i - 1].merge));
  }
}


TEST_F(DiamondTest, DiamondNested_true) {
  Node* p0 = Parameter(0);
  Node* p1 = Parameter(1);
  Diamond d0(graph(), common(), p0);
  Diamond d1(graph(), common(), p1);

  d1.Nest(d0, true);

  EXPECT_THAT(d0.branch, IsBranch(p0, graph()->start()));
  EXPECT_THAT(d0.if_true, IsIfTrue(d0.branch));
  EXPECT_THAT(d0.if_false, IsIfFalse(d0.branch));
  EXPECT_THAT(d0.merge, IsMerge(d1.merge, d0.if_false));

  EXPECT_THAT(d1.branch, IsBranch(p1, d0.if_true));
  EXPECT_THAT(d1.if_true, IsIfTrue(d1.branch));
  EXPECT_THAT(d1.if_false, IsIfFalse(d1.branch));
  EXPECT_THAT(d1.merge, IsMerge(d1.if_true, d1.if_false));
}


TEST_F(DiamondTest, DiamondNested_false) {
  Node* p0 = Parameter(0);
  Node* p1 = Parameter(1);
  Diamond d0(graph(), common(), p0);
  Diamond d1(graph(), common(), p1);

  d1.Nest(d0, false);

  EXPECT_THAT(d0.branch, IsBranch(p0, graph()->start()));
  EXPECT_THAT(d0.if_true, IsIfTrue(d0.branch));
  EXPECT_THAT(d0.if_false, IsIfFalse(d0.branch));
  EXPECT_THAT(d0.merge, IsMerge(d0.if_true, d1.merge));

  EXPECT_THAT(d1.branch, IsBranch(p1, d0.if_false));
  EXPECT_THAT(d1.if_true, IsIfTrue(d1.branch));
  EXPECT_THAT(d1.if_false, IsIfFalse(d1.branch));
  EXPECT_THAT(d1.merge, IsMerge(d1.if_true, d1.if_false));
}


TEST_F(DiamondTest, DiamondPhis) {
  Node* p0 = Parameter(0);
  Node* p1 = Parameter(1);
  Node* p2 = Parameter(2);
  Diamond d(graph(), common(), p0);

  MachineRepresentation types[] = {MachineRepresentation::kTagged,
                                   MachineRepresentation::kWord32};

  for (size_t i = 0; i < arraysize(types); i++) {
    Node* phi = d.Phi(types[i], p1, p2);

    EXPECT_THAT(d.branch, IsBranch(p0, graph()->start()));
    EXPECT_THAT(d.if_true, IsIfTrue(d.branch));
    EXPECT_THAT(d.if_false, IsIfFalse(d.branch));
    EXPECT_THAT(d.merge, IsMerge(d.if_true, d.if_false));
    EXPECT_THAT(phi, IsPhi(types[i], p1, p2, d.merge));
  }
}


TEST_F(DiamondTest, BranchHint) {
  Diamond dn(graph(), common(), Parameter(0));
  CHECK(BranchHint::kNone == BranchHintOf(dn.branch->op()));

  Diamond dt(graph(), common(), Parameter(0), BranchHint::kTrue);
  CHECK(BranchHint::kTrue == BranchHintOf(dt.branch->op()));

  Diamond df(graph(), common(), Parameter(0), BranchHint::kFalse);
  CHECK(BranchHint::kFalse == BranchHintOf(df.branch->op()));
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8

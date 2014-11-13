// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/select-lowering.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::AllOf;
using testing::Capture;
using testing::CaptureEq;

namespace v8 {
namespace internal {
namespace compiler {

class SelectLoweringTest : public GraphTest {
 public:
  SelectLoweringTest() : GraphTest(5), lowering_(graph(), common()) {}

 protected:
  Reduction Reduce(Node* node) { return lowering_.Reduce(node); }

 private:
  SelectLowering lowering_;
};


TEST_F(SelectLoweringTest, SelectWithSameConditions) {
  Node* const p0 = Parameter(0);
  Node* const p1 = Parameter(1);
  Node* const p2 = Parameter(2);
  Node* const p3 = Parameter(3);
  Node* const p4 = Parameter(4);

  Capture<Node*> branch;
  Capture<Node*> merge;
  {
    Reduction const r =
        Reduce(graph()->NewNode(common()->Select(kMachInt32), p0, p1, p2));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(
        r.replacement(),
        IsPhi(
            kMachInt32, p1, p2,
            AllOf(CaptureEq(&merge),
                  IsMerge(IsIfTrue(CaptureEq(&branch)),
                          IsIfFalse(AllOf(CaptureEq(&branch),
                                          IsBranch(p0, graph()->start())))))));
  }
  {
    Reduction const r =
        Reduce(graph()->NewNode(common()->Select(kMachInt32), p0, p3, p4));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsPhi(kMachInt32, p3, p4, CaptureEq(&merge)));
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

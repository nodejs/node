// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/checkpoint-elimination.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/operator.h"
#include "test/unittests/compiler/graph-reducer-unittest.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"

using testing::StrictMock;

namespace v8 {
namespace internal {
namespace compiler {

class CheckpointEliminationTest : public GraphTest {
 public:
  CheckpointEliminationTest() : GraphTest() {}
  ~CheckpointEliminationTest() override {}

 protected:
  Reduction Reduce(AdvancedReducer::Editor* editor, Node* node) {
    CheckpointElimination reducer(editor);
    return reducer.Reduce(node);
  }

  Reduction Reduce(Node* node) {
    StrictMock<MockAdvancedReducerEditor> editor;
    return Reduce(&editor, node);
  }
};

namespace {

const Operator kOpNoWrite(0, Operator::kNoWrite, "OpNoWrite", 0, 1, 0, 0, 1, 0);

}  // namespace

// -----------------------------------------------------------------------------
// Checkpoint

TEST_F(CheckpointEliminationTest, CheckpointChain) {
  Node* const control = graph()->start();
  Node* frame_state = EmptyFrameState();
  Node* checkpoint1 = graph()->NewNode(common()->Checkpoint(), frame_state,
                                       graph()->start(), control);
  Node* effect_link = graph()->NewNode(&kOpNoWrite, checkpoint1);
  Node* checkpoint2 = graph()->NewNode(common()->Checkpoint(), frame_state,
                                       effect_link, control);
  Reduction r = Reduce(checkpoint2);
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(effect_link, r.replacement());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

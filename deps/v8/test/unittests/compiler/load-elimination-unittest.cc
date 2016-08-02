// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/access-builder.h"
#include "src/compiler/load-elimination.h"
#include "src/compiler/simplified-operator.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

class LoadEliminationTest : public TypedGraphTest {
 public:
  LoadEliminationTest() : TypedGraphTest(3), simplified_(zone()) {}
  ~LoadEliminationTest() override {}

 protected:
  Reduction Reduce(Node* node) {
    // TODO(titzer): mock the GraphReducer here for better unit testing.
    GraphReducer graph_reducer(zone(), graph());
    LoadElimination reducer(&graph_reducer, graph(), simplified());
    return reducer.Reduce(node);
  }

  SimplifiedOperatorBuilder* simplified() { return &simplified_; }

 private:
  SimplifiedOperatorBuilder simplified_;
};


TEST_F(LoadEliminationTest, LoadFieldWithStoreField) {
  Node* object1 = Parameter(Type::Any(), 0);
  Node* object2 = Parameter(Type::Any(), 1);
  Node* value = Parameter(Type::Any(), 2);
  Node* effect = graph()->start();
  Node* control = graph()->start();

  FieldAccess access1 = AccessBuilder::ForContextSlot(42);
  Node* store1 = graph()->NewNode(simplified()->StoreField(access1), object1,
                                  value, effect, control);
  Reduction r1 = Reduce(graph()->NewNode(simplified()->LoadField(access1),
                                         object1, store1, control));
  ASSERT_TRUE(r1.Changed());
  EXPECT_EQ(value, r1.replacement());

  FieldAccess access2 = AccessBuilder::ForMap();
  Node* store2 = graph()->NewNode(simplified()->StoreField(access2), object1,
                                  object2, store1, control);
  Reduction r2 = Reduce(graph()->NewNode(simplified()->LoadField(access2),
                                         object1, store2, control));
  ASSERT_TRUE(r2.Changed());
  EXPECT_EQ(object2, r2.replacement());

  Node* store3 = graph()->NewNode(
      simplified()->StoreBuffer(BufferAccess(kExternalInt8Array)), object2,
      value, Int32Constant(10), object1, store2, control);

  Reduction r3 = Reduce(graph()->NewNode(simplified()->LoadField(access1),
                                         object2, store3, control));
  ASSERT_FALSE(r3.Changed());

  Reduction r4 = Reduce(graph()->NewNode(simplified()->LoadField(access1),
                                         object1, store3, control));
  ASSERT_TRUE(r4.Changed());
  EXPECT_EQ(value, r4.replacement());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

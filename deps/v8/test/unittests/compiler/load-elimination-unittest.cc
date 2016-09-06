// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/load-elimination.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node.h"
#include "src/compiler/simplified-operator.h"
#include "test/unittests/compiler/graph-reducer-unittest.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::_;
using testing::StrictMock;

namespace v8 {
namespace internal {
namespace compiler {

class LoadEliminationTest : public TypedGraphTest {
 public:
  LoadEliminationTest()
      : TypedGraphTest(3),
        simplified_(zone()),
        jsgraph_(isolate(), graph(), common(), nullptr, simplified(), nullptr) {
  }
  ~LoadEliminationTest() override {}

 protected:
  JSGraph* jsgraph() { return &jsgraph_; }
  SimplifiedOperatorBuilder* simplified() { return &simplified_; }

 private:
  SimplifiedOperatorBuilder simplified_;
  JSGraph jsgraph_;
};

TEST_F(LoadEliminationTest, LoadElementAndLoadElement) {
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  ElementAccess const access = {kTaggedBase, kPointerSize, Type::Any(),
                                MachineType::AnyTagged(), kNoWriteBarrier};

  StrictMock<MockAdvancedReducerEditor> editor;
  LoadElimination load_elimination(&editor, jsgraph(), zone());

  load_elimination.Reduce(graph()->start());

  Node* load1 = effect = graph()->NewNode(simplified()->LoadElement(access),
                                          object, index, effect, control);
  load_elimination.Reduce(load1);

  Node* load2 = effect = graph()->NewNode(simplified()->LoadElement(access),
                                          object, index, effect, control);
  EXPECT_CALL(editor, ReplaceWithValue(load2, load1, load1, _));
  Reduction r = load_elimination.Reduce(load2);
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(load1, r.replacement());
}

TEST_F(LoadEliminationTest, StoreElementAndLoadElement) {
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  Node* value = Parameter(Type::Any(), 2);
  ElementAccess const access = {kTaggedBase, kPointerSize, Type::Any(),
                                MachineType::AnyTagged(), kNoWriteBarrier};

  StrictMock<MockAdvancedReducerEditor> editor;
  LoadElimination load_elimination(&editor, jsgraph(), zone());

  load_elimination.Reduce(graph()->start());

  Node* store = effect =
      graph()->NewNode(simplified()->StoreElement(access), object, index, value,
                       effect, control);
  load_elimination.Reduce(store);

  Node* load = effect = graph()->NewNode(simplified()->LoadElement(access),
                                         object, index, effect, control);
  EXPECT_CALL(editor, ReplaceWithValue(load, value, store, _));
  Reduction r = load_elimination.Reduce(load);
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(value, r.replacement());
}

TEST_F(LoadEliminationTest, StoreElementAndStoreFieldAndLoadElement) {
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  Node* value = Parameter(Type::Any(), 2);
  ElementAccess const access = {kTaggedBase, kPointerSize, Type::Any(),
                                MachineType::AnyTagged(), kNoWriteBarrier};

  StrictMock<MockAdvancedReducerEditor> editor;
  LoadElimination load_elimination(&editor, jsgraph(), zone());

  load_elimination.Reduce(graph()->start());

  Node* store1 = effect =
      graph()->NewNode(simplified()->StoreElement(access), object, index, value,
                       effect, control);
  load_elimination.Reduce(store1);

  Node* store2 = effect =
      graph()->NewNode(simplified()->StoreField(AccessBuilder::ForMap()),
                       object, value, effect, control);
  load_elimination.Reduce(store2);

  Node* load = effect = graph()->NewNode(simplified()->LoadElement(access),
                                         object, index, effect, control);
  EXPECT_CALL(editor, ReplaceWithValue(load, value, store2, _));
  Reduction r = load_elimination.Reduce(load);
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(value, r.replacement());
}

TEST_F(LoadEliminationTest, LoadFieldAndLoadField) {
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  FieldAccess const access = {kTaggedBase,
                              kPointerSize,
                              MaybeHandle<Name>(),
                              Type::Any(),
                              MachineType::AnyTagged(),
                              kNoWriteBarrier};

  StrictMock<MockAdvancedReducerEditor> editor;
  LoadElimination load_elimination(&editor, jsgraph(), zone());

  load_elimination.Reduce(graph()->start());

  Node* load1 = effect = graph()->NewNode(simplified()->LoadField(access),
                                          object, effect, control);
  load_elimination.Reduce(load1);

  Node* load2 = effect = graph()->NewNode(simplified()->LoadField(access),
                                          object, effect, control);
  EXPECT_CALL(editor, ReplaceWithValue(load2, load1, load1, _));
  Reduction r = load_elimination.Reduce(load2);
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(load1, r.replacement());
}

TEST_F(LoadEliminationTest, StoreFieldAndLoadField) {
  Node* object = Parameter(Type::Any(), 0);
  Node* value = Parameter(Type::Any(), 1);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  FieldAccess access = {kTaggedBase,
                        kPointerSize,
                        MaybeHandle<Name>(),
                        Type::Any(),
                        MachineType::AnyTagged(),
                        kNoWriteBarrier};

  StrictMock<MockAdvancedReducerEditor> editor;
  LoadElimination load_elimination(&editor, jsgraph(), zone());

  load_elimination.Reduce(graph()->start());

  Node* store = effect = graph()->NewNode(simplified()->StoreField(access),
                                          object, value, effect, control);
  load_elimination.Reduce(store);

  Node* load = effect = graph()->NewNode(simplified()->LoadField(access),
                                         object, effect, control);
  EXPECT_CALL(editor, ReplaceWithValue(load, value, store, _));
  Reduction r = load_elimination.Reduce(load);
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(value, r.replacement());
}

TEST_F(LoadEliminationTest, StoreFieldAndStoreElementAndLoadField) {
  Node* object = Parameter(Type::Any(), 0);
  Node* value = Parameter(Type::Any(), 1);
  Node* index = Parameter(Type::UnsignedSmall(), 2);
  Node* effect = graph()->start();
  Node* control = graph()->start();
  FieldAccess access = {kTaggedBase,
                        kPointerSize,
                        MaybeHandle<Name>(),
                        Type::Any(),
                        MachineType::AnyTagged(),
                        kNoWriteBarrier};

  StrictMock<MockAdvancedReducerEditor> editor;
  LoadElimination load_elimination(&editor, jsgraph(), zone());

  load_elimination.Reduce(graph()->start());

  Node* store1 = effect = graph()->NewNode(simplified()->StoreField(access),
                                           object, value, effect, control);
  load_elimination.Reduce(store1);

  Node* store2 = effect = graph()->NewNode(
      simplified()->StoreElement(AccessBuilder::ForFixedArrayElement()), object,
      index, object, effect, control);
  load_elimination.Reduce(store2);

  Node* load = effect = graph()->NewNode(simplified()->LoadField(access),
                                         object, effect, control);
  EXPECT_CALL(editor, ReplaceWithValue(load, value, store2, _));
  Reduction r = load_elimination.Reduce(load);
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(value, r.replacement());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

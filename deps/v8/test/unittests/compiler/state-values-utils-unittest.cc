// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/state-values-utils.h"

#include "src/compiler/bytecode-liveness-map.h"
#include "src/utils/bit-vector.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace v8 {
namespace internal {
namespace compiler {

class StateValuesIteratorTest : public GraphTest {
 public:
  StateValuesIteratorTest() : GraphTest(3) {}

  Node* StateValuesFromVector(NodeVector* nodes) {
    int count = static_cast<int>(nodes->size());
    return graph()->NewNode(
        common()->StateValues(count, SparseInputMask::Dense()), count,
        count == 0 ? nullptr : &(nodes->front()));
  }
};


TEST_F(StateValuesIteratorTest, SimpleIteration) {
  NodeVector inputs(zone());
  const int count = 10;
  for (int i = 0; i < count; i++) {
    inputs.push_back(Int32Constant(i));
  }
  Node* state_values = StateValuesFromVector(&inputs);
  int i = 0;
  for (StateValuesAccess::TypedNode node : StateValuesAccess(state_values)) {
    EXPECT_THAT(node.node, IsInt32Constant(i));
    i++;
  }
  EXPECT_EQ(count, i);
}


TEST_F(StateValuesIteratorTest, EmptyIteration) {
  NodeVector inputs(zone());
  Node* state_values = StateValuesFromVector(&inputs);
  bool empty = true;
  for (auto node : StateValuesAccess(state_values)) {
    USE(node);
    empty = false;
  }
  EXPECT_TRUE(empty);
}


TEST_F(StateValuesIteratorTest, NestedIteration) {
  NodeVector inputs(zone());
  int count = 0;
  for (int i = 0; i < 8; i++) {
    if (i == 2) {
      // Single nested in index 2.
      NodeVector nested_inputs(zone());
      for (int j = 0; j < 8; j++) {
        nested_inputs.push_back(Int32Constant(count++));
      }
      inputs.push_back(StateValuesFromVector(&nested_inputs));
    } else if (i == 5) {
      // Double nested at index 5.
      NodeVector nested_inputs(zone());
      for (int j = 0; j < 8; j++) {
        if (j == 7) {
          NodeVector doubly_nested_inputs(zone());
          for (int k = 0; k < 2; k++) {
            doubly_nested_inputs.push_back(Int32Constant(count++));
          }
          nested_inputs.push_back(StateValuesFromVector(&doubly_nested_inputs));
        } else {
          nested_inputs.push_back(Int32Constant(count++));
        }
      }
      inputs.push_back(StateValuesFromVector(&nested_inputs));
    } else {
      inputs.push_back(Int32Constant(count++));
    }
  }
  Node* state_values = StateValuesFromVector(&inputs);
  int i = 0;
  for (StateValuesAccess::TypedNode node : StateValuesAccess(state_values)) {
    EXPECT_THAT(node.node, IsInt32Constant(i));
    i++;
  }
  EXPECT_EQ(count, i);
}


TEST_F(StateValuesIteratorTest, TreeFromVector) {
  int sizes[] = {0, 1, 2, 100, 5000, 30000};
  TRACED_FOREACH(int, count, sizes) {
    JSOperatorBuilder javascript(zone());
    MachineOperatorBuilder machine(zone());
    JSGraph jsgraph(isolate(), graph(), common(), &javascript, nullptr,
                    &machine);

    // Generate the input vector.
    NodeVector inputs(zone());
    for (int i = 0; i < count; i++) {
      inputs.push_back(Int32Constant(i));
    }

    // Build the tree.
    StateValuesCache builder(&jsgraph);
    Node* values_node = builder.GetNodeForValues(
        inputs.size() == 0 ? nullptr : &(inputs.front()), inputs.size(),
        nullptr);

    // Check the tree contents with vector.
    int i = 0;
    for (StateValuesAccess::TypedNode node : StateValuesAccess(values_node)) {
      EXPECT_THAT(node.node, IsInt32Constant(i));
      i++;
    }
    EXPECT_EQ(inputs.size(), static_cast<size_t>(i));
  }
}

TEST_F(StateValuesIteratorTest, TreeFromVectorWithLiveness) {
  int sizes[] = {0, 1, 2, 100, 5000, 30000};
  TRACED_FOREACH(int, count, sizes) {
    JSOperatorBuilder javascript(zone());
    MachineOperatorBuilder machine(zone());
    JSGraph jsgraph(isolate(), graph(), common(), &javascript, nullptr,
                    &machine);

    // Generate the input vector.
    NodeVector inputs(zone());
    for (int i = 0; i < count; i++) {
      inputs.push_back(Int32Constant(i));
    }
    // Generate the input liveness.
    BytecodeLivenessState liveness(count, zone());
    for (int i = 0; i < count; i++) {
      if (i % 3 == 0) {
        liveness.MarkRegisterLive(i);
      }
    }

    // Build the tree.
    StateValuesCache builder(&jsgraph);
    Node* values_node = builder.GetNodeForValues(
        inputs.size() == 0 ? nullptr : &(inputs.front()), inputs.size(),
        &liveness);

    // Check the tree contents with vector.
    int i = 0;
    for (StateValuesAccess::iterator it =
             StateValuesAccess(values_node).begin();
         !it.done(); ++it) {
      if (liveness.RegisterIsLive(i)) {
        EXPECT_THAT(it.node(), IsInt32Constant(i));
      } else {
        EXPECT_EQ(it.node(), nullptr);
      }
      i++;
    }
    EXPECT_EQ(inputs.size(), static_cast<size_t>(i));
  }
}

TEST_F(StateValuesIteratorTest, BuildTreeIdentical) {
  int sizes[] = {0, 1, 2, 100, 5000, 30000};
  TRACED_FOREACH(int, count, sizes) {
    JSOperatorBuilder javascript(zone());
    MachineOperatorBuilder machine(zone());
    JSGraph jsgraph(isolate(), graph(), common(), &javascript, nullptr,
                    &machine);

    // Generate the input vector.
    NodeVector inputs(zone());
    for (int i = 0; i < count; i++) {
      inputs.push_back(Int32Constant(i));
    }

    // Build two trees from the same data.
    StateValuesCache builder(&jsgraph);
    Node* node1 = builder.GetNodeForValues(
        inputs.size() == 0 ? nullptr : &(inputs.front()), inputs.size(),
        nullptr);
    Node* node2 = builder.GetNodeForValues(
        inputs.size() == 0 ? nullptr : &(inputs.front()), inputs.size(),
        nullptr);

    // The trees should be equal since the data was the same.
    EXPECT_EQ(node1, node2);
  }
}

TEST_F(StateValuesIteratorTest, BuildTreeWithLivenessIdentical) {
  int sizes[] = {0, 1, 2, 100, 5000, 30000};
  TRACED_FOREACH(int, count, sizes) {
    JSOperatorBuilder javascript(zone());
    MachineOperatorBuilder machine(zone());
    JSGraph jsgraph(isolate(), graph(), common(), &javascript, nullptr,
                    &machine);

    // Generate the input vector.
    NodeVector inputs(zone());
    for (int i = 0; i < count; i++) {
      inputs.push_back(Int32Constant(i));
    }
    // Generate the input liveness.
    BytecodeLivenessState liveness(count, zone());
    for (int i = 0; i < count; i++) {
      if (i % 3 == 0) {
        liveness.MarkRegisterLive(i);
      }
    }

    // Build two trees from the same data.
    StateValuesCache builder(&jsgraph);
    Node* node1 = builder.GetNodeForValues(
        inputs.size() == 0 ? nullptr : &(inputs.front()), inputs.size(),
        &liveness);
    Node* node2 = builder.GetNodeForValues(
        inputs.size() == 0 ? nullptr : &(inputs.front()), inputs.size(),
        &liveness);

    // The trees should be equal since the data was the same.
    EXPECT_EQ(node1, node2);
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_COMPILER_GRAPH_UNITTEST_H_
#define V8_UNITTESTS_COMPILER_GRAPH_UNITTEST_H_

#include "src/compiler/common-operator.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/graph.h"
#include "src/compiler/node-origin-table.h"
#include "src/compiler/typer.h"
#include "src/handles/handles.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace v8 {
namespace internal {

// Forward declarations.
class HeapObject;

namespace compiler {

using ::testing::Matcher;

class GraphTest : public TestWithNativeContextAndZone {
 public:
  explicit GraphTest(int num_parameters = 1);
  ~GraphTest() override;

  Node* start() { return graph()->start(); }
  Node* end() { return graph()->end(); }

  Node* Parameter(int32_t index = 0);
  Node* Parameter(Type type, int32_t index = 0);
  Node* Float32Constant(volatile float value);
  Node* Float64Constant(volatile double value);
  Node* Int32Constant(int32_t value);
  Node* Uint32Constant(uint32_t value) {
    return Int32Constant(bit_cast<int32_t>(value));
  }
  Node* Int64Constant(int64_t value);
  Node* NumberConstant(volatile double value);
  Node* HeapConstant(const Handle<HeapObject>& value);
  Node* FalseConstant();
  Node* TrueConstant();
  Node* UndefinedConstant();

  Node* EmptyFrameState();

  Matcher<Node*> IsBooleanConstant(bool value) {
    return value ? IsTrueConstant() : IsFalseConstant();
  }
  Matcher<Node*> IsFalseConstant();
  Matcher<Node*> IsTrueConstant();
  Matcher<Node*> IsNullConstant();
  Matcher<Node*> IsUndefinedConstant();

  CommonOperatorBuilder* common() { return &common_; }
  Graph* graph() { return &graph_; }
  SourcePositionTable* source_positions() { return &source_positions_; }
  NodeOriginTable* node_origins() { return &node_origins_; }
  JSHeapBroker* broker() { return &broker_; }

 private:
  CanonicalHandleScope canonical_;
  CommonOperatorBuilder common_;
  Graph graph_;
  JSHeapBroker broker_;
  SourcePositionTable source_positions_;
  NodeOriginTable node_origins_;
};


class TypedGraphTest : public GraphTest {
 public:
  explicit TypedGraphTest(int num_parameters = 1);
  ~TypedGraphTest() override;

 protected:
  Typer* typer() { return &typer_; }

 private:
  Typer typer_;
};

}  //  namespace compiler
}  //  namespace internal
}  //  namespace v8

#endif  // V8_UNITTESTS_COMPILER_GRAPH_UNITTEST_H_

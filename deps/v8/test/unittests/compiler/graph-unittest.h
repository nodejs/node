// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_COMPILER_GRAPH_UNITTEST_H_
#define V8_UNITTESTS_COMPILER_GRAPH_UNITTEST_H_

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/typer.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace v8 {
namespace internal {

// Forward declarations.
template <class T>
class Handle;
class HeapObject;
template <class T>
class Unique;

namespace compiler {

using ::testing::Matcher;


class GraphTest : public TestWithContext, public TestWithZone {
 public:
  explicit GraphTest(int parameters = 1);
  virtual ~GraphTest();

 protected:
  Node* Parameter(int32_t index);
  Node* Float32Constant(volatile float value);
  Node* Float64Constant(volatile double value);
  Node* Int32Constant(int32_t value);
  Node* Uint32Constant(uint32_t value) {
    return Int32Constant(bit_cast<int32_t>(value));
  }
  Node* Int64Constant(int64_t value);
  Node* NumberConstant(volatile double value);
  Node* HeapConstant(const Handle<HeapObject>& value);
  Node* HeapConstant(const Unique<HeapObject>& value);
  Node* FalseConstant();
  Node* TrueConstant();
  Node* UndefinedConstant();

  Matcher<Node*> IsFalseConstant();
  Matcher<Node*> IsTrueConstant();

  CommonOperatorBuilder* common() { return &common_; }
  Graph* graph() { return &graph_; }

 private:
  CommonOperatorBuilder common_;
  Graph graph_;
};


class TypedGraphTest : public GraphTest {
 public:
  explicit TypedGraphTest(int parameters = 1)
      : GraphTest(parameters), typer_(graph(), MaybeHandle<Context>()) {}

 protected:
  Typer* typer() { return &typer_; }

 private:
  Typer typer_;
};

}  //  namespace compiler
}  //  namespace internal
}  //  namespace v8

#endif  // V8_UNITTESTS_COMPILER_GRAPH_UNITTEST_H_

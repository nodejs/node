// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "src/compiler/bytecode-graph-builder.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/instruction.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/interpreter/bytecode-array-builder.h"
#include "src/parser.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "test/unittests/test-utils.h"

using ::testing::_;

namespace v8 {
namespace internal {
namespace compiler {

class BytecodeGraphBuilderTest : public TestWithIsolateAndZone {
 public:
  BytecodeGraphBuilderTest() : array_builder_(isolate(), zone()) {}

  Graph* GetCompletedGraph();

  Matcher<Node*> IsUndefinedConstant();
  Matcher<Node*> IsNullConstant();
  Matcher<Node*> IsTheHoleConstant();
  Matcher<Node*> IsFalseConstant();
  Matcher<Node*> IsTrueConstant();

  interpreter::BytecodeArrayBuilder* array_builder() { return &array_builder_; }

 private:
  interpreter::BytecodeArrayBuilder array_builder_;

  DISALLOW_COPY_AND_ASSIGN(BytecodeGraphBuilderTest);
};


Graph* BytecodeGraphBuilderTest::GetCompletedGraph() {
  MachineOperatorBuilder* machine = new (zone()) MachineOperatorBuilder(
      zone(), kMachPtr, InstructionSelector::SupportedMachineOperatorFlags());
  CommonOperatorBuilder* common = new (zone()) CommonOperatorBuilder(zone());
  JSOperatorBuilder* javascript = new (zone()) JSOperatorBuilder(zone());
  Graph* graph = new (zone()) Graph(zone());
  JSGraph* jsgraph =
      new (zone()) JSGraph(isolate(), graph, common, javascript, machine);

  Handle<String> name = factory()->NewStringFromStaticChars("test");
  Handle<String> script = factory()->NewStringFromStaticChars("test() {}");
  Handle<SharedFunctionInfo> shared_info =
      factory()->NewSharedFunctionInfo(name, MaybeHandle<Code>());
  shared_info->set_script(*factory()->NewScript(script));

  ParseInfo parse_info(zone(), shared_info);
  CompilationInfo info(&parse_info);
  Handle<BytecodeArray> bytecode_array = array_builder()->ToBytecodeArray();
  info.shared_info()->set_function_data(*bytecode_array);

  BytecodeGraphBuilder graph_builder(zone(), &info, jsgraph);
  graph_builder.CreateGraph();
  return graph;
}


Matcher<Node*> BytecodeGraphBuilderTest::IsUndefinedConstant() {
  return IsHeapConstant(factory()->undefined_value());
}


Matcher<Node*> BytecodeGraphBuilderTest::IsNullConstant() {
  return IsHeapConstant(factory()->null_value());
}


Matcher<Node*> BytecodeGraphBuilderTest::IsTheHoleConstant() {
  return IsHeapConstant(factory()->the_hole_value());
}


Matcher<Node*> BytecodeGraphBuilderTest::IsFalseConstant() {
  return IsHeapConstant(factory()->false_value());
}


Matcher<Node*> BytecodeGraphBuilderTest::IsTrueConstant() {
  return IsHeapConstant(factory()->true_value());
}


TEST_F(BytecodeGraphBuilderTest, ReturnUndefined) {
  array_builder()->set_locals_count(0);
  array_builder()->set_parameter_count(1);
  array_builder()->LoadUndefined().Return();

  Graph* graph = GetCompletedGraph();
  Node* end = graph->end();
  EXPECT_EQ(1, end->InputCount());
  Node* ret = end->InputAt(0);
  Node* effect = graph->start();
  Node* control = graph->start();
  EXPECT_THAT(ret, IsReturn(IsUndefinedConstant(), effect, control));
}


TEST_F(BytecodeGraphBuilderTest, ReturnNull) {
  array_builder()->set_locals_count(0);
  array_builder()->set_parameter_count(1);
  array_builder()->LoadNull().Return();

  Graph* graph = GetCompletedGraph();
  Node* end = graph->end();
  EXPECT_EQ(1, end->InputCount());
  Node* ret = end->InputAt(0);
  EXPECT_THAT(ret, IsReturn(IsNullConstant(), graph->start(), graph->start()));
}


TEST_F(BytecodeGraphBuilderTest, ReturnTheHole) {
  array_builder()->set_locals_count(0);
  array_builder()->set_parameter_count(1);
  array_builder()->LoadTheHole().Return();

  Graph* graph = GetCompletedGraph();
  Node* end = graph->end();
  EXPECT_EQ(1, end->InputCount());
  Node* ret = end->InputAt(0);
  Node* effect = graph->start();
  Node* control = graph->start();
  EXPECT_THAT(ret, IsReturn(IsTheHoleConstant(), effect, control));
}


TEST_F(BytecodeGraphBuilderTest, ReturnTrue) {
  array_builder()->set_locals_count(0);
  array_builder()->set_parameter_count(1);
  array_builder()->LoadTrue().Return();

  Graph* graph = GetCompletedGraph();
  Node* end = graph->end();
  EXPECT_EQ(1, end->InputCount());
  Node* ret = end->InputAt(0);
  Node* effect = graph->start();
  Node* control = graph->start();
  EXPECT_THAT(ret, IsReturn(IsTrueConstant(), effect, control));
}


TEST_F(BytecodeGraphBuilderTest, ReturnFalse) {
  array_builder()->set_locals_count(0);
  array_builder()->set_parameter_count(1);
  array_builder()->LoadFalse().Return();

  Graph* graph = GetCompletedGraph();
  Node* end = graph->end();
  EXPECT_EQ(1, end->InputCount());
  Node* ret = end->InputAt(0);
  Node* effect = graph->start();
  Node* control = graph->start();
  EXPECT_THAT(ret, IsReturn(IsFalseConstant(), effect, control));
}


TEST_F(BytecodeGraphBuilderTest, ReturnInt8) {
  static const int kValue = 3;
  array_builder()->set_locals_count(0);
  array_builder()->set_parameter_count(1);
  array_builder()->LoadLiteral(Smi::FromInt(kValue)).Return();

  Graph* graph = GetCompletedGraph();
  Node* end = graph->end();
  EXPECT_EQ(1, end->InputCount());
  Node* ret = end->InputAt(0);
  Node* effect = graph->start();
  Node* control = graph->start();
  EXPECT_THAT(ret, IsReturn(IsNumberConstant(kValue), effect, control));
}


TEST_F(BytecodeGraphBuilderTest, ReturnDouble) {
  const double kValue = 0.123456789;
  array_builder()->set_locals_count(0);
  array_builder()->set_parameter_count(1);
  array_builder()->LoadLiteral(factory()->NewHeapNumber(kValue));
  array_builder()->Return();

  Graph* graph = GetCompletedGraph();
  Node* end = graph->end();
  EXPECT_EQ(1, end->InputCount());
  Node* ret = end->InputAt(0);
  Node* effect = graph->start();
  Node* control = graph->start();
  EXPECT_THAT(ret, IsReturn(IsNumberConstant(kValue), effect, control));
}


TEST_F(BytecodeGraphBuilderTest, SimpleExpressionWithParameters) {
  array_builder()->set_locals_count(1);
  array_builder()->set_parameter_count(3);
  array_builder()
      ->LoadAccumulatorWithRegister(array_builder()->Parameter(1))
      .BinaryOperation(Token::Value::ADD, array_builder()->Parameter(2))
      .StoreAccumulatorInRegister(interpreter::Register(0))
      .Return();

  Graph* graph = GetCompletedGraph();
  Node* end = graph->end();
  EXPECT_EQ(1, end->InputCount());
  Node* ret = end->InputAt(0);
  // NB binary operation is <reg> <op> <acc>. The register represents
  // the left-hand side, which is why parameters appear in opposite
  // order to construction via the builder.
  EXPECT_THAT(ret, IsReturn(IsJSAdd(IsParameter(2), IsParameter(1)), _, _));
}


TEST_F(BytecodeGraphBuilderTest, SimpleExpressionWithRegister) {
  static const int kLeft = -655371;
  static const int kRight = +2000000;
  array_builder()->set_locals_count(1);
  array_builder()->set_parameter_count(1);
  array_builder()
      ->LoadLiteral(Smi::FromInt(kLeft))
      .StoreAccumulatorInRegister(interpreter::Register(0))
      .LoadLiteral(Smi::FromInt(kRight))
      .BinaryOperation(Token::Value::ADD, interpreter::Register(0))
      .Return();

  Graph* graph = GetCompletedGraph();
  Node* end = graph->end();
  EXPECT_EQ(1, end->InputCount());
  Node* ret = end->InputAt(0);
  EXPECT_THAT(
      ret, IsReturn(IsJSAdd(IsNumberConstant(kLeft), IsNumberConstant(kRight)),
                    _, _));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler.h"

#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/js-type-feedback.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"

#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::Capture;


namespace v8 {
namespace internal {
namespace compiler {

class JSTypeFeedbackTest : public TypedGraphTest {
 public:
  JSTypeFeedbackTest()
      : TypedGraphTest(3),
        javascript_(zone()),
        dependencies_(isolate(), zone()) {}
  ~JSTypeFeedbackTest() override { dependencies_.Rollback(); }

 protected:
  Reduction Reduce(Node* node,
                   JSTypeFeedbackSpecializer::DeoptimizationMode mode) {
    Handle<GlobalObject> global_object(
        isolate()->native_context()->global_object(), isolate());

    MachineOperatorBuilder machine(zone());
    SimplifiedOperatorBuilder simplified(zone());
    JSGraph jsgraph(isolate(), graph(), common(), javascript(), &simplified,
                    &machine);
    JSTypeFeedbackTable table(zone());
    // TODO(titzer): mock the GraphReducer here for better unit testing.
    GraphReducer graph_reducer(zone(), graph());
    JSTypeFeedbackSpecializer reducer(&graph_reducer, &jsgraph, &table, nullptr,
                                      global_object, mode, &dependencies_);
    return reducer.Reduce(node);
  }

  Node* EmptyFrameState() {
    MachineOperatorBuilder machine(zone());
    JSGraph jsgraph(isolate(), graph(), common(), javascript(), nullptr,
                    &machine);
    return jsgraph.EmptyFrameState();
  }

  JSOperatorBuilder* javascript() { return &javascript_; }

  void SetGlobalProperty(const char* string, int value) {
    SetGlobalProperty(string, Handle<Smi>(Smi::FromInt(value), isolate()));
  }

  void SetGlobalProperty(const char* string, double value) {
    SetGlobalProperty(string, isolate()->factory()->NewNumber(value));
  }

  void SetGlobalProperty(const char* string, Handle<Object> value) {
    Handle<JSObject> global(isolate()->context()->global_object(), isolate());
    Handle<String> name =
        isolate()->factory()->NewStringFromAsciiChecked(string);
    MaybeHandle<Object> result =
        JSReceiver::SetProperty(global, name, value, SLOPPY);
    result.Assert();
  }

  Node* ReturnLoadNamedFromGlobal(
      const char* string, Node* effect, Node* control,
      JSTypeFeedbackSpecializer::DeoptimizationMode mode) {
    VectorSlotPair feedback;
    Node* vector = UndefinedConstant();
    Node* context = UndefinedConstant();

    Handle<Name> name = isolate()->factory()->InternalizeUtf8String(string);
    const Operator* op = javascript()->LoadGlobal(name, feedback);
    Node* load = graph()->NewNode(op, vector, context, EmptyFrameState(),
                                  EmptyFrameState(), effect, control);
    Node* if_success = graph()->NewNode(common()->IfSuccess(), load);
    return graph()->NewNode(common()->Return(), load, load, if_success);
  }

  CompilationDependencies* dependencies() { return &dependencies_; }

 private:
  JSOperatorBuilder javascript_;
  CompilationDependencies dependencies_;
};


TEST_F(JSTypeFeedbackTest, JSLoadNamedGlobalConstSmi) {
  const int kValue = 111;
  const char* kName = "banana";
  SetGlobalProperty(kName, kValue);

  Node* ret = ReturnLoadNamedFromGlobal(
      kName, graph()->start(), graph()->start(),
      JSTypeFeedbackSpecializer::kDeoptimizationDisabled);
  graph()->SetEnd(graph()->NewNode(common()->End(1), ret));

  Reduction r = Reduce(ret->InputAt(0),
                       JSTypeFeedbackSpecializer::kDeoptimizationDisabled);
  EXPECT_FALSE(r.Changed());
  EXPECT_TRUE(dependencies()->IsEmpty());
}


TEST_F(JSTypeFeedbackTest, JSLoadNamedGlobalConstSmiWithDeoptimization) {
  const int kValue = 111;
  const char* kName = "banana";
  SetGlobalProperty(kName, kValue);

  Node* ret = ReturnLoadNamedFromGlobal(
      kName, graph()->start(), graph()->start(),
      JSTypeFeedbackSpecializer::kDeoptimizationEnabled);
  graph()->SetEnd(graph()->NewNode(common()->End(1), ret));

  Reduction r = Reduce(ret->InputAt(0),
                       JSTypeFeedbackSpecializer::kDeoptimizationEnabled);

  // Check LoadNamed(global) => HeapConstant[kValue]
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberConstant(kValue));

  EXPECT_THAT(ret, IsReturn(IsNumberConstant(kValue), graph()->start(),
                            graph()->start()));
  EXPECT_THAT(graph()->end(), IsEnd(ret));

  EXPECT_FALSE(dependencies()->IsEmpty());
  dependencies()->Rollback();
}


TEST_F(JSTypeFeedbackTest, JSLoadNamedGlobalConstNumber) {
  const double kValue = -11.25;
  const char* kName = "kiwi";
  SetGlobalProperty(kName, kValue);

  Node* ret = ReturnLoadNamedFromGlobal(
      kName, graph()->start(), graph()->start(),
      JSTypeFeedbackSpecializer::kDeoptimizationDisabled);
  graph()->SetEnd(graph()->NewNode(common()->End(1), ret));

  Reduction r = Reduce(ret->InputAt(0),
                       JSTypeFeedbackSpecializer::kDeoptimizationDisabled);

  EXPECT_FALSE(r.Changed());
  EXPECT_TRUE(dependencies()->IsEmpty());
}


TEST_F(JSTypeFeedbackTest, JSLoadNamedGlobalConstNumberWithDeoptimization) {
  const double kValue = -11.25;
  const char* kName = "kiwi";
  SetGlobalProperty(kName, kValue);

  Node* ret = ReturnLoadNamedFromGlobal(
      kName, graph()->start(), graph()->start(),
      JSTypeFeedbackSpecializer::kDeoptimizationEnabled);
  graph()->SetEnd(graph()->NewNode(common()->End(1), ret));

  Reduction r = Reduce(ret->InputAt(0),
                       JSTypeFeedbackSpecializer::kDeoptimizationEnabled);

  // Check LoadNamed(global) => HeapConstant[kValue]
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberConstant(kValue));

  EXPECT_THAT(ret, IsReturn(IsNumberConstant(kValue), graph()->start(),
                            graph()->start()));
  EXPECT_THAT(graph()->end(), IsEnd(ret));

  EXPECT_FALSE(dependencies()->IsEmpty());
}


TEST_F(JSTypeFeedbackTest, JSLoadNamedGlobalConstString) {
  Handle<HeapObject> kValue = isolate()->factory()->undefined_string();
  const char* kName = "mango";
  SetGlobalProperty(kName, kValue);

  Node* ret = ReturnLoadNamedFromGlobal(
      kName, graph()->start(), graph()->start(),
      JSTypeFeedbackSpecializer::kDeoptimizationDisabled);
  graph()->SetEnd(graph()->NewNode(common()->End(1), ret));

  Reduction r = Reduce(ret->InputAt(0),
                       JSTypeFeedbackSpecializer::kDeoptimizationDisabled);
  ASSERT_FALSE(r.Changed());
  EXPECT_TRUE(dependencies()->IsEmpty());
}


TEST_F(JSTypeFeedbackTest, JSLoadNamedGlobalConstStringWithDeoptimization) {
  Handle<HeapObject> kValue = isolate()->factory()->undefined_string();
  const char* kName = "mango";
  SetGlobalProperty(kName, kValue);

  Node* ret = ReturnLoadNamedFromGlobal(
      kName, graph()->start(), graph()->start(),
      JSTypeFeedbackSpecializer::kDeoptimizationEnabled);
  graph()->SetEnd(graph()->NewNode(common()->End(1), ret));

  Reduction r = Reduce(ret->InputAt(0),
                       JSTypeFeedbackSpecializer::kDeoptimizationEnabled);

  // Check LoadNamed(global) => HeapConstant[kValue]
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsHeapConstant(kValue));

  EXPECT_THAT(ret, IsReturn(IsHeapConstant(kValue), graph()->start(),
                            graph()->start()));
  EXPECT_THAT(graph()->end(), IsEnd(ret));

  EXPECT_FALSE(dependencies()->IsEmpty());
  dependencies()->Rollback();
}


TEST_F(JSTypeFeedbackTest, JSLoadNamedGlobalPropertyCellSmi) {
  const char* kName = "melon";
  SetGlobalProperty(kName, 123);
  SetGlobalProperty(kName, 124);

  Node* ret = ReturnLoadNamedFromGlobal(
      kName, graph()->start(), graph()->start(),
      JSTypeFeedbackSpecializer::kDeoptimizationDisabled);
  graph()->SetEnd(graph()->NewNode(common()->End(1), ret));

  Reduction r = Reduce(ret->InputAt(0),
                       JSTypeFeedbackSpecializer::kDeoptimizationDisabled);
  ASSERT_FALSE(r.Changed());
  EXPECT_TRUE(dependencies()->IsEmpty());
}


TEST_F(JSTypeFeedbackTest, JSLoadNamedGlobalPropertyCellSmiWithDeoptimization) {
  const char* kName = "melon";
  SetGlobalProperty(kName, 123);
  SetGlobalProperty(kName, 124);

  Node* ret = ReturnLoadNamedFromGlobal(
      kName, graph()->start(), graph()->start(),
      JSTypeFeedbackSpecializer::kDeoptimizationEnabled);
  graph()->SetEnd(graph()->NewNode(common()->End(1), ret));

  Reduction r = Reduce(ret->InputAt(0),
                       JSTypeFeedbackSpecializer::kDeoptimizationEnabled);

  // Check LoadNamed(global) => LoadField[PropertyCell::value](cell)
  ASSERT_TRUE(r.Changed());
  FieldAccess access = AccessBuilder::ForPropertyCellValue();
  Capture<Node*> cell_capture;
  Matcher<Node*> load_field_match = IsLoadField(
      access, CaptureEq(&cell_capture), graph()->start(), graph()->start());
  EXPECT_THAT(r.replacement(), load_field_match);

  HeapObjectMatcher cell(cell_capture.value());
  EXPECT_TRUE(cell.HasValue());
  EXPECT_TRUE(cell.Value()->IsPropertyCell());

  EXPECT_THAT(ret,
              IsReturn(load_field_match, load_field_match, graph()->start()));
  EXPECT_THAT(graph()->end(), IsEnd(ret));

  EXPECT_FALSE(dependencies()->IsEmpty());
  dependencies()->Rollback();
}


TEST_F(JSTypeFeedbackTest, JSLoadNamedGlobalPropertyCellString) {
  const char* kName = "pineapple";
  SetGlobalProperty(kName, isolate()->factory()->undefined_string());
  SetGlobalProperty(kName, isolate()->factory()->undefined_value());

  Node* ret = ReturnLoadNamedFromGlobal(
      kName, graph()->start(), graph()->start(),
      JSTypeFeedbackSpecializer::kDeoptimizationDisabled);
  graph()->SetEnd(graph()->NewNode(common()->End(1), ret));

  Reduction r = Reduce(ret->InputAt(0),
                       JSTypeFeedbackSpecializer::kDeoptimizationDisabled);
  ASSERT_FALSE(r.Changed());
  EXPECT_TRUE(dependencies()->IsEmpty());
}


TEST_F(JSTypeFeedbackTest,
       JSLoadNamedGlobalPropertyCellStringWithDeoptimization) {
  const char* kName = "pineapple";
  SetGlobalProperty(kName, isolate()->factory()->undefined_string());
  SetGlobalProperty(kName, isolate()->factory()->undefined_value());

  Node* ret = ReturnLoadNamedFromGlobal(
      kName, graph()->start(), graph()->start(),
      JSTypeFeedbackSpecializer::kDeoptimizationEnabled);
  graph()->SetEnd(graph()->NewNode(common()->End(1), ret));

  Reduction r = Reduce(ret->InputAt(0),
                       JSTypeFeedbackSpecializer::kDeoptimizationEnabled);

  // Check LoadNamed(global) => LoadField[PropertyCell::value](cell)
  ASSERT_TRUE(r.Changed());
  FieldAccess access = AccessBuilder::ForPropertyCellValue();
  Capture<Node*> cell_capture;
  Matcher<Node*> load_field_match = IsLoadField(
      access, CaptureEq(&cell_capture), graph()->start(), graph()->start());
  EXPECT_THAT(r.replacement(), load_field_match);

  HeapObjectMatcher cell(cell_capture.value());
  EXPECT_TRUE(cell.HasValue());
  EXPECT_TRUE(cell.Value()->IsPropertyCell());

  EXPECT_THAT(ret,
              IsReturn(load_field_match, load_field_match, graph()->start()));
  EXPECT_THAT(graph()->end(), IsEnd(ret));

  EXPECT_FALSE(dependencies()->IsEmpty());
  dependencies()->Rollback();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

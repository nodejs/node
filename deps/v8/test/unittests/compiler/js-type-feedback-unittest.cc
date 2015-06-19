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
  Reduction Reduce(Node* node) {
    Handle<GlobalObject> global_object(
        isolate()->native_context()->global_object(), isolate());

    MachineOperatorBuilder machine(zone());
    JSGraph jsgraph(isolate(), graph(), common(), javascript(), &machine);
    JSTypeFeedbackTable table(zone());
    JSTypeFeedbackSpecializer reducer(&jsgraph, &table, nullptr, global_object,
                                      &dependencies_);
    return reducer.Reduce(node);
  }

  Node* EmptyFrameState() {
    MachineOperatorBuilder machine(zone());
    JSGraph jsgraph(isolate(), graph(), common(), javascript(), &machine);
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

  Node* ReturnLoadNamedFromGlobal(const char* string, Node* effect,
                                  Node* control) {
    VectorSlotPair feedback(Handle<TypeFeedbackVector>::null(),
                            FeedbackVectorICSlot::Invalid());
    Node* global = Parameter(Type::GlobalObject());
    Node* context = UndefinedConstant();

    Unique<Name> name = Unique<Name>::CreateUninitialized(
        isolate()->factory()->NewStringFromAsciiChecked(string));
    Node* load = graph()->NewNode(javascript()->LoadNamed(name, feedback),
                                  global, context);
    if (FLAG_turbo_deoptimization) {
      load->AppendInput(zone(), EmptyFrameState());
    }
    load->AppendInput(zone(), effect);
    load->AppendInput(zone(), control);
    Node* if_success = graph()->NewNode(common()->IfSuccess(), load);
    return graph()->NewNode(common()->Return(), load, load, if_success);
  }

  CompilationDependencies* dependencies() { return &dependencies_; }

 private:
  JSOperatorBuilder javascript_;
  CompilationDependencies dependencies_;
};

#define WITH_AND_WITHOUT_TURBO_DEOPTIMIZATION        \
  for (int i = FLAG_turbo_deoptimization = 0; i < 2; \
       FLAG_turbo_deoptimization = ++i)


TEST_F(JSTypeFeedbackTest, JSLoadNamedGlobalConst_smi) {
  const int const_value = 111;
  const char* property_name = "banana";
  SetGlobalProperty(property_name, const_value);

  WITH_AND_WITHOUT_TURBO_DEOPTIMIZATION {
    Node* ret = ReturnLoadNamedFromGlobal(property_name, graph()->start(),
                                          graph()->start());
    graph()->SetEnd(graph()->NewNode(common()->End(), ret));

    Reduction r = Reduce(ret->InputAt(0));

    if (FLAG_turbo_deoptimization) {
      // Check LoadNamed(global) => HeapConstant[const_value]
      ASSERT_TRUE(r.Changed());
      EXPECT_THAT(r.replacement(), IsNumberConstant(const_value));

      EXPECT_THAT(ret, IsReturn(IsNumberConstant(const_value), graph()->start(),
                                graph()->start()));
      EXPECT_THAT(graph()->end(), IsEnd(ret));

      EXPECT_FALSE(dependencies()->IsEmpty());
      dependencies()->Rollback();
    } else {
      ASSERT_FALSE(r.Changed());
      EXPECT_TRUE(dependencies()->IsEmpty());
    }
  }
}


TEST_F(JSTypeFeedbackTest, JSLoadNamedGlobalConst_derble) {
  const double const_value = -11.25;
  const char* property_name = "kiwi";
  SetGlobalProperty(property_name, const_value);

  WITH_AND_WITHOUT_TURBO_DEOPTIMIZATION {
    Node* ret = ReturnLoadNamedFromGlobal(property_name, graph()->start(),
                                          graph()->start());
    graph()->SetEnd(graph()->NewNode(common()->End(), ret));

    Reduction r = Reduce(ret->InputAt(0));

    if (FLAG_turbo_deoptimization) {
      // Check LoadNamed(global) => HeapConstant[const_value]
      ASSERT_TRUE(r.Changed());
      EXPECT_THAT(r.replacement(), IsNumberConstant(const_value));

      EXPECT_THAT(ret, IsReturn(IsNumberConstant(const_value), graph()->start(),
                                graph()->start()));
      EXPECT_THAT(graph()->end(), IsEnd(ret));

      EXPECT_FALSE(dependencies()->IsEmpty());
    } else {
      ASSERT_FALSE(r.Changed());
      EXPECT_TRUE(dependencies()->IsEmpty());
    }
  }
}


TEST_F(JSTypeFeedbackTest, JSLoadNamedGlobalConst_string) {
  Unique<HeapObject> const_value = Unique<HeapObject>::CreateImmovable(
      isolate()->factory()->undefined_string());
  const char* property_name = "mango";
  SetGlobalProperty(property_name, const_value.handle());

  WITH_AND_WITHOUT_TURBO_DEOPTIMIZATION {
    Node* ret = ReturnLoadNamedFromGlobal(property_name, graph()->start(),
                                          graph()->start());
    graph()->SetEnd(graph()->NewNode(common()->End(), ret));

    Reduction r = Reduce(ret->InputAt(0));

    if (FLAG_turbo_deoptimization) {
      // Check LoadNamed(global) => HeapConstant[const_value]
      ASSERT_TRUE(r.Changed());
      EXPECT_THAT(r.replacement(), IsHeapConstant(const_value));

      EXPECT_THAT(ret, IsReturn(IsHeapConstant(const_value), graph()->start(),
                                graph()->start()));
      EXPECT_THAT(graph()->end(), IsEnd(ret));

      EXPECT_FALSE(dependencies()->IsEmpty());
      dependencies()->Rollback();
    } else {
      ASSERT_FALSE(r.Changed());
      EXPECT_TRUE(dependencies()->IsEmpty());
    }
  }
}


TEST_F(JSTypeFeedbackTest, JSLoadNamedGlobalPropertyCell_smi) {
  const char* property_name = "melon";
  SetGlobalProperty(property_name, 123);
  SetGlobalProperty(property_name, 124);

  WITH_AND_WITHOUT_TURBO_DEOPTIMIZATION {
    Node* ret = ReturnLoadNamedFromGlobal(property_name, graph()->start(),
                                          graph()->start());
    graph()->SetEnd(graph()->NewNode(common()->End(), ret));

    Reduction r = Reduce(ret->InputAt(0));

    if (FLAG_turbo_deoptimization) {
      // Check LoadNamed(global) => LoadField[PropertyCell::value](cell)
      ASSERT_TRUE(r.Changed());
      FieldAccess access = AccessBuilder::ForPropertyCellValue();
      Capture<Node*> cell_capture;
      Matcher<Node*> load_field_match = IsLoadField(
          access, CaptureEq(&cell_capture), graph()->start(), graph()->start());
      EXPECT_THAT(r.replacement(), load_field_match);

      HeapObjectMatcher<PropertyCell> cell(cell_capture.value());
      EXPECT_TRUE(cell.HasValue());
      EXPECT_TRUE(cell.Value().handle()->IsPropertyCell());

      EXPECT_THAT(
          ret, IsReturn(load_field_match, load_field_match, graph()->start()));
      EXPECT_THAT(graph()->end(), IsEnd(ret));

      EXPECT_FALSE(dependencies()->IsEmpty());
      dependencies()->Rollback();
    } else {
      ASSERT_FALSE(r.Changed());
      EXPECT_TRUE(dependencies()->IsEmpty());
    }
  }
}


TEST_F(JSTypeFeedbackTest, JSLoadNamedGlobalPropertyCell_string) {
  const char* property_name = "pineapple";
  SetGlobalProperty(property_name, isolate()->factory()->undefined_string());
  SetGlobalProperty(property_name, isolate()->factory()->undefined_value());

  WITH_AND_WITHOUT_TURBO_DEOPTIMIZATION {
    Node* ret = ReturnLoadNamedFromGlobal(property_name, graph()->start(),
                                          graph()->start());
    graph()->SetEnd(graph()->NewNode(common()->End(), ret));

    Reduction r = Reduce(ret->InputAt(0));

    if (FLAG_turbo_deoptimization) {
      // Check LoadNamed(global) => LoadField[PropertyCell::value](cell)
      ASSERT_TRUE(r.Changed());
      FieldAccess access = AccessBuilder::ForPropertyCellValue();
      Capture<Node*> cell_capture;
      Matcher<Node*> load_field_match = IsLoadField(
          access, CaptureEq(&cell_capture), graph()->start(), graph()->start());
      EXPECT_THAT(r.replacement(), load_field_match);

      HeapObjectMatcher<PropertyCell> cell(cell_capture.value());
      EXPECT_TRUE(cell.HasValue());
      EXPECT_TRUE(cell.Value().handle()->IsPropertyCell());

      EXPECT_THAT(
          ret, IsReturn(load_field_match, load_field_match, graph()->start()));
      EXPECT_THAT(graph()->end(), IsEnd(ret));

      EXPECT_FALSE(dependencies()->IsEmpty());
      dependencies()->Rollback();
    } else {
      ASSERT_FALSE(r.Changed());
      EXPECT_TRUE(dependencies()->IsEmpty());
    }
  }
}
}
}
}

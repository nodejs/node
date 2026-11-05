// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-typed-lowering.h"

#include "src/codegen/tick-counter.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/turbofan-typer.h"
#include "src/execution/isolate-inl.h"
#include "src/heap/factory-inl.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/objects.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/js-heap-broker-base.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock-support.h"

using testing::_;
using testing::BitEq;
using testing::IsNaN;

namespace v8 {
namespace internal {
namespace compiler {
namespace js_typed_lowering_unittest {

namespace {

const size_t kIndices[] = {0, 1, 42, 100, 1024};

Type const kJSTypes[] = {Type::Undefined(), Type::Null(),   Type::Boolean(),
                         Type::Number(),    Type::String(), Type::Object()};

}  // namespace


class JSTypedLoweringTest : public TypedGraphTest {
 public:
  JSTypedLoweringTest()
      : TypedGraphTest(3), javascript_(zone()), deps_(broker(), zone()) {}
  ~JSTypedLoweringTest() override = default;

 protected:
  Reduction Reduce(Node* node) {
    MachineOperatorBuilder machine(zone());
    SimplifiedOperatorBuilder simplified(zone());
    JSGraph jsgraph(isolate(), graph(), common(), javascript(), &simplified,
                    &machine);
    GraphReducer graph_reducer(zone(), graph(), tick_counter(), broker());
    JSTypedLowering reducer(&graph_reducer, &jsgraph, broker(), zone());
    return reducer.Reduce(node);
  }

  JSOperatorBuilder* javascript() { return &javascript_; }

 private:
  JSOperatorBuilder javascript_;
  CompilationDependencies deps_;
};



// -----------------------------------------------------------------------------
// JSToName

TEST_F(JSTypedLoweringTest, JSToNameWithString) {
  Node* const input = Parameter(Type::String(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ToName(), input, context,
                                        EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(input, r.replacement());
}

TEST_F(JSTypedLoweringTest, JSToNameWithSymbol) {
  Node* const input = Parameter(Type::Symbol(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ToName(), input, context,
                                        EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(input, r.replacement());
}

TEST_F(JSTypedLoweringTest, JSToNameWithAny) {
  Node* const input = Parameter(Type::Any(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ToName(), input, context,
                                        EmptyFrameState(), effect, control));
  ASSERT_FALSE(r.Changed());
}

// -----------------------------------------------------------------------------
// JSToNumber

TEST_F(JSTypedLoweringTest, JSToNumberWithPlainPrimitive) {
  Node* const input = Parameter(Type::PlainPrimitive(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r =
      Reduce(graph()->NewNode(javascript()->ToNumber(), input, context,
                              EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsPlainPrimitiveToNumber(input));
}


// -----------------------------------------------------------------------------
// JSToObject


TEST_F(JSTypedLoweringTest, JSToObjectWithAny) {
  Node* const input = Parameter(Type::Any(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const frame_state = EmptyFrameState();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ToObject(), input,
                                        context, frame_state, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsPhi(MachineRepresentation::kTagged, _, _, _));
}


TEST_F(JSTypedLoweringTest, JSToObjectWithReceiver) {
  Node* const input = Parameter(Type::Receiver(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const frame_state = EmptyFrameState();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ToObject(), input,
                                        context, frame_state, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(input, r.replacement());
}


// -----------------------------------------------------------------------------
// JSToString


TEST_F(JSTypedLoweringTest, JSToStringWithBoolean) {
  Node* const input = Parameter(Type::Boolean(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const frame_state = EmptyFrameState();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ToString(), input,
                                        context, frame_state, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsSelect(MachineRepresentation::kTagged, input,
                       IsHeapConstant(factory()->true_string()),
                       IsHeapConstant(factory()->false_string())));
}


// -----------------------------------------------------------------------------
// JSStrictEqual

namespace {

FeedbackSource FeedbackSourceWithOneBinarySlot(JSTypedLoweringTest* R) {
  return FeedbackSource{
      FeedbackVector::NewWithOneBinarySlotForTesting(R->zone(), R->isolate()),
      FeedbackSlot{0}};
}

}  // namespace

TEST_F(JSTypedLoweringTest, JSStrictEqualWithTheHole) {
  Node* const the_hole = HeapConstantHole(factory()->the_hole_value());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  TRACED_FOREACH(Type, type, kJSTypes) {
    Node* const lhs = Parameter(type);
    Reduction r = Reduce(
        graph()->NewNode(javascript()->StrictEqual(CompareOperationHint::kNone),
                         lhs, the_hole, context, effect, control));
    ASSERT_FALSE(r.Changed());
  }
}


TEST_F(JSTypedLoweringTest, JSStrictEqualWithUnique) {
  Node* const lhs = Parameter(Type::Unique(), 0);
  Node* const rhs = Parameter(Type::Unique(), 1);
  Node* const context = Parameter(Type::Any(), 2);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(
      graph()->NewNode(javascript()->StrictEqual(CompareOperationHint::kNone),
                       lhs, rhs, context, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsReferenceEqual(lhs, rhs));
}


// -----------------------------------------------------------------------------
// JSShiftLeft

TEST_F(JSTypedLoweringTest, JSShiftLeftWithSigned32AndConstant) {
  Node* const lhs = Parameter(Type::Signed32());
  Node* const feedback = UndefinedConstant();
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  TRACED_FORRANGE(double, rhs, 0, 31) {
    Reduction r = Reduce(graph()->NewNode(
        javascript()->ShiftLeft(FeedbackSourceWithOneBinarySlot(this)), lhs,
        NumberConstant(rhs), feedback, context, EmptyFrameState(), effect,
        control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsNumberShiftLeft(lhs, IsNumberConstant(BitEq(rhs))));
  }
}

TEST_F(JSTypedLoweringTest, JSShiftLeftWithSigned32AndUnsigned32) {
  Node* const lhs = Parameter(Type::Signed32());
  Node* const rhs = Parameter(Type::Unsigned32());
  Node* const feedback = UndefinedConstant();
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(
      javascript()->ShiftLeft(FeedbackSourceWithOneBinarySlot(this)), lhs, rhs,
      feedback, context, EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberShiftLeft(lhs, rhs));
}


// -----------------------------------------------------------------------------
// JSShiftRight


TEST_F(JSTypedLoweringTest, JSShiftRightWithSigned32AndConstant) {
  Node* const lhs = Parameter(Type::Signed32());
  Node* const feedback = UndefinedConstant();
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  TRACED_FORRANGE(double, rhs, 0, 31) {
    Reduction r = Reduce(graph()->NewNode(
        javascript()->ShiftRight(FeedbackSourceWithOneBinarySlot(this)), lhs,
        NumberConstant(rhs), feedback, context, EmptyFrameState(), effect,
        control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsNumberShiftRight(lhs, IsNumberConstant(BitEq(rhs))));
  }
}


TEST_F(JSTypedLoweringTest, JSShiftRightWithSigned32AndUnsigned32) {
  Node* const lhs = Parameter(Type::Signed32());
  Node* const rhs = Parameter(Type::Unsigned32());
  Node* const feedback = UndefinedConstant();
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(
      javascript()->ShiftRight(FeedbackSourceWithOneBinarySlot(this)), lhs, rhs,
      feedback, context, EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberShiftRight(lhs, rhs));
}


// -----------------------------------------------------------------------------
// JSShiftRightLogical


TEST_F(JSTypedLoweringTest,
                   JSShiftRightLogicalWithUnsigned32AndConstant) {
  Node* const lhs = Parameter(Type::Unsigned32());
  Node* const feedback = UndefinedConstant();
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  TRACED_FORRANGE(double, rhs, 0, 31) {
    Reduction r = Reduce(graph()->NewNode(
        javascript()->ShiftRightLogical(FeedbackSourceWithOneBinarySlot(this)),
        lhs, NumberConstant(rhs), feedback, context, EmptyFrameState(), effect,
        control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsNumberShiftRightLogical(lhs, IsNumberConstant(BitEq(rhs))));
  }
}


TEST_F(JSTypedLoweringTest, JSShiftRightLogicalWithUnsigned32AndUnsigned32) {
  Node* const lhs = Parameter(Type::Unsigned32());
  Node* const rhs = Parameter(Type::Unsigned32());
  Node* const feedback = UndefinedConstant();
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(
      javascript()->ShiftRightLogical(FeedbackSourceWithOneBinarySlot(this)),
      lhs, rhs, feedback, context, EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberShiftRightLogical(lhs, rhs));
}

// -----------------------------------------------------------------------------
// JSLoadContextNoCell

TEST_F(JSTypedLoweringTest, JSLoadContextNoCell) {
  Node* const context = Parameter(Type::Any());
  Node* const effect = graph()->start();
  static bool kBooleans[] = {false, true};
  TRACED_FOREACH(size_t, index, kIndices) {
    TRACED_FOREACH(bool, immutable, kBooleans) {
      Reduction const r1 = Reduce(
          graph()->NewNode(javascript()->LoadContextNoCell(0, index, immutable),
                           context, effect));
      ASSERT_TRUE(r1.Changed());
      EXPECT_THAT(r1.replacement(),
                  IsLoadField(AccessBuilder::ForContextSlot(index), context,
                              effect, graph()->start()));

      Reduction const r2 = Reduce(
          graph()->NewNode(javascript()->LoadContextNoCell(1, index, immutable),
                           context, effect));
      ASSERT_TRUE(r2.Changed());
      EXPECT_THAT(
          r2.replacement(),
          IsLoadField(AccessBuilder::ForContextSlot(index),
                      IsLoadField(AccessBuilder::ForContextSlotKnownPointer(
                                      Context::PREVIOUS_INDEX),
                                  context, effect, graph()->start()),
                      _, graph()->start()));
    }
  }
}

// -----------------------------------------------------------------------------
// JSStoreContextNoCell

TEST_F(JSTypedLoweringTest, JSStoreContextNoCell) {
  Node* const context = Parameter(Type::Any());
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  TRACED_FOREACH(size_t, index, kIndices) {
    TRACED_FOREACH(Type, type, kJSTypes) {
      Node* const value = Parameter(type);

      Reduction const r1 =
          Reduce(graph()->NewNode(javascript()->StoreContextNoCell(0, index),
                                  value, context, effect, control));
      ASSERT_TRUE(r1.Changed());
      EXPECT_THAT(r1.replacement(),
                  IsStoreField(AccessBuilder::ForContextSlot(index), context,
                               value, effect, control));

      Reduction const r2 =
          Reduce(graph()->NewNode(javascript()->StoreContextNoCell(1, index),
                                  value, context, effect, control));
      ASSERT_TRUE(r2.Changed());
      EXPECT_THAT(
          r2.replacement(),
          IsStoreField(AccessBuilder::ForContextSlot(index),
                       IsLoadField(AccessBuilder::ForContextSlotKnownPointer(
                                       Context::PREVIOUS_INDEX),
                                   context, effect, graph()->start()),
                       value, _, control));
    }
  }
}

// -----------------------------------------------------------------------------
// JSLoadNamed


TEST_F(JSTypedLoweringTest, JSLoadNamedStringLength) {
  NameRef name = broker()->length_string();
  Node* const receiver = Parameter(Type::String(), 0);
  Node* const feedback = UndefinedConstant();
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction const r = Reduce(graph()->NewNode(
      javascript()->LoadNamed(name, FeedbackSource{}), receiver, feedback,
      context, EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsStringLength(receiver));
}


// -----------------------------------------------------------------------------
// JSAdd


TEST_F(JSTypedLoweringTest, JSAddWithString) {
  Node* lhs = Parameter(Type::String(), 0);
  Node* rhs = Parameter(Type::String(), 1);
  Node* const feedback = UndefinedConstant();
  Node* context = Parameter(Type::Any(), 2);
  Node* frame_state = EmptyFrameState();
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(
      javascript()->Add(FeedbackSourceWithOneBinarySlot(this)), lhs, rhs,
      feedback, context, frame_state, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsStringConcat(_, lhs, rhs));
}

class JSTypedLoweringTester : public JSHeapBrokerTestBase {
 public:
  JSTypedLoweringTester(Isolate* isolate, Zone* zone, int num_parameters = 0)
      : JSHeapBrokerTestBase(isolate, zone),
        isolate(isolate),
        zone_(zone),
        binop(nullptr),
        unop(nullptr),
        javascript(zone),
        machine(zone),
        simplified(zone),
        common(zone),
        graph(zone),
        typer(broker(), Typer::kNoFlags, &graph, &tick_counter),
        context_node(nullptr),
        deps(broker(), zone) {
    graph.SetStart(graph.NewNode(common.Start(num_parameters)));
    graph.SetEnd(graph.NewNode(common.End(1), graph.start()));
    typer.Run();
  }

  Isolate* main_isolate() { return isolate; }
  Zone* main_zone() { return zone_; }

  Isolate* isolate;
  Zone* zone_;
  TickCounter tick_counter;
  const Operator* binop;
  const Operator* unop;
  JSOperatorBuilder javascript;
  MachineOperatorBuilder machine;
  SimplifiedOperatorBuilder simplified;
  CommonOperatorBuilder common;
  TFGraph graph;
  Typer typer;
  Node* context_node;
  CompilationDependencies deps;

  Node* Parameter(Type t, int32_t index = 0) {
    Node* n = graph.NewNode(common.Parameter(index), graph.start());
    NodeProperties::SetType(n, t);
    return n;
  }

  Node* UndefinedConstant() {
    Handle<HeapObject> value = isolate->factory()->undefined_value();
    return graph.NewNode(common.HeapConstant(value));
  }

  Node* HeapConstantNoHole(Handle<HeapObject> constant) {
    return graph.NewNode(common.HeapConstant(constant));
  }

  Node* EmptyFrameState(Node* context) {
    Node* parameters =
        graph.NewNode(common.StateValues(0, SparseInputMask::Dense()));
    Node* locals =
        graph.NewNode(common.StateValues(0, SparseInputMask::Dense()));
    Node* stack =
        graph.NewNode(common.StateValues(0, SparseInputMask::Dense()));

    FrameStateFunctionInfo const* function_info =
        common.CreateFrameStateFunctionInfo(
            FrameStateType::kUnoptimizedFunction, 0, 0, 0, {}, {});
    Node* state_node = graph.NewNode(
        common.FrameState(BytecodeOffset::None(),
                          OutputFrameStateCombine::Ignore(), function_info),
        parameters, locals, stack, context, UndefinedConstant(), graph.start());

    return state_node;
  }

  Node* reduce(Node* node) {
    JSGraph jsgraph(main_isolate(), &graph, &common, &javascript, &simplified,
                    &machine);
    GraphReducer graph_reducer(main_zone(), &graph, &tick_counter, broker());
    JSTypedLowering reducer(&graph_reducer, &jsgraph, broker(), main_zone());
    Reduction reduction = reducer.Reduce(node);
    if (reduction.Changed()) return reduction.replacement();
    return node;
  }

  Node* start() { return graph.start(); }

  Node* context() {
    if (context_node == nullptr) {
      context_node = graph.NewNode(common.Parameter(-1), graph.start());
    }
    return context_node;
  }

  Node* control() { return start(); }

  void CheckBinop(IrOpcode::Value expected, Node* node) {
    CHECK_EQ(expected, node->opcode());
  }

  void CheckBinop(const Operator* expected, Node* node) {
    CHECK_EQ(expected->opcode(), node->op()->opcode());
  }

  Node* ReduceUnop(const Operator* op, Type input_type) {
    return reduce(Unop(op, Parameter(input_type)));
  }

  Node* ReduceBinop(const Operator* op, Type left_type, Type right_type) {
    return reduce(Binop(op, Parameter(left_type, 0), Parameter(right_type, 1)));
  }

  Node* Binop(const Operator* op, Node* left, Node* right) {
    // JS binops also require context, effect, and control
    std::vector<Node*> inputs;
    inputs.push_back(left);
    inputs.push_back(right);
    if (JSOperator::IsBinaryWithFeedback(op->opcode())) {
      inputs.push_back(UndefinedConstant());  // Feedback vector.
    }
    if (OperatorProperties::HasContextInput(op)) {
      inputs.push_back(context());
    }
    for (int i = 0; i < OperatorProperties::GetFrameStateInputCount(op); i++) {
      inputs.push_back(EmptyFrameState(context()));
    }
    if (op->EffectInputCount() > 0) {
      inputs.push_back(start());
    }
    if (op->ControlInputCount() > 0) {
      inputs.push_back(control());
    }
    return graph.NewNode(op, static_cast<int>(inputs.size()),
                         &(inputs.front()));
  }

  Node* Unop(const Operator* op, Node* input) {
    // JS unops also require context, effect, and control
    if (OperatorProperties::GetFrameStateInputCount(op) > 0) {
      CHECK_EQ(1, OperatorProperties::GetFrameStateInputCount(op));
      return graph.NewNode(op, input, context(), EmptyFrameState(context()),
                           start(), control());
    } else {
      return graph.NewNode(op, input, context(), start(), control());
    }
  }

  Node* UseForEffect(Node* node) {
    Node* merge = graph.NewNode(common.Merge(1), start());
    return graph.NewNode(common.EffectPhi(1), node, merge);
  }

  void CheckEffectInput(Node* effect, Node* use) {
    CHECK_EQ(effect, NodeProperties::GetEffectInput(use));
  }

  void CheckNumberConstant(double expected, Node* result) {
    CHECK_EQ(IrOpcode::kNumberConstant, result->opcode());
    CHECK_EQ(expected, OpParameter<double>(result->op()));
  }

  void CheckNaN(Node* result) {
    CHECK_EQ(IrOpcode::kNumberConstant, result->opcode());
    double value = OpParameter<double>(result->op());
    CHECK(std::isnan(value));
  }

  void CheckTrue(Node* result) {
    CheckHandle(isolate->factory()->true_value(), result);
  }

  void CheckFalse(Node* result) {
    CheckHandle(isolate->factory()->false_value(), result);
  }

  void CheckHandle(DirectHandle<HeapObject> expected, Node* result) {
    CHECK_EQ(IrOpcode::kHeapConstant, result->opcode());
    DirectHandle<HeapObject> value = HeapConstantOf(result->op());
    CHECK_EQ(*expected, *value);
  }
};

static Type kStringTypes[] = {Type::InternalizedString(), Type::String()};

static Type kInt32Types[] = {Type::UnsignedSmall(), Type::Negative32(),
                             Type::Unsigned31(),    Type::SignedSmall(),
                             Type::Signed32(),      Type::Unsigned32(),
                             Type::Integral32()};

static Type kNumberTypes[] = {
    Type::UnsignedSmall(), Type::Negative32(),  Type::Unsigned31(),
    Type::SignedSmall(),   Type::Signed32(),    Type::Unsigned32(),
    Type::Integral32(),    Type::MinusZero(),   Type::NaN(),
    Type::OrderedNumber(), Type::PlainNumber(), Type::Number()};

static Type I32Type(bool is_signed) {
  return is_signed ? Type::Signed32() : Type::Unsigned32();
}

static IrOpcode::Value NumberToI32(bool is_signed) {
  return is_signed ? IrOpcode::kNumberToInt32 : IrOpcode::kNumberToUint32;
}

namespace {

FeedbackSource FeedbackSourceWithOneBinarySlot(JSTypedLoweringTester* R) {
  return FeedbackSource{FeedbackVector::NewWithOneBinarySlotForTesting(
                            R->main_zone(), R->main_isolate()),
                        FeedbackSlot{0}};
}
}  // namespace

using JSTypedLoweringTesterTest = TestWithNativeContextAndZone;

TEST_F(JSTypedLoweringTesterTest, StringBinops) {
  JSTypedLoweringTester R(i_isolate(), zone());

  for (size_t i = 0; i < arraysize(kStringTypes); ++i) {
    Node* p0 = R.Parameter(kStringTypes[i], 0);

    for (size_t j = 0; j < arraysize(kStringTypes); ++j) {
      Node* p1 = R.Parameter(kStringTypes[j], 1);

      Node* add = R.Binop(R.javascript.Add(FeedbackSourceWithOneBinarySlot(&R)),
                          p0, p1);
      Node* r = R.reduce(add);

      R.CheckBinop(IrOpcode::kStringConcat, r);
      CHECK_EQ(p0, r->InputAt(1));
      CHECK_EQ(p1, r->InputAt(2));
    }
  }
}

TEST_F(JSTypedLoweringTesterTest, AddNumber1) {
  JSTypedLoweringTester R(i_isolate(), zone());
  for (size_t i = 0; i < arraysize(kNumberTypes); ++i) {
    Node* p0 = R.Parameter(kNumberTypes[i], 0);
    Node* p1 = R.Parameter(kNumberTypes[i], 1);
    Node* add =
        R.Binop(R.javascript.Add(FeedbackSourceWithOneBinarySlot(&R)), p0, p1);
    Node* r = R.reduce(add);

    R.CheckBinop(IrOpcode::kNumberAdd, r);
    CHECK_EQ(p0, r->InputAt(0));
    CHECK_EQ(p1, r->InputAt(1));
  }
}

TEST_F(JSTypedLoweringTesterTest, NumberBinops) {
  JSTypedLoweringTester R(i_isolate(), zone());
  FeedbackSource feedback_source = FeedbackSourceWithOneBinarySlot(&R);
  const Operator* ops[] = {
      R.javascript.Add(feedback_source),      R.simplified.NumberAdd(),
      R.javascript.Subtract(feedback_source), R.simplified.NumberSubtract(),
      R.javascript.Multiply(feedback_source), R.simplified.NumberMultiply(),
      R.javascript.Divide(feedback_source),   R.simplified.NumberDivide(),
      R.javascript.Modulus(feedback_source),  R.simplified.NumberModulus(),
  };

  for (size_t i = 0; i < arraysize(kNumberTypes); ++i) {
    Node* p0 = R.Parameter(kNumberTypes[i], 0);

    for (size_t j = 0; j < arraysize(kNumberTypes); ++j) {
      Node* p1 = R.Parameter(kNumberTypes[j], 1);

      for (size_t k = 0; k < arraysize(ops); k += 2) {
        Node* add = R.Binop(ops[k], p0, p1);
        Node* r = R.reduce(add);

        R.CheckBinop(ops[k + 1], r);
        CHECK_EQ(p0, r->InputAt(0));
        CHECK_EQ(p1, r->InputAt(1));
      }
    }
  }
}

static void CheckToI32(Node* old_input, Node* new_input, bool is_signed) {
  Type old_type = NodeProperties::GetType(old_input);
  Type new_type = NodeProperties::GetType(new_input);
  Type expected_type = I32Type(is_signed);
  CHECK(new_type.Is(expected_type));
  if (old_type.Is(expected_type)) {
    CHECK_EQ(old_input, new_input);
  } else if (new_input->opcode() == IrOpcode::kNumberConstant) {
    double v = OpParameter<double>(new_input->op());
    double e = static_cast<double>(is_signed ? FastD2I(v) : FastD2UI(v));
    CHECK_EQ(e, v);
  }
}

// A helper class for testing lowering of bitwise shift operators.
class JSBitwiseShiftTypedLoweringTester : public JSTypedLoweringTester {
 public:
  JSBitwiseShiftTypedLoweringTester(Isolate* isolate, Zone* zone)
      : JSTypedLoweringTester(isolate, zone) {
    int i = 0;
    FeedbackSource feedback_source = FeedbackSourceWithOneBinarySlot(this);
    set(i++, javascript.ShiftLeft(feedback_source), true);
    set(i++, simplified.NumberShiftLeft(), false);
    set(i++, javascript.ShiftRight(feedback_source), true);
    set(i++, simplified.NumberShiftRight(), false);
    set(i++, javascript.ShiftRightLogical(feedback_source), false);
    set(i++, simplified.NumberShiftRightLogical(), false);
  }
  static const int kNumberOps = 6;
  const Operator* ops[kNumberOps];
  bool signedness[kNumberOps];

 private:
  void set(int idx, const Operator* op, bool s) {
    ops[idx] = op;
    signedness[idx] = s;
  }
};

TEST_F(JSTypedLoweringTesterTest, Int32BitwiseShifts) {
  JSBitwiseShiftTypedLoweringTester R(i_isolate(), zone());

  Type types[] = {
      Type::SignedSmall(), Type::UnsignedSmall(), Type::Negative32(),
      Type::Unsigned31(),  Type::Unsigned32(),    Type::Signed32(),
      Type::MinusZero(),   Type::NaN(),           Type::Undefined(),
      Type::Null(),        Type::Boolean(),       Type::Number(),
      Type::PlainNumber(), Type::String()};

  for (size_t i = 0; i < arraysize(types); ++i) {
    Node* p0 = R.Parameter(types[i], 0);

    for (size_t j = 0; j < arraysize(types); ++j) {
      Node* p1 = R.Parameter(types[j], 1);

      for (int k = 0; k < R.kNumberOps; k += 2) {
        Node* add = R.Binop(R.ops[k], p0, p1);
        Node* r = R.reduce(add);

        R.CheckBinop(R.ops[k + 1], r);
        Node* r0 = r->InputAt(0);
        Node* r1 = r->InputAt(1);

        CheckToI32(p0, r0, R.signedness[k]);
        CheckToI32(p1, r1, false);
      }
    }
  }
}

// A helper class for testing lowering of bitwise operators.
class JSBitwiseTypedLoweringTester : public JSTypedLoweringTester {
 public:
  JSBitwiseTypedLoweringTester(Isolate* isolate, Zone* zone)
      : JSTypedLoweringTester(isolate, zone) {
    int i = 0;
    FeedbackSource feedback_source = FeedbackSourceWithOneBinarySlot(this);
    set(i++, javascript.BitwiseOr(feedback_source), true);
    set(i++, simplified.NumberBitwiseOr(), true);
    set(i++, javascript.BitwiseXor(feedback_source), true);
    set(i++, simplified.NumberBitwiseXor(), true);
    set(i++, javascript.BitwiseAnd(feedback_source), true);
    set(i++, simplified.NumberBitwiseAnd(), true);
  }
  static const int kNumberOps = 6;
  const Operator* ops[kNumberOps];
  bool signedness[kNumberOps];

 private:
  void set(int idx, const Operator* op, bool s) {
    ops[idx] = op;
    signedness[idx] = s;
  }
};

TEST_F(JSTypedLoweringTesterTest, Int32BitwiseBinops) {
  JSBitwiseTypedLoweringTester R(i_isolate(), zone());

  Type types[] = {
      Type::SignedSmall(),   Type::UnsignedSmall(), Type::Unsigned32(),
      Type::Signed32(),      Type::MinusZero(),     Type::NaN(),
      Type::OrderedNumber(), Type::PlainNumber(),   Type::Undefined(),
      Type::Null(),          Type::Boolean(),       Type::Number(),
      Type::String()};

  for (size_t i = 0; i < arraysize(types); ++i) {
    Node* p0 = R.Parameter(types[i], 0);

    for (size_t j = 0; j < arraysize(types); ++j) {
      Node* p1 = R.Parameter(types[j], 1);

      for (int k = 0; k < R.kNumberOps; k += 2) {
        Node* add = R.Binop(R.ops[k], p0, p1);
        Node* r = R.reduce(add);

        R.CheckBinop(R.ops[k + 1], r);

        CheckToI32(p0, r->InputAt(0), R.signedness[k]);
        CheckToI32(p1, r->InputAt(1), R.signedness[k + 1]);
      }
    }
  }
}

TEST_F(JSTypedLoweringTesterTest, JSToNumber1) {
  JSTypedLoweringTester R(i_isolate(), zone());
  const Operator* ton = R.javascript.ToNumber();

  for (size_t i = 0; i < arraysize(kNumberTypes); i++) {  // ToNumber(number)
    Node* r = R.ReduceUnop(ton, kNumberTypes[i]);
    CHECK_EQ(IrOpcode::kParameter, r->opcode());
  }

  {  // ToNumber(undefined)
    Node* r = R.ReduceUnop(ton, Type::Undefined());
    R.CheckNaN(r);
  }

  {  // ToNumber(null)
    Node* r = R.ReduceUnop(ton, Type::Null());
    R.CheckNumberConstant(0.0, r);
  }
}

TEST_F(JSTypedLoweringTesterTest, JSToNumber_replacement) {
  JSTypedLoweringTester R(i_isolate(), zone());

  Type types[] = {Type::Null(), Type::Undefined(), Type::Number()};

  for (size_t i = 0; i < arraysize(types); i++) {
    Node* n = R.Parameter(types[i]);
    Node* c =
        R.graph.NewNode(R.javascript.ToNumber(), n, R.context(),
                        R.EmptyFrameState(R.context()), R.start(), R.start());
    Node* effect_use = R.UseForEffect(c);
    Node* add = R.graph.NewNode(R.simplified.ReferenceEqual(), n, c);

    R.CheckEffectInput(c, effect_use);
    Node* r = R.reduce(c);

    if (types[i].Is(Type::Number())) {
      CHECK_EQ(n, r);
    } else {
      CHECK_EQ(IrOpcode::kNumberConstant, r->opcode());
    }

    CHECK_EQ(n, add->InputAt(0));
    CHECK_EQ(r, add->InputAt(1));
    R.CheckEffectInput(R.start(), effect_use);
  }
}

TEST_F(JSTypedLoweringTesterTest, JSToNumberOfConstant) {
  JSTypedLoweringTester R(i_isolate(), zone());

  const Operator* ops[] = {R.common.NumberConstant(0),
                           R.common.NumberConstant(-1),
                           R.common.NumberConstant(0.1)};

  for (size_t i = 0; i < arraysize(ops); i++) {
    Node* n = R.graph.NewNode(ops[i]);
    Node* convert = R.Unop(R.javascript.ToNumber(), n);
    Node* r = R.reduce(convert);
    // Note that either outcome below is correct. It only depends on whether
    // the types of constants are eagerly computed or only computed by the
    // typing pass.
    if (NodeProperties::GetType(n).Is(Type::Number())) {
      // If number constants are eagerly typed, then reduction should
      // remove the ToNumber.
      CHECK_EQ(n, r);
    } else {
      // Otherwise, type-based lowering should only look at the type, and
      // *not* try to constant fold.
      CHECK_EQ(convert, r);
    }
  }
}

TEST_F(JSTypedLoweringTesterTest, JSToNumberOfNumberOrOtherPrimitive) {
  JSTypedLoweringTester R(i_isolate(), zone());
  Type others[] = {Type::Undefined(), Type::Null(), Type::Boolean(),
                   Type::String()};

  for (size_t i = 0; i < arraysize(others); i++) {
    Type t = Type::Union(Type::Number(), others[i], R.main_zone());
    Node* r = R.ReduceUnop(R.javascript.ToNumber(), t);
    CHECK_EQ(IrOpcode::kPlainPrimitiveToNumber, r->opcode());
  }
}

TEST_F(JSTypedLoweringTesterTest, JSToString1) {
  JSTypedLoweringTester R(i_isolate(), zone());

  for (size_t i = 0; i < arraysize(kStringTypes); i++) {
    Node* r = R.ReduceUnop(R.javascript.ToString(), kStringTypes[i]);
    CHECK_EQ(IrOpcode::kParameter, r->opcode());
  }

  const Operator* op = R.javascript.ToString();

  {  // ToString(undefined) => "undefined"
    Node* r = R.ReduceUnop(op, Type::Undefined());
    R.CheckHandle(R.isolate->factory()->undefined_string(), r);
  }

  {  // ToString(null) => "null"
    Node* r = R.ReduceUnop(op, Type::Null());
    R.CheckHandle(R.isolate->factory()->null_string(), r);
  }

  {  // ToString(boolean)
    Node* r = R.ReduceUnop(op, Type::Boolean());
    CHECK_EQ(IrOpcode::kSelect, r->opcode());
  }

  {  // ToString(number)
    Node* r = R.ReduceUnop(op, Type::Number());
    CHECK_EQ(IrOpcode::kNumberToString, r->opcode());
  }

  {  // ToString(string)
    Node* r = R.ReduceUnop(op, Type::String());
    CHECK_EQ(IrOpcode::kParameter, r->opcode());  // No-op
  }

  {  // ToString(object)
    Node* r = R.ReduceUnop(op, Type::Object());
    CHECK_EQ(IrOpcode::kJSToString, r->opcode());  // No reduction.
  }
}

TEST_F(JSTypedLoweringTesterTest, JSToString_replacement) {
  JSTypedLoweringTester R(i_isolate(), zone());

  Type types[] = {Type::Null(), Type::Undefined(), Type::String()};

  for (size_t i = 0; i < arraysize(types); i++) {
    Node* n = R.Parameter(types[i]);
    Node* c =
        R.graph.NewNode(R.javascript.ToString(), n, R.context(),
                        R.EmptyFrameState(R.context()), R.start(), R.start());
    Node* effect_use = R.UseForEffect(c);
    Node* add = R.graph.NewNode(R.simplified.ReferenceEqual(), n, c);

    R.CheckEffectInput(c, effect_use);
    Node* r = R.reduce(c);

    if (types[i].Is(Type::String())) {
      CHECK_EQ(n, r);
    } else {
      CHECK_EQ(IrOpcode::kHeapConstant, r->opcode());
    }

    CHECK_EQ(n, add->InputAt(0));
    CHECK_EQ(r, add->InputAt(1));
    R.CheckEffectInput(R.start(), effect_use);
  }
}

TEST_F(JSTypedLoweringTesterTest, StringComparison) {
  JSTypedLoweringTester R(i_isolate(), zone());

  const Operator* ops[] = {
      R.javascript.LessThan(CompareOperationHint::kNone),
      R.simplified.StringLessThan(),
      R.javascript.LessThanOrEqual(CompareOperationHint::kNone),
      R.simplified.StringLessThanOrEqual(),
      R.javascript.GreaterThan(CompareOperationHint::kNone),
      R.simplified.StringLessThan(),
      R.javascript.GreaterThanOrEqual(CompareOperationHint::kNone),
      R.simplified.StringLessThanOrEqual()};

  for (size_t i = 0; i < arraysize(kStringTypes); i++) {
    Node* p0 = R.Parameter(kStringTypes[i], 0);
    for (size_t j = 0; j < arraysize(kStringTypes); j++) {
      Node* p1 = R.Parameter(kStringTypes[j], 1);

      for (size_t k = 0; k < arraysize(ops); k += 2) {
        Node* cmp = R.Binop(ops[k], p0, p1);
        Node* r = R.reduce(cmp);

        R.CheckBinop(ops[k + 1], r);
        if (k >= 4) {
          // GreaterThan and GreaterThanOrEqual commute the inputs
          // and use the LessThan and LessThanOrEqual operators.
          CHECK_EQ(p1, r->InputAt(0));
          CHECK_EQ(p0, r->InputAt(1));
        } else {
          CHECK_EQ(p0, r->InputAt(0));
          CHECK_EQ(p1, r->InputAt(1));
        }
      }
    }
  }
}

static void CheckIsConvertedToNumber(Node* val, Node* converted) {
  if (NodeProperties::GetType(val).Is(Type::Number())) {
    CHECK_EQ(val, converted);
  } else {
    if (converted->opcode() == IrOpcode::kNumberConstant) return;
    CHECK(IrOpcode::kJSToNumber == converted->opcode() ||
          IrOpcode::kJSToNumberConvertBigInt == converted->opcode());
    CHECK_EQ(val, converted->InputAt(0));
  }
}

TEST_F(JSTypedLoweringTesterTest, NumberComparison) {
  JSTypedLoweringTester R(i_isolate(), zone());

  const Operator* ops[] = {
      R.javascript.LessThan(CompareOperationHint::kNone),
      R.simplified.NumberLessThan(),
      R.javascript.LessThanOrEqual(CompareOperationHint::kNone),
      R.simplified.NumberLessThanOrEqual(),
      R.javascript.GreaterThan(CompareOperationHint::kNone),
      R.simplified.NumberLessThan(),
      R.javascript.GreaterThanOrEqual(CompareOperationHint::kNone),
      R.simplified.NumberLessThanOrEqual()};

  Node* const p0 = R.Parameter(Type::Number(), 0);
  Node* const p1 = R.Parameter(Type::Number(), 1);

  for (size_t k = 0; k < arraysize(ops); k += 2) {
    Node* cmp = R.Binop(ops[k], p0, p1);
    Node* r = R.reduce(cmp);

    R.CheckBinop(ops[k + 1], r);
    if (k >= 4) {
      // GreaterThan and GreaterThanOrEqual commute the inputs
      // and use the LessThan and LessThanOrEqual operators.
      CheckIsConvertedToNumber(p1, r->InputAt(0));
      CheckIsConvertedToNumber(p0, r->InputAt(1));
    } else {
      CheckIsConvertedToNumber(p0, r->InputAt(0));
      CheckIsConvertedToNumber(p1, r->InputAt(1));
    }
  }
}

TEST_F(JSTypedLoweringTesterTest, MixedComparison1) {
  JSTypedLoweringTester R(i_isolate(), zone());

  Type types[] = {Type::Number(), Type::String(),
                  Type::Union(Type::Number(), Type::String(), R.main_zone())};

  for (size_t i = 0; i < arraysize(types); i++) {
    Node* p0 = R.Parameter(types[i], 0);

    for (size_t j = 0; j < arraysize(types); j++) {
      Node* p1 = R.Parameter(types[j], 1);
      {
        const Operator* less_than =
            R.javascript.LessThan(CompareOperationHint::kNone);
        Node* cmp = R.Binop(less_than, p0, p1);
        Node* r = R.reduce(cmp);
        if (types[i].Is(Type::String()) && types[j].Is(Type::String())) {
          R.CheckBinop(R.simplified.StringLessThan(), r);
        } else if ((types[i].Is(Type::Number()) &&
                    types[j].Is(Type::Number())) ||
                   (!types[i].Maybe(Type::String()) ||
                    !types[j].Maybe(Type::String()))) {
          R.CheckBinop(R.simplified.NumberLessThan(), r);
        } else {
          // No reduction of mixed types.
          CHECK_EQ(r->op(), less_than);
        }
      }
    }
  }
}

TEST_F(JSTypedLoweringTesterTest, RemoveToNumberEffects) {
  JSTypedLoweringTester R(i_isolate(), zone());

  FeedbackSource feedback_source = FeedbackSourceWithOneBinarySlot(&R);
  Node* feedback = R.UndefinedConstant();
  Node* effect_use = nullptr;
  Node* zero = R.graph.NewNode(R.common.NumberConstant(0));
  for (int i = 0; i < 10; i++) {
    Node* p0 = R.Parameter(Type::Number());
    Node* ton = R.Unop(R.javascript.ToNumber(), p0);
    Node* frame_state = R.EmptyFrameState(R.context());
    effect_use = nullptr;

    switch (i) {
      case 0:
        CHECK_EQ(1, OperatorProperties::GetFrameStateInputCount(
                        R.javascript.ToNumber()));
        effect_use = R.graph.NewNode(R.javascript.ToNumber(), p0, R.context(),
                                     frame_state, ton, R.start());
        break;
      case 1:
        CHECK_EQ(1, OperatorProperties::GetFrameStateInputCount(
                        R.javascript.ToNumber()));
        effect_use = R.graph.NewNode(R.javascript.ToNumber(), ton, R.context(),
                                     frame_state, ton, R.start());
        break;
      case 2:
        effect_use = R.graph.NewNode(R.common.EffectPhi(1), ton, R.start());
        break;
      case 3:
        effect_use =
            R.graph.NewNode(R.javascript.Add(feedback_source), ton, ton,
                            feedback, R.context(), frame_state, ton, R.start());
        break;
      case 4:
        effect_use =
            R.graph.NewNode(R.javascript.Add(feedback_source), p0, p0, feedback,
                            R.context(), frame_state, ton, R.start());
        break;
      case 5:
        effect_use =
            R.graph.NewNode(R.common.Return(), zero, p0, ton, R.start());
        break;
      case 6:
        effect_use =
            R.graph.NewNode(R.common.Return(), zero, ton, ton, R.start());
    }

    R.CheckEffectInput(R.start(), ton);
    if (effect_use != nullptr) R.CheckEffectInput(ton, effect_use);

    Node* r = R.reduce(ton);
    CHECK_EQ(p0, r);
    CHECK_NE(R.start(), r);

    if (effect_use != nullptr) {
      R.CheckEffectInput(R.start(), effect_use);
      // Check that value uses of ToNumber() do not go to start().
      for (int j = 0; j < effect_use->op()->ValueInputCount(); j++) {
        CHECK_NE(R.start(), effect_use->InputAt(j));
      }
    }
  }

  CHECK(!effect_use);  // should have done all cases above.
}

// Helper class for testing the reduction of a single binop.
class BinopEffectsTester {
 public:
  BinopEffectsTester(Isolate* isolate, Zone* zone, const Operator* op, Type t0,
                     Type t1)
      : R(isolate, zone, 0),
        p0(R.Parameter(t0, 0)),
        p1(R.Parameter(t1, 1)),
        binop(R.Binop(op, p0, p1)),
        effect_use(R.graph.NewNode(R.common.EffectPhi(1), binop, R.start())) {
    // Effects should be ordered start -> binop -> effect_use
    R.CheckEffectInput(R.start(), binop);
    R.CheckEffectInput(binop, effect_use);
    result = R.reduce(binop);
  }

  JSTypedLoweringTester R;
  Node* p0;
  Node* p1;
  Node* binop;
  Node* effect_use;
  Node* result;

  void CheckEffectsRemoved() { R.CheckEffectInput(R.start(), effect_use); }

  void CheckEffectOrdering(Node* n0) {
    R.CheckEffectInput(R.start(), n0);
    R.CheckEffectInput(n0, effect_use);
  }

  void CheckEffectOrdering(Node* n0, Node* n1) {
    R.CheckEffectInput(R.start(), n0);
    R.CheckEffectInput(n0, n1);
    R.CheckEffectInput(n1, effect_use);
  }

  Node* CheckConvertedInput(IrOpcode::Value opcode, int which, bool effects) {
    return CheckConverted(opcode, result->InputAt(which), effects);
  }

  Node* CheckConverted(IrOpcode::Value opcode, Node* node, bool effects) {
    CHECK_EQ(opcode, node->opcode());
    if (effects) {
      CHECK_LT(0, node->op()->EffectInputCount());
    } else {
      CHECK_EQ(0, node->op()->EffectInputCount());
    }
    return node;
  }

  Node* CheckNoOp(int which) {
    CHECK_EQ(which == 0 ? p0 : p1, result->InputAt(which));
    return result->InputAt(which);
  }
};

// Helper function for strict and non-strict equality reductions.
void CheckEqualityReduction(JSTypedLoweringTester* R, bool strict, Node* l,
                            Node* r, IrOpcode::Value expected) {
  for (int j = 0; j < 2; j++) {
    Node* p0 = j == 0 ? l : r;
    Node* p1 = j == 1 ? l : r;

    {
      const Operator* op =
          strict ? R->javascript.StrictEqual(CompareOperationHint::kNone)
                 : R->javascript.Equal(CompareOperationHint::kNone);
      Node* eq = R->Binop(op, p0, p1);
      Node* reduced = R->reduce(eq);
      R->CheckBinop(expected, reduced);
    }
  }
}

TEST_F(JSTypedLoweringTesterTest, EqualityForNumbers) {
  JSTypedLoweringTester R(i_isolate(), zone());

  Type simple_number_types[] = {Type::UnsignedSmall(), Type::SignedSmall(),
                                Type::Signed32(), Type::Unsigned32(),
                                Type::Number()};

  for (size_t i = 0; i < arraysize(simple_number_types); ++i) {
    Node* p0 = R.Parameter(simple_number_types[i], 0);

    for (size_t j = 0; j < arraysize(simple_number_types); ++j) {
      Node* p1 = R.Parameter(simple_number_types[j], 1);

      CheckEqualityReduction(&R, true, p0, p1, IrOpcode::kNumberEqual);
      CheckEqualityReduction(&R, false, p0, p1, IrOpcode::kNumberEqual);
    }
  }
}

TEST_F(JSTypedLoweringTesterTest, StrictEqualityForRefEqualTypes) {
  JSTypedLoweringTester R(i_isolate(), zone());

  Type types[] = {Type::Undefined(), Type::Null(), Type::Boolean(),
                  Type::Object(), Type::Receiver()};

  Node* p0 = R.Parameter(Type::Any());
  for (size_t i = 0; i < arraysize(types); i++) {
    Node* p1 = R.Parameter(types[i]);
    CheckEqualityReduction(&R, true, p0, p1, IrOpcode::kReferenceEqual);
  }
}

TEST_F(JSTypedLoweringTesterTest, StrictEqualityForUnique) {
  JSTypedLoweringTester R(i_isolate(), zone());

  Node* p0 = R.Parameter(Type::Unique());
  Node* p1 = R.Parameter(Type::Unique());
  CheckEqualityReduction(&R, true, p0, p1, IrOpcode::kReferenceEqual);
  CheckEqualityReduction(&R, true, p1, p0, IrOpcode::kReferenceEqual);
}

TEST_F(JSTypedLoweringTesterTest, StringEquality) {
  JSTypedLoweringTester R(i_isolate(), zone());
  Node* p0 = R.Parameter(Type::String());
  Node* p1 = R.Parameter(Type::String());

  CheckEqualityReduction(&R, true, p0, p1, IrOpcode::kStringEqual);
  CheckEqualityReduction(&R, false, p0, p1, IrOpcode::kStringEqual);
}

TEST_F(JSTypedLoweringTesterTest, RemovePureNumberBinopEffects) {
  JSTypedLoweringTester R(i_isolate(), zone());
  FeedbackSource binary_source = FeedbackSourceWithOneBinarySlot(&R);

  const Operator* ops[] = {
      R.javascript.Equal(CompareOperationHint::kNone),
      R.simplified.NumberEqual(),
      R.javascript.Add(binary_source),
      R.simplified.NumberAdd(),
      R.javascript.Subtract(binary_source),
      R.simplified.NumberSubtract(),
      R.javascript.Multiply(binary_source),
      R.simplified.NumberMultiply(),
      R.javascript.Divide(binary_source),
      R.simplified.NumberDivide(),
      R.javascript.Modulus(binary_source),
      R.simplified.NumberModulus(),
      R.javascript.LessThan(CompareOperationHint::kNone),
      R.simplified.NumberLessThan(),
      R.javascript.LessThanOrEqual(CompareOperationHint::kNone),
      R.simplified.NumberLessThanOrEqual(),
  };

  for (size_t j = 0; j < arraysize(ops); j += 2) {
    BinopEffectsTester B(i_isolate(), zone(), ops[j], Type::Number(),
                         Type::Number());
    CHECK_EQ(ops[j + 1]->opcode(), B.result->op()->opcode());

    B.R.CheckBinop(B.result->opcode(), B.result);

    B.CheckNoOp(0);
    B.CheckNoOp(1);

    B.CheckEffectsRemoved();
  }
}

TEST_F(JSTypedLoweringTesterTest, Int32BinopEffects) {
  JSBitwiseTypedLoweringTester R(i_isolate(), zone());
  for (int j = 0; j < R.kNumberOps; j += 2) {
    bool signed_left = R.signedness[j], signed_right = R.signedness[j + 1];
    BinopEffectsTester B(i_isolate(), zone(), R.ops[j], I32Type(signed_left),
                         I32Type(signed_right));
    CHECK_EQ(R.ops[j + 1]->opcode(), B.result->op()->opcode());

    B.R.CheckBinop(B.result->opcode(), B.result);

    B.CheckNoOp(0);
    B.CheckNoOp(1);

    B.CheckEffectsRemoved();
  }

  for (int j = 0; j < R.kNumberOps; j += 2) {
    bool signed_left = R.signedness[j], signed_right = R.signedness[j + 1];
    BinopEffectsTester B(i_isolate(), zone(), R.ops[j], Type::Number(),
                         Type::Number());
    CHECK_EQ(R.ops[j + 1]->opcode(), B.result->op()->opcode());

    B.R.CheckBinop(B.result->opcode(), B.result);

    B.CheckConvertedInput(NumberToI32(signed_left), 0, false);
    B.CheckConvertedInput(NumberToI32(signed_right), 1, false);

    B.CheckEffectsRemoved();
  }

  for (int j = 0; j < R.kNumberOps; j += 2) {
    bool signed_left = R.signedness[j];
    BinopEffectsTester B(i_isolate(), zone(), R.ops[j], Type::Number(),
                         Type::Boolean());

    B.R.CheckBinop(B.result->opcode(), B.result);

    B.CheckConvertedInput(NumberToI32(signed_left), 0, false);
    B.CheckConvertedInput(IrOpcode::kPlainPrimitiveToNumber, 1, false);

    B.CheckEffectsRemoved();
  }

  for (int j = 0; j < R.kNumberOps; j += 2) {
    bool signed_right = R.signedness[j + 1];
    BinopEffectsTester B(i_isolate(), zone(), R.ops[j], Type::Boolean(),
                         Type::Number());

    B.R.CheckBinop(B.result->opcode(), B.result);

    B.CheckConvertedInput(IrOpcode::kPlainPrimitiveToNumber, 0, false);
    B.CheckConvertedInput(NumberToI32(signed_right), 1, false);

    B.CheckEffectsRemoved();
  }

  for (int j = 0; j < R.kNumberOps; j += 2) {
    BinopEffectsTester B(i_isolate(), zone(), R.ops[j], Type::Boolean(),
                         Type::Boolean());

    B.R.CheckBinop(B.result->opcode(), B.result);

    B.CheckConvertedInput(IrOpcode::kPlainPrimitiveToNumber, 0, false);
    B.CheckConvertedInput(IrOpcode::kPlainPrimitiveToNumber, 1, false);

    B.CheckEffectsRemoved();
  }
}

TEST_F(JSTypedLoweringTesterTest, Int32AddNarrowing) {
  {
    JSBitwiseTypedLoweringTester R(i_isolate(), zone());

    for (int o = 0; o < R.kNumberOps; o += 2) {
      for (size_t i = 0; i < arraysize(kInt32Types); i++) {
        Node* n0 = R.Parameter(kInt32Types[i]);
        for (size_t j = 0; j < arraysize(kInt32Types); j++) {
          Node* n1 = R.Parameter(kInt32Types[j]);
          Node* one = R.graph.NewNode(R.common.NumberConstant(1));

          for (int l = 0; l < 2; l++) {
            Node* add_node = R.Binop(R.simplified.NumberAdd(), n0, n1);
            Node* or_node =
                R.Binop(R.ops[o], l ? add_node : one, l ? one : add_node);
            Node* r = R.reduce(or_node);

            CHECK_EQ(R.ops[o + 1]->opcode(), r->op()->opcode());
            CHECK_EQ(IrOpcode::kNumberAdd, add_node->opcode());
          }
        }
      }
    }
  }
  {
    JSBitwiseShiftTypedLoweringTester R(i_isolate(), zone());

    for (int o = 0; o < R.kNumberOps; o += 2) {
      for (size_t i = 0; i < arraysize(kInt32Types); i++) {
        Node* n0 = R.Parameter(kInt32Types[i]);
        for (size_t j = 0; j < arraysize(kInt32Types); j++) {
          Node* n1 = R.Parameter(kInt32Types[j]);
          Node* one = R.graph.NewNode(R.common.NumberConstant(1));

          for (int l = 0; l < 2; l++) {
            Node* add_node = R.Binop(R.simplified.NumberAdd(), n0, n1);
            Node* or_node =
                R.Binop(R.ops[o], l ? add_node : one, l ? one : add_node);
            Node* r = R.reduce(or_node);

            CHECK_EQ(R.ops[o + 1]->opcode(), r->op()->opcode());
            CHECK_EQ(IrOpcode::kNumberAdd, add_node->opcode());
          }
        }
      }
    }
  }
  {
    JSBitwiseTypedLoweringTester R(i_isolate(), zone());

    for (int o = 0; o < R.kNumberOps; o += 2) {
      Node* n0 = R.Parameter(I32Type(R.signedness[o]));
      Node* n1 = R.Parameter(I32Type(R.signedness[o + 1]));
      Node* one = R.graph.NewNode(R.common.NumberConstant(1));

      Node* add_node = R.Binop(R.simplified.NumberAdd(), n0, n1);
      Node* or_node = R.Binop(R.ops[o], add_node, one);
      Node* other_use = R.Binop(R.simplified.NumberAdd(), add_node, one);
      Node* r = R.reduce(or_node);
      CHECK_EQ(R.ops[o + 1]->opcode(), r->op()->opcode());
      CHECK_EQ(IrOpcode::kNumberAdd, add_node->opcode());
      // Conversion to int32 should be done.
      CheckToI32(add_node, r->InputAt(0), R.signedness[o]);
      CheckToI32(one, r->InputAt(1), R.signedness[o + 1]);
      // The other use should also not be touched.
      CHECK_EQ(add_node, other_use->InputAt(0));
      CHECK_EQ(one, other_use->InputAt(1));
    }
  }
}

TEST_F(JSTypedLoweringTesterTest, Int32Comparisons) {
  JSTypedLoweringTester R(i_isolate(), zone());

  struct Entry {
    const Operator* js_op;
    const Operator* num_op;
    bool commute;
  };

  Entry ops[] = {{R.javascript.LessThan(CompareOperationHint::kNone),
                  R.simplified.NumberLessThan(), false},
                 {R.javascript.LessThanOrEqual(CompareOperationHint::kNone),
                  R.simplified.NumberLessThanOrEqual(), false},
                 {R.javascript.GreaterThan(CompareOperationHint::kNone),
                  R.simplified.NumberLessThan(), true},
                 {R.javascript.GreaterThanOrEqual(CompareOperationHint::kNone),
                  R.simplified.NumberLessThanOrEqual(), true}};

  for (size_t o = 0; o < arraysize(ops); o++) {
    for (size_t i = 0; i < arraysize(kNumberTypes); i++) {
      Type t0 = kNumberTypes[i];
      Node* p0 = R.Parameter(t0, 0);

      for (size_t j = 0; j < arraysize(kNumberTypes); j++) {
        Type t1 = kNumberTypes[j];
        Node* p1 = R.Parameter(t1, 1);

        Node* cmp = R.Binop(ops[o].js_op, p0, p1);
        Node* r = R.reduce(cmp);

        R.CheckBinop(ops[o].num_op, r);
        if (ops[o].commute) {
          CHECK_EQ(p1, r->InputAt(0));
          CHECK_EQ(p0, r->InputAt(1));
        } else {
          CHECK_EQ(p0, r->InputAt(0));
          CHECK_EQ(p1, r->InputAt(1));
        }
      }
    }
  }
}

}  // namespace js_typed_lowering_unittest
}  // namespace compiler
}  // namespace internal
}  // namespace v8

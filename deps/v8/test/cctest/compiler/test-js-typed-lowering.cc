// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/compiler/graph-inl.h"
#include "src/compiler/js-typed-lowering.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/typer.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

class JSTypedLoweringTester : public HandleAndZoneScope {
 public:
  explicit JSTypedLoweringTester(int num_parameters = 0)
      : isolate(main_isolate()),
        binop(NULL),
        unop(NULL),
        javascript(main_zone()),
        simplified(main_zone()),
        common(main_zone()),
        graph(main_zone()),
        typer(main_zone()),
        context_node(NULL) {
    typer.DecorateGraph(&graph);
    Node* s = graph.NewNode(common.Start(num_parameters));
    graph.SetStart(s);
  }

  Isolate* isolate;
  const Operator* binop;
  const Operator* unop;
  JSOperatorBuilder javascript;
  MachineOperatorBuilder machine;
  SimplifiedOperatorBuilder simplified;
  CommonOperatorBuilder common;
  Graph graph;
  Typer typer;
  Node* context_node;

  Node* Parameter(Type* t, int32_t index = 0) {
    Node* n = graph.NewNode(common.Parameter(index), graph.start());
    NodeProperties::SetBounds(n, Bounds(Type::None(), t));
    return n;
  }

  Node* UndefinedConstant() {
    Unique<Object> unique =
        Unique<Object>::CreateImmovable(isolate->factory()->undefined_value());
    return graph.NewNode(common.HeapConstant(unique));
  }

  Node* HeapConstant(Handle<Object> constant) {
    Unique<Object> unique = Unique<Object>::CreateUninitialized(constant);
    return graph.NewNode(common.HeapConstant(unique));
  }

  Node* EmptyFrameState(Node* context) {
    Node* parameters = graph.NewNode(common.StateValues(0));
    Node* locals = graph.NewNode(common.StateValues(0));
    Node* stack = graph.NewNode(common.StateValues(0));

    Node* state_node =
        graph.NewNode(common.FrameState(JS_FRAME, BailoutId(0), kIgnoreOutput),
                      parameters, locals, stack, context, UndefinedConstant());

    return state_node;
  }

  Node* reduce(Node* node) {
    JSGraph jsgraph(&graph, &common, &javascript, &typer, &machine);
    JSTypedLowering reducer(&jsgraph);
    Reduction reduction = reducer.Reduce(node);
    if (reduction.Changed()) return reduction.replacement();
    return node;
  }

  Node* start() { return graph.start(); }

  Node* context() {
    if (context_node == NULL) {
      context_node = graph.NewNode(common.Parameter(-1), graph.start());
    }
    return context_node;
  }

  Node* control() { return start(); }

  void CheckPureBinop(IrOpcode::Value expected, Node* node) {
    CHECK_EQ(expected, node->opcode());
    CHECK_EQ(2, node->InputCount());  // should not have context, effect, etc.
  }

  void CheckPureBinop(const Operator* expected, Node* node) {
    CHECK_EQ(expected->opcode(), node->op()->opcode());
    CHECK_EQ(2, node->InputCount());  // should not have context, effect, etc.
  }

  Node* ReduceUnop(const Operator* op, Type* input_type) {
    return reduce(Unop(op, Parameter(input_type)));
  }

  Node* ReduceBinop(const Operator* op, Type* left_type, Type* right_type) {
    return reduce(Binop(op, Parameter(left_type, 0), Parameter(right_type, 1)));
  }

  Node* Binop(const Operator* op, Node* left, Node* right) {
    // JS binops also require context, effect, and control
    return graph.NewNode(op, left, right, context(), start(), control());
  }

  Node* Unop(const Operator* op, Node* input) {
    // JS unops also require context, effect, and control
    return graph.NewNode(op, input, context(), start(), control());
  }

  Node* UseForEffect(Node* node) {
    // TODO(titzer): use EffectPhi after fixing EffectCount
    return graph.NewNode(javascript.ToNumber(), node, context(), node,
                         control());
  }

  void CheckEffectInput(Node* effect, Node* use) {
    CHECK_EQ(effect, NodeProperties::GetEffectInput(use));
  }

  void CheckInt32Constant(int32_t expected, Node* result) {
    CHECK_EQ(IrOpcode::kInt32Constant, result->opcode());
    CHECK_EQ(expected, OpParameter<int32_t>(result));
  }

  void CheckNumberConstant(double expected, Node* result) {
    CHECK_EQ(IrOpcode::kNumberConstant, result->opcode());
    CHECK_EQ(expected, OpParameter<double>(result));
  }

  void CheckNaN(Node* result) {
    CHECK_EQ(IrOpcode::kNumberConstant, result->opcode());
    double value = OpParameter<double>(result);
    CHECK(std::isnan(value));
  }

  void CheckTrue(Node* result) {
    CheckHandle(isolate->factory()->true_value(), result);
  }

  void CheckFalse(Node* result) {
    CheckHandle(isolate->factory()->false_value(), result);
  }

  void CheckHandle(Handle<Object> expected, Node* result) {
    CHECK_EQ(IrOpcode::kHeapConstant, result->opcode());
    Handle<Object> value = OpParameter<Unique<Object> >(result).handle();
    CHECK_EQ(*expected, *value);
  }
};

static Type* kStringTypes[] = {Type::InternalizedString(), Type::OtherString(),
                               Type::String()};


static Type* kInt32Types[] = {
    Type::UnsignedSmall(),   Type::OtherSignedSmall(), Type::OtherUnsigned31(),
    Type::OtherUnsigned32(), Type::OtherSigned32(),    Type::SignedSmall(),
    Type::Signed32(),        Type::Unsigned32(),       Type::Integral32()};


static Type* kNumberTypes[] = {
    Type::UnsignedSmall(),   Type::OtherSignedSmall(), Type::OtherUnsigned31(),
    Type::OtherUnsigned32(), Type::OtherSigned32(),    Type::SignedSmall(),
    Type::Signed32(),        Type::Unsigned32(),       Type::Integral32(),
    Type::MinusZero(),       Type::NaN(),              Type::OtherNumber(),
    Type::OrderedNumber(),   Type::Number()};


static Type* kJSTypes[] = {Type::Undefined(), Type::Null(),   Type::Boolean(),
                           Type::Number(),    Type::String(), Type::Object()};


static Type* I32Type(bool is_signed) {
  return is_signed ? Type::Signed32() : Type::Unsigned32();
}


static IrOpcode::Value NumberToI32(bool is_signed) {
  return is_signed ? IrOpcode::kNumberToInt32 : IrOpcode::kNumberToUint32;
}


// TODO(turbofan): Lowering of StringAdd is disabled for now.
#if 0
TEST(StringBinops) {
  JSTypedLoweringTester R;

  for (size_t i = 0; i < arraysize(kStringTypes); ++i) {
    Node* p0 = R.Parameter(kStringTypes[i], 0);

    for (size_t j = 0; j < arraysize(kStringTypes); ++j) {
      Node* p1 = R.Parameter(kStringTypes[j], 1);

      Node* add = R.Binop(R.javascript.Add(), p0, p1);
      Node* r = R.reduce(add);

      R.CheckPureBinop(IrOpcode::kStringAdd, r);
      CHECK_EQ(p0, r->InputAt(0));
      CHECK_EQ(p1, r->InputAt(1));
    }
  }
}
#endif


TEST(AddNumber1) {
  JSTypedLoweringTester R;
  for (size_t i = 0; i < arraysize(kNumberTypes); ++i) {
    Node* p0 = R.Parameter(kNumberTypes[i], 0);
    Node* p1 = R.Parameter(kNumberTypes[i], 1);
    Node* add = R.Binop(R.javascript.Add(), p0, p1);
    Node* r = R.reduce(add);

    R.CheckPureBinop(IrOpcode::kNumberAdd, r);
    CHECK_EQ(p0, r->InputAt(0));
    CHECK_EQ(p1, r->InputAt(1));
  }
}


TEST(NumberBinops) {
  JSTypedLoweringTester R;
  const Operator* ops[] = {
      R.javascript.Add(),      R.simplified.NumberAdd(),
      R.javascript.Subtract(), R.simplified.NumberSubtract(),
      R.javascript.Multiply(), R.simplified.NumberMultiply(),
      R.javascript.Divide(),   R.simplified.NumberDivide(),
      R.javascript.Modulus(),  R.simplified.NumberModulus(),
  };

  for (size_t i = 0; i < arraysize(kNumberTypes); ++i) {
    Node* p0 = R.Parameter(kNumberTypes[i], 0);

    for (size_t j = 0; j < arraysize(kNumberTypes); ++j) {
      Node* p1 = R.Parameter(kNumberTypes[j], 1);

      for (size_t k = 0; k < arraysize(ops); k += 2) {
        Node* add = R.Binop(ops[k], p0, p1);
        Node* r = R.reduce(add);

        R.CheckPureBinop(ops[k + 1], r);
        CHECK_EQ(p0, r->InputAt(0));
        CHECK_EQ(p1, r->InputAt(1));
      }
    }
  }
}


static void CheckToI32(Node* old_input, Node* new_input, bool is_signed) {
  Type* old_type = NodeProperties::GetBounds(old_input).upper;
  Type* expected_type = I32Type(is_signed);
  if (old_type->Is(expected_type)) {
    CHECK_EQ(old_input, new_input);
  } else if (new_input->opcode() == IrOpcode::kNumberConstant) {
    CHECK(NodeProperties::GetBounds(new_input).upper->Is(expected_type));
    double v = OpParameter<double>(new_input);
    double e = static_cast<double>(is_signed ? FastD2I(v) : FastD2UI(v));
    CHECK_EQ(e, v);
  } else {
    CHECK_EQ(NumberToI32(is_signed), new_input->opcode());
  }
}


// A helper class for testing lowering of bitwise shift operators.
class JSBitwiseShiftTypedLoweringTester : public JSTypedLoweringTester {
 public:
  static const int kNumberOps = 6;
  const Operator* ops[kNumberOps];
  bool signedness[kNumberOps];

  JSBitwiseShiftTypedLoweringTester() {
    int i = 0;
    set(i++, javascript.ShiftLeft(), true);
    set(i++, machine.Word32Shl(), false);
    set(i++, javascript.ShiftRight(), true);
    set(i++, machine.Word32Sar(), false);
    set(i++, javascript.ShiftRightLogical(), false);
    set(i++, machine.Word32Shr(), false);
  }

 private:
  void set(int idx, const Operator* op, bool s) {
    ops[idx] = op;
    signedness[idx] = s;
  }
};


TEST(Int32BitwiseShifts) {
  JSBitwiseShiftTypedLoweringTester R;

  Type* types[] = {
      Type::SignedSmall(), Type::UnsignedSmall(), Type::OtherSigned32(),
      Type::Unsigned32(),  Type::Signed32(),      Type::MinusZero(),
      Type::NaN(),         Type::OtherNumber(),   Type::Undefined(),
      Type::Null(),        Type::Boolean(),       Type::Number(),
      Type::String(),      Type::Object()};

  for (size_t i = 0; i < arraysize(types); ++i) {
    Node* p0 = R.Parameter(types[i], 0);

    for (size_t j = 0; j < arraysize(types); ++j) {
      Node* p1 = R.Parameter(types[j], 1);

      for (int k = 0; k < R.kNumberOps; k += 2) {
        Node* add = R.Binop(R.ops[k], p0, p1);
        Node* r = R.reduce(add);

        R.CheckPureBinop(R.ops[k + 1], r);
        Node* r0 = r->InputAt(0);
        Node* r1 = r->InputAt(1);

        CheckToI32(p0, r0, R.signedness[k]);

        R.CheckPureBinop(IrOpcode::kWord32And, r1);
        CheckToI32(p1, r1->InputAt(0), R.signedness[k + 1]);
        R.CheckInt32Constant(0x1F, r1->InputAt(1));
      }
    }
  }
}


// A helper class for testing lowering of bitwise operators.
class JSBitwiseTypedLoweringTester : public JSTypedLoweringTester {
 public:
  static const int kNumberOps = 6;
  const Operator* ops[kNumberOps];
  bool signedness[kNumberOps];

  JSBitwiseTypedLoweringTester() {
    int i = 0;
    set(i++, javascript.BitwiseOr(), true);
    set(i++, machine.Word32Or(), true);
    set(i++, javascript.BitwiseXor(), true);
    set(i++, machine.Word32Xor(), true);
    set(i++, javascript.BitwiseAnd(), true);
    set(i++, machine.Word32And(), true);
  }

 private:
  void set(int idx, const Operator* op, bool s) {
    ops[idx] = op;
    signedness[idx] = s;
  }
};


TEST(Int32BitwiseBinops) {
  JSBitwiseTypedLoweringTester R;

  Type* types[] = {
      Type::SignedSmall(), Type::UnsignedSmall(), Type::OtherSigned32(),
      Type::Unsigned32(),  Type::Signed32(),      Type::MinusZero(),
      Type::NaN(),         Type::OtherNumber(),   Type::Undefined(),
      Type::Null(),        Type::Boolean(),       Type::Number(),
      Type::String(),      Type::Object()};

  for (size_t i = 0; i < arraysize(types); ++i) {
    Node* p0 = R.Parameter(types[i], 0);

    for (size_t j = 0; j < arraysize(types); ++j) {
      Node* p1 = R.Parameter(types[j], 1);

      for (int k = 0; k < R.kNumberOps; k += 2) {
        Node* add = R.Binop(R.ops[k], p0, p1);
        Node* r = R.reduce(add);

        R.CheckPureBinop(R.ops[k + 1], r);

        CheckToI32(p0, r->InputAt(0), R.signedness[k]);
        CheckToI32(p1, r->InputAt(1), R.signedness[k + 1]);
      }
    }
  }
}


TEST(JSToNumber1) {
  JSTypedLoweringTester R;
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


TEST(JSToNumber_replacement) {
  JSTypedLoweringTester R;

  Type* types[] = {Type::Null(), Type::Undefined(), Type::Number()};

  for (size_t i = 0; i < arraysize(types); i++) {
    Node* n = R.Parameter(types[i]);
    Node* c = R.graph.NewNode(R.javascript.ToNumber(), n, R.context(),
                              R.start(), R.start());
    Node* effect_use = R.UseForEffect(c);
    Node* add = R.graph.NewNode(R.simplified.ReferenceEqual(Type::Any()), n, c);

    R.CheckEffectInput(c, effect_use);
    Node* r = R.reduce(c);

    if (types[i]->Is(Type::Number())) {
      CHECK_EQ(n, r);
    } else {
      CHECK_EQ(IrOpcode::kNumberConstant, r->opcode());
    }

    CHECK_EQ(n, add->InputAt(0));
    CHECK_EQ(r, add->InputAt(1));
    R.CheckEffectInput(R.start(), effect_use);
  }
}


TEST(JSToNumberOfConstant) {
  JSTypedLoweringTester R;

  const Operator* ops[] = {
      R.common.NumberConstant(0), R.common.NumberConstant(-1),
      R.common.NumberConstant(0.1), R.common.Int32Constant(1177),
      R.common.Float64Constant(0.99)};

  for (size_t i = 0; i < arraysize(ops); i++) {
    Node* n = R.graph.NewNode(ops[i]);
    Node* convert = R.Unop(R.javascript.ToNumber(), n);
    Node* r = R.reduce(convert);
    // Note that either outcome below is correct. It only depends on whether
    // the types of constants are eagerly computed or only computed by the
    // typing pass.
    if (NodeProperties::GetBounds(n).upper->Is(Type::Number())) {
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


TEST(JSToNumberOfNumberOrOtherPrimitive) {
  JSTypedLoweringTester R;
  Type* others[] = {Type::Undefined(), Type::Null(), Type::Boolean(),
                    Type::String()};

  for (size_t i = 0; i < arraysize(others); i++) {
    Type* t = Type::Union(Type::Number(), others[i], R.main_zone());
    Node* r = R.ReduceUnop(R.javascript.ToNumber(), t);
    CHECK_EQ(IrOpcode::kJSToNumber, r->opcode());
  }
}


TEST(JSToBoolean) {
  JSTypedLoweringTester R;
  const Operator* op = R.javascript.ToBoolean();

  {  // ToBoolean(undefined)
    Node* r = R.ReduceUnop(op, Type::Undefined());
    R.CheckFalse(r);
  }

  {  // ToBoolean(null)
    Node* r = R.ReduceUnop(op, Type::Null());
    R.CheckFalse(r);
  }

  {  // ToBoolean(boolean)
    Node* r = R.ReduceUnop(op, Type::Boolean());
    CHECK_EQ(IrOpcode::kParameter, r->opcode());
  }

  {  // ToBoolean(ordered-number)
    Node* r = R.ReduceUnop(op, Type::OrderedNumber());
    CHECK_EQ(IrOpcode::kBooleanNot, r->opcode());
    Node* i = r->InputAt(0);
    CHECK_EQ(IrOpcode::kNumberEqual, i->opcode());
    // ToBoolean(x:ordered-number) => BooleanNot(NumberEqual(x, #0))
  }

  {  // ToBoolean(string)
    Node* r = R.ReduceUnop(op, Type::String());
    // TODO(titzer): test will break with better js-typed-lowering
    CHECK_EQ(IrOpcode::kJSToBoolean, r->opcode());
  }

  {  // ToBoolean(object)
    Node* r = R.ReduceUnop(op, Type::DetectableObject());
    R.CheckTrue(r);
  }

  {  // ToBoolean(undetectable)
    Node* r = R.ReduceUnop(op, Type::Undetectable());
    R.CheckFalse(r);
  }

  {  // ToBoolean(object)
    Node* r = R.ReduceUnop(op, Type::Object());
    CHECK_EQ(IrOpcode::kJSToBoolean, r->opcode());
  }
}


TEST(JSToBoolean_replacement) {
  JSTypedLoweringTester R;

  Type* types[] = {Type::Null(),             Type::Undefined(),
                   Type::Boolean(),          Type::OrderedNumber(),
                   Type::DetectableObject(), Type::Undetectable()};

  for (size_t i = 0; i < arraysize(types); i++) {
    Node* n = R.Parameter(types[i]);
    Node* c = R.graph.NewNode(R.javascript.ToBoolean(), n, R.context(),
                              R.start(), R.start());
    Node* effect_use = R.UseForEffect(c);
    Node* add = R.graph.NewNode(R.simplified.ReferenceEqual(Type::Any()), n, c);

    R.CheckEffectInput(c, effect_use);
    Node* r = R.reduce(c);

    if (types[i]->Is(Type::Boolean())) {
      CHECK_EQ(n, r);
    } else if (types[i]->Is(Type::OrderedNumber())) {
      CHECK_EQ(IrOpcode::kBooleanNot, r->opcode());
    } else {
      CHECK_EQ(IrOpcode::kHeapConstant, r->opcode());
    }

    CHECK_EQ(n, add->InputAt(0));
    CHECK_EQ(r, add->InputAt(1));
    R.CheckEffectInput(R.start(), effect_use);
  }
}


TEST(JSToString1) {
  JSTypedLoweringTester R;

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
    // TODO(titzer): could be a branch
    CHECK_EQ(IrOpcode::kJSToString, r->opcode());
  }

  {  // ToString(number)
    Node* r = R.ReduceUnop(op, Type::Number());
    // TODO(titzer): could remove effects
    CHECK_EQ(IrOpcode::kJSToString, r->opcode());
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


TEST(JSToString_replacement) {
  JSTypedLoweringTester R;

  Type* types[] = {Type::Null(), Type::Undefined(), Type::String()};

  for (size_t i = 0; i < arraysize(types); i++) {
    Node* n = R.Parameter(types[i]);
    Node* c = R.graph.NewNode(R.javascript.ToString(), n, R.context(),
                              R.start(), R.start());
    Node* effect_use = R.UseForEffect(c);
    Node* add = R.graph.NewNode(R.simplified.ReferenceEqual(Type::Any()), n, c);

    R.CheckEffectInput(c, effect_use);
    Node* r = R.reduce(c);

    if (types[i]->Is(Type::String())) {
      CHECK_EQ(n, r);
    } else {
      CHECK_EQ(IrOpcode::kHeapConstant, r->opcode());
    }

    CHECK_EQ(n, add->InputAt(0));
    CHECK_EQ(r, add->InputAt(1));
    R.CheckEffectInput(R.start(), effect_use);
  }
}


TEST(StringComparison) {
  JSTypedLoweringTester R;

  const Operator* ops[] = {
      R.javascript.LessThan(),           R.simplified.StringLessThan(),
      R.javascript.LessThanOrEqual(),    R.simplified.StringLessThanOrEqual(),
      R.javascript.GreaterThan(),        R.simplified.StringLessThan(),
      R.javascript.GreaterThanOrEqual(), R.simplified.StringLessThanOrEqual()};

  for (size_t i = 0; i < arraysize(kStringTypes); i++) {
    Node* p0 = R.Parameter(kStringTypes[i], 0);
    for (size_t j = 0; j < arraysize(kStringTypes); j++) {
      Node* p1 = R.Parameter(kStringTypes[j], 1);

      for (size_t k = 0; k < arraysize(ops); k += 2) {
        Node* cmp = R.Binop(ops[k], p0, p1);
        Node* r = R.reduce(cmp);

        R.CheckPureBinop(ops[k + 1], r);
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
  if (NodeProperties::GetBounds(val).upper->Is(Type::Number())) {
    CHECK_EQ(val, converted);
  } else if (NodeProperties::GetBounds(val).upper->Is(Type::Boolean())) {
    CHECK_EQ(IrOpcode::kBooleanToNumber, converted->opcode());
    CHECK_EQ(val, converted->InputAt(0));
  } else {
    if (converted->opcode() == IrOpcode::kNumberConstant) return;
    CHECK_EQ(IrOpcode::kJSToNumber, converted->opcode());
    CHECK_EQ(val, converted->InputAt(0));
  }
}


TEST(NumberComparison) {
  JSTypedLoweringTester R;

  const Operator* ops[] = {
      R.javascript.LessThan(),           R.simplified.NumberLessThan(),
      R.javascript.LessThanOrEqual(),    R.simplified.NumberLessThanOrEqual(),
      R.javascript.GreaterThan(),        R.simplified.NumberLessThan(),
      R.javascript.GreaterThanOrEqual(), R.simplified.NumberLessThanOrEqual()};

  for (size_t i = 0; i < arraysize(kJSTypes); i++) {
    Type* t0 = kJSTypes[i];
    // Skip Type::String and Type::Receiver which might coerce into a string.
    if (t0->Is(Type::String()) || t0->Is(Type::Receiver())) continue;
    Node* p0 = R.Parameter(t0, 0);

    for (size_t j = 0; j < arraysize(kJSTypes); j++) {
      Type* t1 = kJSTypes[j];
      // Skip Type::String and Type::Receiver which might coerce into a string.
      if (t1->Is(Type::String()) || t0->Is(Type::Receiver())) continue;
      Node* p1 = R.Parameter(t1, 1);

      for (size_t k = 0; k < arraysize(ops); k += 2) {
        Node* cmp = R.Binop(ops[k], p0, p1);
        Node* r = R.reduce(cmp);

        R.CheckPureBinop(ops[k + 1], r);
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
  }
}


TEST(MixedComparison1) {
  JSTypedLoweringTester R;

  Type* types[] = {Type::Number(), Type::String(),
                   Type::Union(Type::Number(), Type::String(), R.main_zone())};

  for (size_t i = 0; i < arraysize(types); i++) {
    Node* p0 = R.Parameter(types[i], 0);

    for (size_t j = 0; j < arraysize(types); j++) {
      Node* p1 = R.Parameter(types[j], 1);
      {
        Node* cmp = R.Binop(R.javascript.LessThan(), p0, p1);
        Node* r = R.reduce(cmp);

        if (!types[i]->Maybe(Type::String()) ||
            !types[j]->Maybe(Type::String())) {
          if (types[i]->Is(Type::String()) && types[j]->Is(Type::String())) {
            R.CheckPureBinop(R.simplified.StringLessThan(), r);
          } else {
            R.CheckPureBinop(R.simplified.NumberLessThan(), r);
          }
        } else {
          CHECK_EQ(cmp, r);  // No reduction of mixed types.
        }
      }
    }
  }
}


TEST(ObjectComparison) {
  JSTypedLoweringTester R;

  Node* p0 = R.Parameter(Type::Number(), 0);
  Node* p1 = R.Parameter(Type::Object(), 1);

  Node* cmp = R.Binop(R.javascript.LessThan(), p0, p1);
  Node* effect_use = R.UseForEffect(cmp);

  R.CheckEffectInput(R.start(), cmp);
  R.CheckEffectInput(cmp, effect_use);

  Node* r = R.reduce(cmp);

  R.CheckPureBinop(R.simplified.NumberLessThan(), r);

  Node* i0 = r->InputAt(0);
  Node* i1 = r->InputAt(1);

  CHECK_EQ(p0, i0);
  CHECK_NE(p1, i1);
  CHECK_EQ(IrOpcode::kParameter, i0->opcode());
  CHECK_EQ(IrOpcode::kJSToNumber, i1->opcode());

  // Check effect chain is correct.
  R.CheckEffectInput(R.start(), i1);
  R.CheckEffectInput(i1, effect_use);
}


TEST(UnaryNot) {
  JSTypedLoweringTester R;
  const Operator* opnot = R.javascript.UnaryNot();

  for (size_t i = 0; i < arraysize(kJSTypes); i++) {
    Node* orig = R.Unop(opnot, R.Parameter(kJSTypes[i]));
    Node* use = R.graph.NewNode(R.common.Return(), orig);
    Node* r = R.reduce(orig);
    // TODO(titzer): test will break if/when js-typed-lowering constant folds.
    CHECK_EQ(IrOpcode::kBooleanNot, use->InputAt(0)->opcode());

    if (r == orig && orig->opcode() == IrOpcode::kJSToBoolean) {
      // The original node was turned into a ToBoolean.
      CHECK_EQ(IrOpcode::kJSToBoolean, r->opcode());
    } else {
      CHECK_EQ(IrOpcode::kBooleanNot, r->opcode());
    }
  }
}


TEST(RemoveToNumberEffects) {
  FLAG_turbo_deoptimization = true;

  JSTypedLoweringTester R;

  Node* effect_use = NULL;
  for (int i = 0; i < 10; i++) {
    Node* p0 = R.Parameter(Type::Number());
    Node* ton = R.Unop(R.javascript.ToNumber(), p0);
    Node* frame_state = R.EmptyFrameState(R.context());
    effect_use = NULL;

    switch (i) {
      case 0:
        effect_use = R.graph.NewNode(R.javascript.ToNumber(), p0, R.context(),
                                     ton, R.start());
        break;
      case 1:
        effect_use = R.graph.NewNode(R.javascript.ToNumber(), ton, R.context(),
                                     ton, R.start());
        break;
      case 2:
        effect_use = R.graph.NewNode(R.common.EffectPhi(1), ton, R.start());
      case 3:
        effect_use = R.graph.NewNode(R.javascript.Add(), ton, ton, R.context(),
                                     frame_state, ton, R.start());
        break;
      case 4:
        effect_use = R.graph.NewNode(R.javascript.Add(), p0, p0, R.context(),
                                     frame_state, ton, R.start());
        break;
      case 5:
        effect_use = R.graph.NewNode(R.common.Return(), p0, ton, R.start());
        break;
      case 6:
        effect_use = R.graph.NewNode(R.common.Return(), ton, ton, R.start());
    }

    R.CheckEffectInput(R.start(), ton);
    if (effect_use != NULL) R.CheckEffectInput(ton, effect_use);

    Node* r = R.reduce(ton);
    CHECK_EQ(p0, r);
    CHECK_NE(R.start(), r);

    if (effect_use != NULL) {
      R.CheckEffectInput(R.start(), effect_use);
      // Check that value uses of ToNumber() do not go to start().
      for (int i = 0; i < effect_use->op()->InputCount(); i++) {
        CHECK_NE(R.start(), effect_use->InputAt(i));
      }
    }
  }

  CHECK_EQ(NULL, effect_use);  // should have done all cases above.
}


// Helper class for testing the reduction of a single binop.
class BinopEffectsTester {
 public:
  explicit BinopEffectsTester(const Operator* op, Type* t0, Type* t1)
      : R(),
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
      CHECK_LT(0, OperatorProperties::GetEffectInputCount(node->op()));
    } else {
      CHECK_EQ(0, OperatorProperties::GetEffectInputCount(node->op()));
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
      Node* eq = strict ? R->graph.NewNode(R->javascript.StrictEqual(), p0, p1)
                        : R->Binop(R->javascript.Equal(), p0, p1);
      Node* r = R->reduce(eq);
      R->CheckPureBinop(expected, r);
    }

    {
      Node* ne = strict
                     ? R->graph.NewNode(R->javascript.StrictNotEqual(), p0, p1)
                     : R->Binop(R->javascript.NotEqual(), p0, p1);
      Node* n = R->reduce(ne);
      CHECK_EQ(IrOpcode::kBooleanNot, n->opcode());
      Node* r = n->InputAt(0);
      R->CheckPureBinop(expected, r);
    }
  }
}


TEST(EqualityForNumbers) {
  JSTypedLoweringTester R;

  Type* simple_number_types[] = {Type::UnsignedSmall(), Type::SignedSmall(),
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


TEST(StrictEqualityForRefEqualTypes) {
  JSTypedLoweringTester R;

  Type* types[] = {Type::Undefined(), Type::Null(), Type::Boolean(),
                   Type::Object(), Type::Receiver()};

  Node* p0 = R.Parameter(Type::Any());
  for (size_t i = 0; i < arraysize(types); i++) {
    Node* p1 = R.Parameter(types[i]);
    CheckEqualityReduction(&R, true, p0, p1, IrOpcode::kReferenceEqual);
  }
  // TODO(titzer): Equal(RefEqualTypes)
}


TEST(StringEquality) {
  JSTypedLoweringTester R;
  Node* p0 = R.Parameter(Type::String());
  Node* p1 = R.Parameter(Type::String());

  CheckEqualityReduction(&R, true, p0, p1, IrOpcode::kStringEqual);
  CheckEqualityReduction(&R, false, p0, p1, IrOpcode::kStringEqual);
}


TEST(RemovePureNumberBinopEffects) {
  JSTypedLoweringTester R;

  const Operator* ops[] = {
      R.javascript.Equal(),           R.simplified.NumberEqual(),
      R.javascript.Add(),             R.simplified.NumberAdd(),
      R.javascript.Subtract(),        R.simplified.NumberSubtract(),
      R.javascript.Multiply(),        R.simplified.NumberMultiply(),
      R.javascript.Divide(),          R.simplified.NumberDivide(),
      R.javascript.Modulus(),         R.simplified.NumberModulus(),
      R.javascript.LessThan(),        R.simplified.NumberLessThan(),
      R.javascript.LessThanOrEqual(), R.simplified.NumberLessThanOrEqual(),
  };

  for (size_t j = 0; j < arraysize(ops); j += 2) {
    BinopEffectsTester B(ops[j], Type::Number(), Type::Number());
    CHECK_EQ(ops[j + 1]->opcode(), B.result->op()->opcode());

    B.R.CheckPureBinop(B.result->opcode(), B.result);

    B.CheckNoOp(0);
    B.CheckNoOp(1);

    B.CheckEffectsRemoved();
  }
}


TEST(OrderNumberBinopEffects1) {
  JSTypedLoweringTester R;

  const Operator* ops[] = {
      R.javascript.Subtract(), R.simplified.NumberSubtract(),
      R.javascript.Multiply(), R.simplified.NumberMultiply(),
      R.javascript.Divide(),   R.simplified.NumberDivide(),
      R.javascript.Modulus(),  R.simplified.NumberModulus(),
  };

  for (size_t j = 0; j < arraysize(ops); j += 2) {
    BinopEffectsTester B(ops[j], Type::Object(), Type::String());
    CHECK_EQ(ops[j + 1]->opcode(), B.result->op()->opcode());

    Node* i0 = B.CheckConvertedInput(IrOpcode::kJSToNumber, 0, true);
    Node* i1 = B.CheckConvertedInput(IrOpcode::kJSToNumber, 1, true);

    CHECK_EQ(B.p0, i0->InputAt(0));
    CHECK_EQ(B.p1, i1->InputAt(0));

    // Effects should be ordered start -> i0 -> i1 -> effect_use
    B.CheckEffectOrdering(i0, i1);
  }
}


TEST(OrderNumberBinopEffects2) {
  JSTypedLoweringTester R;

  const Operator* ops[] = {
      R.javascript.Add(),      R.simplified.NumberAdd(),
      R.javascript.Subtract(), R.simplified.NumberSubtract(),
      R.javascript.Multiply(), R.simplified.NumberMultiply(),
      R.javascript.Divide(),   R.simplified.NumberDivide(),
      R.javascript.Modulus(),  R.simplified.NumberModulus(),
  };

  for (size_t j = 0; j < arraysize(ops); j += 2) {
    BinopEffectsTester B(ops[j], Type::Number(), Type::Symbol());

    Node* i0 = B.CheckNoOp(0);
    Node* i1 = B.CheckConvertedInput(IrOpcode::kJSToNumber, 1, true);

    CHECK_EQ(B.p0, i0);
    CHECK_EQ(B.p1, i1->InputAt(0));

    // Effects should be ordered start -> i1 -> effect_use
    B.CheckEffectOrdering(i1);
  }

  for (size_t j = 0; j < arraysize(ops); j += 2) {
    BinopEffectsTester B(ops[j], Type::Symbol(), Type::Number());

    Node* i0 = B.CheckConvertedInput(IrOpcode::kJSToNumber, 0, true);
    Node* i1 = B.CheckNoOp(1);

    CHECK_EQ(B.p0, i0->InputAt(0));
    CHECK_EQ(B.p1, i1);

    // Effects should be ordered start -> i0 -> effect_use
    B.CheckEffectOrdering(i0);
  }
}


TEST(OrderCompareEffects) {
  JSTypedLoweringTester R;

  const Operator* ops[] = {
      R.javascript.GreaterThan(), R.simplified.NumberLessThan(),
      R.javascript.GreaterThanOrEqual(), R.simplified.NumberLessThanOrEqual(),
  };

  for (size_t j = 0; j < arraysize(ops); j += 2) {
    BinopEffectsTester B(ops[j], Type::Symbol(), Type::String());
    CHECK_EQ(ops[j + 1]->opcode(), B.result->op()->opcode());

    Node* i0 = B.CheckConvertedInput(IrOpcode::kJSToNumber, 0, true);
    Node* i1 = B.CheckConvertedInput(IrOpcode::kJSToNumber, 1, true);

    // Inputs should be commuted.
    CHECK_EQ(B.p1, i0->InputAt(0));
    CHECK_EQ(B.p0, i1->InputAt(0));

    // But effects should be ordered start -> i1 -> i0 -> effect_use
    B.CheckEffectOrdering(i1, i0);
  }

  for (size_t j = 0; j < arraysize(ops); j += 2) {
    BinopEffectsTester B(ops[j], Type::Number(), Type::Symbol());

    Node* i0 = B.CheckConvertedInput(IrOpcode::kJSToNumber, 0, true);
    Node* i1 = B.result->InputAt(1);

    CHECK_EQ(B.p1, i0->InputAt(0));  // Should be commuted.
    CHECK_EQ(B.p0, i1);

    // Effects should be ordered start -> i1 -> effect_use
    B.CheckEffectOrdering(i0);
  }

  for (size_t j = 0; j < arraysize(ops); j += 2) {
    BinopEffectsTester B(ops[j], Type::Symbol(), Type::Number());

    Node* i0 = B.result->InputAt(0);
    Node* i1 = B.CheckConvertedInput(IrOpcode::kJSToNumber, 1, true);

    CHECK_EQ(B.p1, i0);  // Should be commuted.
    CHECK_EQ(B.p0, i1->InputAt(0));

    // Effects should be ordered start -> i0 -> effect_use
    B.CheckEffectOrdering(i1);
  }
}


TEST(Int32BinopEffects) {
  JSBitwiseTypedLoweringTester R;

  for (int j = 0; j < R.kNumberOps; j += 2) {
    bool signed_left = R.signedness[j], signed_right = R.signedness[j + 1];
    BinopEffectsTester B(R.ops[j], I32Type(signed_left), I32Type(signed_right));
    CHECK_EQ(R.ops[j + 1]->opcode(), B.result->op()->opcode());

    B.R.CheckPureBinop(B.result->opcode(), B.result);

    B.CheckNoOp(0);
    B.CheckNoOp(1);

    B.CheckEffectsRemoved();
  }

  for (int j = 0; j < R.kNumberOps; j += 2) {
    bool signed_left = R.signedness[j], signed_right = R.signedness[j + 1];
    BinopEffectsTester B(R.ops[j], Type::Number(), Type::Number());
    CHECK_EQ(R.ops[j + 1]->opcode(), B.result->op()->opcode());

    B.R.CheckPureBinop(B.result->opcode(), B.result);

    B.CheckConvertedInput(NumberToI32(signed_left), 0, false);
    B.CheckConvertedInput(NumberToI32(signed_right), 1, false);

    B.CheckEffectsRemoved();
  }

  for (int j = 0; j < R.kNumberOps; j += 2) {
    bool signed_left = R.signedness[j], signed_right = R.signedness[j + 1];
    BinopEffectsTester B(R.ops[j], Type::Number(), Type::Object());

    B.R.CheckPureBinop(B.result->opcode(), B.result);

    Node* i0 = B.CheckConvertedInput(NumberToI32(signed_left), 0, false);
    Node* i1 = B.CheckConvertedInput(NumberToI32(signed_right), 1, false);

    CHECK_EQ(B.p0, i0->InputAt(0));
    Node* ii1 = B.CheckConverted(IrOpcode::kJSToNumber, i1->InputAt(0), true);

    CHECK_EQ(B.p1, ii1->InputAt(0));

    B.CheckEffectOrdering(ii1);
  }

  for (int j = 0; j < R.kNumberOps; j += 2) {
    bool signed_left = R.signedness[j], signed_right = R.signedness[j + 1];
    BinopEffectsTester B(R.ops[j], Type::Object(), Type::Number());

    B.R.CheckPureBinop(B.result->opcode(), B.result);

    Node* i0 = B.CheckConvertedInput(NumberToI32(signed_left), 0, false);
    Node* i1 = B.CheckConvertedInput(NumberToI32(signed_right), 1, false);

    Node* ii0 = B.CheckConverted(IrOpcode::kJSToNumber, i0->InputAt(0), true);
    CHECK_EQ(B.p1, i1->InputAt(0));

    CHECK_EQ(B.p0, ii0->InputAt(0));

    B.CheckEffectOrdering(ii0);
  }

  for (int j = 0; j < R.kNumberOps; j += 2) {
    bool signed_left = R.signedness[j], signed_right = R.signedness[j + 1];
    BinopEffectsTester B(R.ops[j], Type::Object(), Type::Object());

    B.R.CheckPureBinop(B.result->opcode(), B.result);

    Node* i0 = B.CheckConvertedInput(NumberToI32(signed_left), 0, false);
    Node* i1 = B.CheckConvertedInput(NumberToI32(signed_right), 1, false);

    Node* ii0 = B.CheckConverted(IrOpcode::kJSToNumber, i0->InputAt(0), true);
    Node* ii1 = B.CheckConverted(IrOpcode::kJSToNumber, i1->InputAt(0), true);

    CHECK_EQ(B.p0, ii0->InputAt(0));
    CHECK_EQ(B.p1, ii1->InputAt(0));

    B.CheckEffectOrdering(ii0, ii1);
  }
}


TEST(UnaryNotEffects) {
  JSTypedLoweringTester R;
  const Operator* opnot = R.javascript.UnaryNot();

  for (size_t i = 0; i < arraysize(kJSTypes); i++) {
    Node* p0 = R.Parameter(kJSTypes[i], 0);
    Node* orig = R.Unop(opnot, p0);
    Node* effect_use = R.UseForEffect(orig);
    Node* value_use = R.graph.NewNode(R.common.Return(), orig);
    Node* r = R.reduce(orig);
    // TODO(titzer): test will break if/when js-typed-lowering constant folds.
    CHECK_EQ(IrOpcode::kBooleanNot, value_use->InputAt(0)->opcode());

    if (r == orig && orig->opcode() == IrOpcode::kJSToBoolean) {
      // The original node was turned into a ToBoolean, which has an effect.
      CHECK_EQ(IrOpcode::kJSToBoolean, r->opcode());
      R.CheckEffectInput(R.start(), orig);
      R.CheckEffectInput(orig, effect_use);
    } else {
      // effect should have been removed from this node.
      CHECK_EQ(IrOpcode::kBooleanNot, r->opcode());
      R.CheckEffectInput(R.start(), effect_use);
    }
  }
}


TEST(Int32AddNarrowing) {
  {
    JSBitwiseTypedLoweringTester R;

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
            CHECK_EQ(IrOpcode::kInt32Add, add_node->opcode());
            bool is_signed = l ? R.signedness[o] : R.signedness[o + 1];

            Type* add_type = NodeProperties::GetBounds(add_node).upper;
            CHECK(add_type->Is(I32Type(is_signed)));
          }
        }
      }
    }
  }
  {
    JSBitwiseShiftTypedLoweringTester R;

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
            CHECK_EQ(IrOpcode::kInt32Add, add_node->opcode());
            bool is_signed = l ? R.signedness[o] : R.signedness[o + 1];

            Type* add_type = NodeProperties::GetBounds(add_node).upper;
            CHECK(add_type->Is(I32Type(is_signed)));
          }
        }
      }
    }
  }
}


TEST(Int32AddNarrowingNotOwned) {
  JSBitwiseTypedLoweringTester R;

  for (int o = 0; o < R.kNumberOps; o += 2) {
    Node* n0 = R.Parameter(I32Type(R.signedness[o]));
    Node* n1 = R.Parameter(I32Type(R.signedness[o + 1]));
    Node* one = R.graph.NewNode(R.common.NumberConstant(1));

    Node* add_node = R.Binop(R.simplified.NumberAdd(), n0, n1);
    Node* or_node = R.Binop(R.ops[o], add_node, one);
    Node* other_use = R.Binop(R.simplified.NumberAdd(), add_node, one);
    Node* r = R.reduce(or_node);
    CHECK_EQ(R.ops[o + 1]->opcode(), r->op()->opcode());
    // Should not be reduced to Int32Add because of the other number add.
    CHECK_EQ(IrOpcode::kNumberAdd, add_node->opcode());
    // Conversion to int32 should be done.
    CheckToI32(add_node, r->InputAt(0), R.signedness[o]);
    CheckToI32(one, r->InputAt(1), R.signedness[o + 1]);
    // The other use should also not be touched.
    CHECK_EQ(add_node, other_use->InputAt(0));
    CHECK_EQ(one, other_use->InputAt(1));
  }
}


TEST(Int32Comparisons) {
  JSTypedLoweringTester R;

  struct Entry {
    const Operator* js_op;
    const Operator* uint_op;
    const Operator* int_op;
    const Operator* num_op;
    bool commute;
  };

  Entry ops[] = {
      {R.javascript.LessThan(), R.machine.Uint32LessThan(),
       R.machine.Int32LessThan(), R.simplified.NumberLessThan(), false},
      {R.javascript.LessThanOrEqual(), R.machine.Uint32LessThanOrEqual(),
       R.machine.Int32LessThanOrEqual(), R.simplified.NumberLessThanOrEqual(),
       false},
      {R.javascript.GreaterThan(), R.machine.Uint32LessThan(),
       R.machine.Int32LessThan(), R.simplified.NumberLessThan(), true},
      {R.javascript.GreaterThanOrEqual(), R.machine.Uint32LessThanOrEqual(),
       R.machine.Int32LessThanOrEqual(), R.simplified.NumberLessThanOrEqual(),
       true}};

  for (size_t o = 0; o < arraysize(ops); o++) {
    for (size_t i = 0; i < arraysize(kNumberTypes); i++) {
      Type* t0 = kNumberTypes[i];
      Node* p0 = R.Parameter(t0, 0);

      for (size_t j = 0; j < arraysize(kNumberTypes); j++) {
        Type* t1 = kNumberTypes[j];
        Node* p1 = R.Parameter(t1, 1);

        Node* cmp = R.Binop(ops[o].js_op, p0, p1);
        Node* r = R.reduce(cmp);

        const Operator* expected;
        if (t0->Is(Type::Unsigned32()) && t1->Is(Type::Unsigned32())) {
          expected = ops[o].uint_op;
        } else if (t0->Is(Type::Signed32()) && t1->Is(Type::Signed32())) {
          expected = ops[o].int_op;
        } else {
          expected = ops[o].num_op;
        }
        R.CheckPureBinop(expected, r);
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

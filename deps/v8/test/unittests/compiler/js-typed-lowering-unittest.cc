// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/js-typed-lowering.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-properties-inl.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::BitEq;
using testing::IsNaN;


namespace v8 {
namespace internal {
namespace compiler {

namespace {

const ExternalArrayType kExternalArrayTypes[] = {
    kExternalUint8Array,   kExternalInt8Array,   kExternalUint16Array,
    kExternalInt16Array,   kExternalUint32Array, kExternalInt32Array,
    kExternalFloat32Array, kExternalFloat64Array};


const double kFloat64Values[] = {
    -V8_INFINITY, -4.23878e+275, -5.82632e+265, -6.60355e+220, -6.26172e+212,
    -2.56222e+211, -4.82408e+201, -1.84106e+157, -1.63662e+127, -1.55772e+100,
    -1.67813e+72, -2.3382e+55, -3.179e+30, -1.441e+09, -1.0647e+09,
    -7.99361e+08, -5.77375e+08, -2.20984e+08, -32757, -13171, -9970, -3984,
    -107, -105, -92, -77, -61, -0.000208163, -1.86685e-06, -1.17296e-10,
    -9.26358e-11, -5.08004e-60, -1.74753e-65, -1.06561e-71, -5.67879e-79,
    -5.78459e-130, -2.90989e-171, -7.15489e-243, -3.76242e-252, -1.05639e-263,
    -4.40497e-267, -2.19666e-273, -4.9998e-276, -5.59821e-278, -2.03855e-282,
    -5.99335e-283, -7.17554e-284, -3.11744e-309, -0.0, 0.0, 2.22507e-308,
    1.30127e-270, 7.62898e-260, 4.00313e-249, 3.16829e-233, 1.85244e-228,
    2.03544e-129, 1.35126e-110, 1.01182e-106, 5.26333e-94, 1.35292e-90,
    2.85394e-83, 1.78323e-77, 5.4967e-57, 1.03207e-25, 4.57401e-25, 1.58738e-05,
    2, 125, 2310, 9636, 14802, 17168, 28945, 29305, 4.81336e+07, 1.41207e+08,
    4.65962e+08, 1.40499e+09, 2.12648e+09, 8.80006e+30, 1.4446e+45, 1.12164e+54,
    2.48188e+89, 6.71121e+102, 3.074e+112, 4.9699e+152, 5.58383e+166,
    4.30654e+172, 7.08824e+185, 9.6586e+214, 2.028e+223, 6.63277e+243,
    1.56192e+261, 1.23202e+269, 5.72883e+289, 8.5798e+290, 1.40256e+294,
    1.79769e+308, V8_INFINITY};


const size_t kIndices[] = {0, 1, 42, 100, 1024};


const double kIntegerValues[] = {-V8_INFINITY, INT_MIN, -1000.0,  -42.0,
                                 -1.0,         0.0,     1.0,      42.0,
                                 1000.0,       INT_MAX, UINT_MAX, V8_INFINITY};


Type* const kJSTypes[] = {Type::Undefined(), Type::Null(),   Type::Boolean(),
                          Type::Number(),    Type::String(), Type::Object()};


const StrictMode kStrictModes[] = {SLOPPY, STRICT};

}  // namespace


class JSTypedLoweringTest : public TypedGraphTest {
 public:
  JSTypedLoweringTest() : TypedGraphTest(3), javascript_(zone()) {}
  ~JSTypedLoweringTest() OVERRIDE {}

 protected:
  Reduction Reduce(Node* node) {
    MachineOperatorBuilder machine(zone());
    JSGraph jsgraph(graph(), common(), javascript(), &machine);
    JSTypedLowering reducer(&jsgraph, zone());
    return reducer.Reduce(node);
  }

  Handle<JSArrayBuffer> NewArrayBuffer(void* bytes, size_t byte_length) {
    Handle<JSArrayBuffer> buffer = factory()->NewJSArrayBuffer();
    Runtime::SetupArrayBuffer(isolate(), buffer, true, bytes, byte_length);
    return buffer;
  }

  Matcher<Node*> IsIntPtrConstant(intptr_t value) {
    return sizeof(value) == 4 ? IsInt32Constant(static_cast<int32_t>(value))
                              : IsInt64Constant(static_cast<int64_t>(value));
  }

  JSOperatorBuilder* javascript() { return &javascript_; }

 private:
  JSOperatorBuilder javascript_;
};


// -----------------------------------------------------------------------------
// JSUnaryNot


TEST_F(JSTypedLoweringTest, JSUnaryNotWithBoolean) {
  Node* input = Parameter(Type::Boolean(), 0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r =
      Reduce(graph()->NewNode(javascript()->UnaryNot(), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsBooleanNot(input));
}


TEST_F(JSTypedLoweringTest, JSUnaryNotWithFalsish) {
  Handle<Object> zero = factory()->NewNumber(0);
  Node* input = Parameter(
      Type::Union(
          Type::MinusZero(),
          Type::Union(
              Type::NaN(),
              Type::Union(
                  Type::Null(),
                  Type::Union(
                      Type::Undefined(),
                      Type::Union(
                          Type::Undetectable(),
                          Type::Union(
                              Type::Constant(factory()->false_value(), zone()),
                              Type::Range(zero, zero, zone()), zone()),
                          zone()),
                      zone()),
                  zone()),
              zone()),
          zone()),
      0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r =
      Reduce(graph()->NewNode(javascript()->UnaryNot(), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsTrueConstant());
}


TEST_F(JSTypedLoweringTest, JSUnaryNotWithTruish) {
  Node* input = Parameter(
      Type::Union(
          Type::Constant(factory()->true_value(), zone()),
          Type::Union(Type::DetectableReceiver(), Type::Symbol(), zone()),
          zone()),
      0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r =
      Reduce(graph()->NewNode(javascript()->UnaryNot(), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsFalseConstant());
}


TEST_F(JSTypedLoweringTest, JSUnaryNotWithNonZeroPlainNumber) {
  Node* input = Parameter(
      Type::Range(factory()->NewNumber(1), factory()->NewNumber(42), zone()),
      0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r =
      Reduce(graph()->NewNode(javascript()->UnaryNot(), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsFalseConstant());
}


TEST_F(JSTypedLoweringTest, JSUnaryNotWithAny) {
  Node* input = Parameter(Type::Any(), 0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r =
      Reduce(graph()->NewNode(javascript()->UnaryNot(), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsBooleanNot(IsAnyToBoolean(input)));
}


// -----------------------------------------------------------------------------
// Constant propagation


TEST_F(JSTypedLoweringTest, ParameterWithMinusZero) {
  {
    Reduction r = Reduce(
        Parameter(Type::Constant(factory()->minus_zero_value(), zone())));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberConstant(-0.0));
  }
  {
    Reduction r = Reduce(Parameter(Type::MinusZero()));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberConstant(-0.0));
  }
  {
    Reduction r = Reduce(Parameter(
        Type::Union(Type::MinusZero(),
                    Type::Constant(factory()->NewNumber(0), zone()), zone())));
    EXPECT_FALSE(r.Changed());
  }
}


TEST_F(JSTypedLoweringTest, ParameterWithNull) {
  Handle<HeapObject> null = factory()->null_value();
  {
    Reduction r = Reduce(Parameter(Type::Constant(null, zone())));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsHeapConstant(Unique<HeapObject>::CreateImmovable(null)));
  }
  {
    Reduction r = Reduce(Parameter(Type::Null()));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsHeapConstant(Unique<HeapObject>::CreateImmovable(null)));
  }
}


TEST_F(JSTypedLoweringTest, ParameterWithNaN) {
  const double kNaNs[] = {base::OS::nan_value(),
                          std::numeric_limits<double>::quiet_NaN(),
                          std::numeric_limits<double>::signaling_NaN()};
  TRACED_FOREACH(double, nan, kNaNs) {
    Handle<Object> constant = factory()->NewNumber(nan);
    Reduction r = Reduce(Parameter(Type::Constant(constant, zone())));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberConstant(IsNaN()));
  }
  {
    Reduction r =
        Reduce(Parameter(Type::Constant(factory()->nan_value(), zone())));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberConstant(IsNaN()));
  }
  {
    Reduction r = Reduce(Parameter(Type::NaN()));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberConstant(IsNaN()));
  }
}


TEST_F(JSTypedLoweringTest, ParameterWithPlainNumber) {
  TRACED_FOREACH(double, value, kFloat64Values) {
    Handle<Object> constant = factory()->NewNumber(value);
    Reduction r = Reduce(Parameter(Type::Constant(constant, zone())));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberConstant(value));
  }
  TRACED_FOREACH(double, value, kIntegerValues) {
    Handle<Object> constant = factory()->NewNumber(value);
    Reduction r = Reduce(Parameter(Type::Range(constant, constant, zone())));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsNumberConstant(value));
  }
}


TEST_F(JSTypedLoweringTest, ParameterWithUndefined) {
  Handle<HeapObject> undefined = factory()->undefined_value();
  {
    Reduction r = Reduce(Parameter(Type::Undefined()));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsHeapConstant(Unique<HeapObject>::CreateImmovable(undefined)));
  }
  {
    Reduction r = Reduce(Parameter(Type::Constant(undefined, zone())));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsHeapConstant(Unique<HeapObject>::CreateImmovable(undefined)));
  }
}


// -----------------------------------------------------------------------------
// JSToBoolean


TEST_F(JSTypedLoweringTest, JSToBooleanWithBoolean) {
  Node* input = Parameter(Type::Boolean(), 0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r =
      Reduce(graph()->NewNode(javascript()->ToBoolean(), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(input, r.replacement());
}


TEST_F(JSTypedLoweringTest, JSToBooleanWithFalsish) {
  Handle<Object> zero = factory()->NewNumber(0);
  Node* input = Parameter(
      Type::Union(
          Type::MinusZero(),
          Type::Union(
              Type::NaN(),
              Type::Union(
                  Type::Null(),
                  Type::Union(
                      Type::Undefined(),
                      Type::Union(
                          Type::Undetectable(),
                          Type::Union(
                              Type::Constant(factory()->false_value(), zone()),
                              Type::Range(zero, zero, zone()), zone()),
                          zone()),
                      zone()),
                  zone()),
              zone()),
          zone()),
      0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r =
      Reduce(graph()->NewNode(javascript()->ToBoolean(), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsFalseConstant());
}


TEST_F(JSTypedLoweringTest, JSToBooleanWithTruish) {
  Node* input = Parameter(
      Type::Union(
          Type::Constant(factory()->true_value(), zone()),
          Type::Union(Type::DetectableReceiver(), Type::Symbol(), zone()),
          zone()),
      0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r =
      Reduce(graph()->NewNode(javascript()->ToBoolean(), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsTrueConstant());
}


TEST_F(JSTypedLoweringTest, JSToBooleanWithNonZeroPlainNumber) {
  Node* input =
      Parameter(Type::Range(factory()->NewNumber(1),
                            factory()->NewNumber(V8_INFINITY), zone()),
                0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r =
      Reduce(graph()->NewNode(javascript()->ToBoolean(), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsTrueConstant());
}


TEST_F(JSTypedLoweringTest, JSToBooleanWithAny) {
  Node* input = Parameter(Type::Any(), 0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r =
      Reduce(graph()->NewNode(javascript()->ToBoolean(), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsAnyToBoolean(input));
}


// -----------------------------------------------------------------------------
// JSToNumber


TEST_F(JSTypedLoweringTest, JSToNumberWithPlainPrimitive) {
  Node* const input = Parameter(Type::PlainPrimitive(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ToNumber(), input,
                                        context, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsToNumber(input, IsNumberConstant(BitEq(0.0)),
                                          graph()->start(), control));
}


// -----------------------------------------------------------------------------
// JSStrictEqual


TEST_F(JSTypedLoweringTest, JSStrictEqualWithTheHole) {
  Node* const the_hole = HeapConstant(factory()->the_hole_value());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  TRACED_FOREACH(Type*, type, kJSTypes) {
    Node* const lhs = Parameter(type);
    Reduction r = Reduce(graph()->NewNode(javascript()->StrictEqual(), lhs,
                                          the_hole, context, effect, control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsFalseConstant());
  }
}


// -----------------------------------------------------------------------------
// JSShiftLeft


TEST_F(JSTypedLoweringTest, JSShiftLeftWithSigned32AndConstant) {
  Node* const lhs = Parameter(Type::Signed32());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  TRACED_FORRANGE(double, rhs, 0, 31) {
    Reduction r =
        Reduce(graph()->NewNode(javascript()->ShiftLeft(), lhs,
                                NumberConstant(rhs), context, effect, control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsWord32Shl(lhs, IsNumberConstant(BitEq(rhs))));
  }
}


TEST_F(JSTypedLoweringTest, JSShiftLeftWithSigned32AndUnsigned32) {
  Node* const lhs = Parameter(Type::Signed32());
  Node* const rhs = Parameter(Type::Unsigned32());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ShiftLeft(), lhs, rhs,
                                        context, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsWord32Shl(lhs, IsWord32And(rhs, IsInt32Constant(0x1f))));
}


// -----------------------------------------------------------------------------
// JSShiftRight


TEST_F(JSTypedLoweringTest, JSShiftRightWithSigned32AndConstant) {
  Node* const lhs = Parameter(Type::Signed32());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  TRACED_FORRANGE(double, rhs, 0, 31) {
    Reduction r =
        Reduce(graph()->NewNode(javascript()->ShiftRight(), lhs,
                                NumberConstant(rhs), context, effect, control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsWord32Sar(lhs, IsNumberConstant(BitEq(rhs))));
  }
}


TEST_F(JSTypedLoweringTest, JSShiftRightWithSigned32AndUnsigned32) {
  Node* const lhs = Parameter(Type::Signed32());
  Node* const rhs = Parameter(Type::Unsigned32());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ShiftRight(), lhs, rhs,
                                        context, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsWord32Sar(lhs, IsWord32And(rhs, IsInt32Constant(0x1f))));
}


// -----------------------------------------------------------------------------
// JSShiftRightLogical


TEST_F(JSTypedLoweringTest, JSShiftRightLogicalWithUnsigned32AndConstant) {
  Node* const lhs = Parameter(Type::Unsigned32());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  TRACED_FORRANGE(double, rhs, 0, 31) {
    Reduction r =
        Reduce(graph()->NewNode(javascript()->ShiftRightLogical(), lhs,
                                NumberConstant(rhs), context, effect, control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsWord32Shr(lhs, IsNumberConstant(BitEq(rhs))));
  }
}


TEST_F(JSTypedLoweringTest, JSShiftRightLogicalWithUnsigned32AndUnsigned32) {
  Node* const lhs = Parameter(Type::Unsigned32());
  Node* const rhs = Parameter(Type::Unsigned32());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ShiftRightLogical(), lhs,
                                        rhs, context, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsWord32Shr(lhs, IsWord32And(rhs, IsInt32Constant(0x1f))));
}


// -----------------------------------------------------------------------------
// JSLoadContext


TEST_F(JSTypedLoweringTest, JSLoadContext) {
  Node* const context = Parameter(Type::Any());
  Node* const effect = graph()->start();
  static bool kBooleans[] = {false, true};
  TRACED_FOREACH(size_t, index, kIndices) {
    TRACED_FOREACH(bool, immutable, kBooleans) {
      Reduction const r1 = Reduce(
          graph()->NewNode(javascript()->LoadContext(0, index, immutable),
                           context, context, effect));
      ASSERT_TRUE(r1.Changed());
      EXPECT_THAT(r1.replacement(),
                  IsLoadField(AccessBuilder::ForContextSlot(index), context,
                              effect, graph()->start()));

      Reduction const r2 = Reduce(
          graph()->NewNode(javascript()->LoadContext(1, index, immutable),
                           context, context, effect));
      ASSERT_TRUE(r2.Changed());
      EXPECT_THAT(r2.replacement(),
                  IsLoadField(AccessBuilder::ForContextSlot(index),
                              IsLoadField(AccessBuilder::ForContextSlot(
                                              Context::PREVIOUS_INDEX),
                                          context, effect, graph()->start()),
                              effect, graph()->start()));
    }
  }
}


// -----------------------------------------------------------------------------
// JSStoreContext


TEST_F(JSTypedLoweringTest, JSStoreContext) {
  Node* const context = Parameter(Type::Any());
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  TRACED_FOREACH(size_t, index, kIndices) {
    TRACED_FOREACH(Type*, type, kJSTypes) {
      Node* const value = Parameter(type);

      Reduction const r1 =
          Reduce(graph()->NewNode(javascript()->StoreContext(0, index), context,
                                  value, context, effect, control));
      ASSERT_TRUE(r1.Changed());
      EXPECT_THAT(r1.replacement(),
                  IsStoreField(AccessBuilder::ForContextSlot(index), context,
                               value, effect, control));

      Reduction const r2 =
          Reduce(graph()->NewNode(javascript()->StoreContext(1, index), context,
                                  value, context, effect, control));
      ASSERT_TRUE(r2.Changed());
      EXPECT_THAT(r2.replacement(),
                  IsStoreField(AccessBuilder::ForContextSlot(index),
                               IsLoadField(AccessBuilder::ForContextSlot(
                                               Context::PREVIOUS_INDEX),
                                           context, effect, graph()->start()),
                               value, effect, control));
    }
  }
}


// -----------------------------------------------------------------------------
// JSLoadProperty


TEST_F(JSTypedLoweringTest, JSLoadPropertyFromExternalTypedArray) {
  const size_t kLength = 17;
  double backing_store[kLength];
  Handle<JSArrayBuffer> buffer =
      NewArrayBuffer(backing_store, sizeof(backing_store));
  VectorSlotPair feedback(Handle<TypeFeedbackVector>::null(),
                          FeedbackVectorICSlot::Invalid());
  TRACED_FOREACH(ExternalArrayType, type, kExternalArrayTypes) {
    Handle<JSTypedArray> array =
        factory()->NewJSTypedArray(type, buffer, 0, kLength);
    int const element_size = static_cast<int>(array->element_size());

    Node* key = Parameter(
        Type::Range(factory()->NewNumber(kMinInt / element_size),
                    factory()->NewNumber(kMaxInt / element_size), zone()));
    Node* base = HeapConstant(array);
    Node* context = UndefinedConstant();
    Node* effect = graph()->start();
    Node* control = graph()->start();
    Node* node = graph()->NewNode(javascript()->LoadProperty(feedback), base,
                                  key, context);
    if (FLAG_turbo_deoptimization) {
      node->AppendInput(zone(), UndefinedConstant());
    }
    node->AppendInput(zone(), effect);
    node->AppendInput(zone(), control);
    Reduction r = Reduce(node);

    Matcher<Node*> offset_matcher =
        element_size == 1
            ? key
            : IsWord32Shl(key, IsInt32Constant(WhichPowerOf2(element_size)));

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(
        r.replacement(),
        IsLoadBuffer(BufferAccess(type),
                     IsIntPtrConstant(bit_cast<intptr_t>(&backing_store[0])),
                     offset_matcher,
                     IsNumberConstant(array->byte_length()->Number()), effect,
                     control));
  }
}


TEST_F(JSTypedLoweringTest, JSLoadPropertyFromExternalTypedArrayWithSafeKey) {
  const size_t kLength = 17;
  double backing_store[kLength];
  Handle<JSArrayBuffer> buffer =
      NewArrayBuffer(backing_store, sizeof(backing_store));
  VectorSlotPair feedback(Handle<TypeFeedbackVector>::null(),
                          FeedbackVectorICSlot::Invalid());
  TRACED_FOREACH(ExternalArrayType, type, kExternalArrayTypes) {
    Handle<JSTypedArray> array =
        factory()->NewJSTypedArray(type, buffer, 0, kLength);
    ElementAccess access = AccessBuilder::ForTypedArrayElement(type, true);

    int min = random_number_generator()->NextInt(static_cast<int>(kLength));
    int max = random_number_generator()->NextInt(static_cast<int>(kLength));
    if (min > max) std::swap(min, max);
    Node* key = Parameter(Type::Range(factory()->NewNumber(min),
                                      factory()->NewNumber(max), zone()));
    Node* base = HeapConstant(array);
    Node* context = UndefinedConstant();
    Node* effect = graph()->start();
    Node* control = graph()->start();
    Node* node = graph()->NewNode(javascript()->LoadProperty(feedback), base,
                                  key, context);
    if (FLAG_turbo_deoptimization) {
      node->AppendInput(zone(), UndefinedConstant());
    }
    node->AppendInput(zone(), effect);
    node->AppendInput(zone(), control);
    Reduction r = Reduce(node);

    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(
        r.replacement(),
        IsLoadElement(access,
                      IsIntPtrConstant(bit_cast<intptr_t>(&backing_store[0])),
                      key, effect, control));
  }
}


// -----------------------------------------------------------------------------
// JSStoreProperty


TEST_F(JSTypedLoweringTest, JSStorePropertyToExternalTypedArray) {
  const size_t kLength = 17;
  double backing_store[kLength];
  Handle<JSArrayBuffer> buffer =
      NewArrayBuffer(backing_store, sizeof(backing_store));
  TRACED_FOREACH(ExternalArrayType, type, kExternalArrayTypes) {
    TRACED_FOREACH(StrictMode, strict_mode, kStrictModes) {
      Handle<JSTypedArray> array =
          factory()->NewJSTypedArray(type, buffer, 0, kLength);
      int const element_size = static_cast<int>(array->element_size());

      Node* key = Parameter(
          Type::Range(factory()->NewNumber(kMinInt / element_size),
                      factory()->NewNumber(kMaxInt / element_size), zone()));
      Node* base = HeapConstant(array);
      Node* value =
          Parameter(AccessBuilder::ForTypedArrayElement(type, true).type);
      Node* context = UndefinedConstant();
      Node* effect = graph()->start();
      Node* control = graph()->start();
      Node* node = graph()->NewNode(javascript()->StoreProperty(strict_mode),
                                    base, key, value, context);
      if (FLAG_turbo_deoptimization) {
        node->AppendInput(zone(), UndefinedConstant());
      }
      node->AppendInput(zone(), effect);
      node->AppendInput(zone(), control);
      Reduction r = Reduce(node);

      Matcher<Node*> offset_matcher =
          element_size == 1
              ? key
              : IsWord32Shl(key, IsInt32Constant(WhichPowerOf2(element_size)));

      ASSERT_TRUE(r.Changed());
      EXPECT_THAT(
          r.replacement(),
          IsStoreBuffer(BufferAccess(type),
                        IsIntPtrConstant(bit_cast<intptr_t>(&backing_store[0])),
                        offset_matcher,
                        IsNumberConstant(array->byte_length()->Number()), value,
                        effect, control));
    }
  }
}


TEST_F(JSTypedLoweringTest, JSStorePropertyToExternalTypedArrayWithConversion) {
  const size_t kLength = 17;
  double backing_store[kLength];
  Handle<JSArrayBuffer> buffer =
      NewArrayBuffer(backing_store, sizeof(backing_store));
  TRACED_FOREACH(ExternalArrayType, type, kExternalArrayTypes) {
    TRACED_FOREACH(StrictMode, strict_mode, kStrictModes) {
      Handle<JSTypedArray> array =
          factory()->NewJSTypedArray(type, buffer, 0, kLength);
      int const element_size = static_cast<int>(array->element_size());

      Node* key = Parameter(
          Type::Range(factory()->NewNumber(kMinInt / element_size),
                      factory()->NewNumber(kMaxInt / element_size), zone()));
      Node* base = HeapConstant(array);
      Node* value = Parameter(Type::Any());
      Node* context = UndefinedConstant();
      Node* effect = graph()->start();
      Node* control = graph()->start();
      Node* node = graph()->NewNode(javascript()->StoreProperty(strict_mode),
                                    base, key, value, context);
      if (FLAG_turbo_deoptimization) {
        node->AppendInput(zone(), UndefinedConstant());
      }
      node->AppendInput(zone(), effect);
      node->AppendInput(zone(), control);
      Reduction r = Reduce(node);

      Matcher<Node*> offset_matcher =
          element_size == 1
              ? key
              : IsWord32Shl(key, IsInt32Constant(WhichPowerOf2(element_size)));

      Matcher<Node*> value_matcher =
          IsToNumber(value, context, effect, control);
      Matcher<Node*> effect_matcher = value_matcher;
      if (AccessBuilder::ForTypedArrayElement(type, true)
              .type->Is(Type::Signed32())) {
        value_matcher = IsNumberToInt32(value_matcher);
      } else if (AccessBuilder::ForTypedArrayElement(type, true)
                     .type->Is(Type::Unsigned32())) {
        value_matcher = IsNumberToUint32(value_matcher);
      }

      ASSERT_TRUE(r.Changed());
      EXPECT_THAT(
          r.replacement(),
          IsStoreBuffer(BufferAccess(type),
                        IsIntPtrConstant(bit_cast<intptr_t>(&backing_store[0])),
                        offset_matcher,
                        IsNumberConstant(array->byte_length()->Number()),
                        value_matcher, effect_matcher, control));
    }
  }
}


TEST_F(JSTypedLoweringTest, JSStorePropertyToExternalTypedArrayWithSafeKey) {
  const size_t kLength = 17;
  double backing_store[kLength];
  Handle<JSArrayBuffer> buffer =
      NewArrayBuffer(backing_store, sizeof(backing_store));
  TRACED_FOREACH(ExternalArrayType, type, kExternalArrayTypes) {
    TRACED_FOREACH(StrictMode, strict_mode, kStrictModes) {
      Handle<JSTypedArray> array =
          factory()->NewJSTypedArray(type, buffer, 0, kLength);
      ElementAccess access = AccessBuilder::ForTypedArrayElement(type, true);

      int min = random_number_generator()->NextInt(static_cast<int>(kLength));
      int max = random_number_generator()->NextInt(static_cast<int>(kLength));
      if (min > max) std::swap(min, max);
      Node* key = Parameter(Type::Range(factory()->NewNumber(min),
                                        factory()->NewNumber(max), zone()));
      Node* base = HeapConstant(array);
      Node* value = Parameter(access.type);
      Node* context = UndefinedConstant();
      Node* effect = graph()->start();
      Node* control = graph()->start();
      Node* node = graph()->NewNode(javascript()->StoreProperty(strict_mode),
                                    base, key, value, context);
      if (FLAG_turbo_deoptimization) {
        node->AppendInput(zone(), UndefinedConstant());
      }
      node->AppendInput(zone(), effect);
      node->AppendInput(zone(), control);
      Reduction r = Reduce(node);

      ASSERT_TRUE(r.Changed());
      EXPECT_THAT(
          r.replacement(),
          IsStoreElement(
              access, IsIntPtrConstant(bit_cast<intptr_t>(&backing_store[0])),
              key, value, effect, control));
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

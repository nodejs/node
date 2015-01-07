// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/js-typed-lowering.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/typer.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

const ExternalArrayType kExternalArrayTypes[] = {
    kExternalUint8Array,   kExternalInt8Array,   kExternalUint16Array,
    kExternalInt16Array,   kExternalUint32Array, kExternalInt32Array,
    kExternalFloat32Array, kExternalFloat64Array};


const size_t kIndices[] = {0, 1, 42, 100, 1024};


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
// JSToBoolean


TEST_F(JSTypedLoweringTest, JSToBooleanWithBoolean) {
  Node* input = Parameter(Type::Boolean());
  Node* context = UndefinedConstant();

  Reduction r =
      Reduce(graph()->NewNode(javascript()->ToBoolean(), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(input, r.replacement());
}


TEST_F(JSTypedLoweringTest, JSToBooleanWithUndefined) {
  Node* input = Parameter(Type::Undefined());
  Node* context = UndefinedConstant();

  Reduction r =
      Reduce(graph()->NewNode(javascript()->ToBoolean(), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsFalseConstant());
}


TEST_F(JSTypedLoweringTest, JSToBooleanWithNull) {
  Node* input = Parameter(Type::Null());
  Node* context = UndefinedConstant();

  Reduction r =
      Reduce(graph()->NewNode(javascript()->ToBoolean(), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsFalseConstant());
}


TEST_F(JSTypedLoweringTest, JSToBooleanWithDetectableReceiver) {
  Node* input = Parameter(Type::DetectableReceiver());
  Node* context = UndefinedConstant();

  Reduction r =
      Reduce(graph()->NewNode(javascript()->ToBoolean(), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsTrueConstant());
}


TEST_F(JSTypedLoweringTest, JSToBooleanWithUndetectable) {
  Node* input = Parameter(Type::Undetectable());
  Node* context = UndefinedConstant();

  Reduction r =
      Reduce(graph()->NewNode(javascript()->ToBoolean(), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsFalseConstant());
}


TEST_F(JSTypedLoweringTest, JSToBooleanWithOrderedNumber) {
  Node* input = Parameter(Type::OrderedNumber());
  Node* context = UndefinedConstant();

  Reduction r =
      Reduce(graph()->NewNode(javascript()->ToBoolean(), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsBooleanNot(IsNumberEqual(input, IsNumberConstant(0))));
}


TEST_F(JSTypedLoweringTest, JSToBooleanWithString) {
  Node* input = Parameter(Type::String());
  Node* context = UndefinedConstant();

  Reduction r =
      Reduce(graph()->NewNode(javascript()->ToBoolean(), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsBooleanNot(IsNumberEqual(
                  IsLoadField(AccessBuilder::ForStringLength(), input,
                              graph()->start(), graph()->start()),
                  IsNumberConstant(0))));
}


TEST_F(JSTypedLoweringTest, JSToBooleanWithPhi) {
  Node* p0 = Parameter(Type::OrderedNumber(), 0);
  Node* p1 = Parameter(Type::Boolean(), 1);
  Node* context = UndefinedConstant();
  Node* control = graph()->start();

  Reduction r = Reduce(graph()->NewNode(
      javascript()->ToBoolean(),
      graph()->NewNode(common()->Phi(kMachAnyTagged, 2), p0, p1, control),
      context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(
      r.replacement(),
      IsPhi(kMachAnyTagged,
            IsBooleanNot(IsNumberEqual(p0, IsNumberConstant(0))), p1, control));
}


TEST_F(JSTypedLoweringTest, JSToBooleanWithSelect) {
  Node* p0 = Parameter(Type::Boolean(), 0);
  Node* p1 = Parameter(Type::DetectableReceiver(), 1);
  Node* p2 = Parameter(Type::OrderedNumber(), 2);
  Node* context = UndefinedConstant();

  Reduction r = Reduce(graph()->NewNode(
      javascript()->ToBoolean(),
      graph()->NewNode(common()->Select(kMachAnyTagged, BranchHint::kTrue), p0,
                       p1, p2),
      context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsSelect(kMachAnyTagged, p0, IsTrueConstant(),
                       IsBooleanNot(IsNumberEqual(p2, IsNumberConstant(0)))));
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
  EXPECT_THAT(r.replacement(), IsToNumber(input, IsNumberConstant(0),
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
  TRACED_FORRANGE(int32_t, rhs, 0, 31) {
    Reduction r =
        Reduce(graph()->NewNode(javascript()->ShiftLeft(), lhs,
                                NumberConstant(rhs), context, effect, control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsWord32Shl(lhs, IsNumberConstant(rhs)));
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
  TRACED_FORRANGE(int32_t, rhs, 0, 31) {
    Reduction r =
        Reduce(graph()->NewNode(javascript()->ShiftRight(), lhs,
                                NumberConstant(rhs), context, effect, control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsWord32Sar(lhs, IsNumberConstant(rhs)));
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
  TRACED_FORRANGE(int32_t, rhs, 0, 31) {
    Reduction r =
        Reduce(graph()->NewNode(javascript()->ShiftRightLogical(), lhs,
                                NumberConstant(rhs), context, effect, control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsWord32Shr(lhs, IsNumberConstant(rhs)));
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

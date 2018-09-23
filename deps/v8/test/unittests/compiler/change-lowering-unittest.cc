// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-stubs.h"
#include "src/compiler/change-lowering.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::_;
using testing::AllOf;
using testing::BitEq;
using testing::Capture;
using testing::CaptureEq;

namespace v8 {
namespace internal {
namespace compiler {

class ChangeLoweringTest : public TypedGraphTest {
 public:
  ChangeLoweringTest() : simplified_(zone()) {}

  virtual MachineType WordRepresentation() const = 0;

 protected:
  bool Is32() const { return WordRepresentation() == kRepWord32; }
  bool Is64() const { return WordRepresentation() == kRepWord64; }

  Reduction Reduce(Node* node) {
    MachineOperatorBuilder machine(zone(), WordRepresentation());
    JSOperatorBuilder javascript(zone());
    JSGraph jsgraph(isolate(), graph(), common(), &javascript, &machine);
    ChangeLowering reducer(&jsgraph);
    return reducer.Reduce(node);
  }

  SimplifiedOperatorBuilder* simplified() { return &simplified_; }

  Matcher<Node*> IsAllocateHeapNumber(const Matcher<Node*>& effect_matcher,
                                      const Matcher<Node*>& control_matcher) {
    return IsCall(_, IsHeapConstant(Unique<HeapObject>::CreateImmovable(
                         AllocateHeapNumberStub(isolate()).GetCode())),
                  IsNumberConstant(BitEq(0.0)), effect_matcher,
                  control_matcher);
  }
  Matcher<Node*> IsChangeInt32ToSmi(const Matcher<Node*>& value_matcher) {
    return Is64() ? IsWord64Shl(IsChangeInt32ToInt64(value_matcher),
                                IsSmiShiftBitsConstant())
                  : IsWord32Shl(value_matcher, IsSmiShiftBitsConstant());
  }
  Matcher<Node*> IsChangeSmiToInt32(const Matcher<Node*>& value_matcher) {
    return Is64() ? IsTruncateInt64ToInt32(
                        IsWord64Sar(value_matcher, IsSmiShiftBitsConstant()))
                  : IsWord32Sar(value_matcher, IsSmiShiftBitsConstant());
  }
  Matcher<Node*> IsChangeUint32ToSmi(const Matcher<Node*>& value_matcher) {
    return Is64() ? IsWord64Shl(IsChangeUint32ToUint64(value_matcher),
                                IsSmiShiftBitsConstant())
                  : IsWord32Shl(value_matcher, IsSmiShiftBitsConstant());
  }
  Matcher<Node*> IsLoadHeapNumber(const Matcher<Node*>& value_matcher,
                                  const Matcher<Node*>& control_matcher) {
    return IsLoad(kMachFloat64, value_matcher,
                  IsIntPtrConstant(HeapNumber::kValueOffset - kHeapObjectTag),
                  graph()->start(), control_matcher);
  }
  Matcher<Node*> IsIntPtrConstant(int value) {
    return Is32() ? IsInt32Constant(value) : IsInt64Constant(value);
  }
  Matcher<Node*> IsSmiShiftBitsConstant() {
    return IsIntPtrConstant(kSmiShiftSize + kSmiTagSize);
  }
  Matcher<Node*> IsWordEqual(const Matcher<Node*>& lhs_matcher,
                             const Matcher<Node*>& rhs_matcher) {
    return Is32() ? IsWord32Equal(lhs_matcher, rhs_matcher)
                  : IsWord64Equal(lhs_matcher, rhs_matcher);
  }

 private:
  SimplifiedOperatorBuilder simplified_;
};


// -----------------------------------------------------------------------------
// Common.


class ChangeLoweringCommonTest
    : public ChangeLoweringTest,
      public ::testing::WithParamInterface<MachineType> {
 public:
  ~ChangeLoweringCommonTest() override {}

  MachineType WordRepresentation() const final { return GetParam(); }
};


TARGET_TEST_P(ChangeLoweringCommonTest, ChangeBitToBool) {
  Node* value = Parameter(Type::Boolean());
  Reduction r =
      Reduce(graph()->NewNode(simplified()->ChangeBitToBool(), value));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsSelect(kMachAnyTagged, value, IsTrueConstant(),
                                        IsFalseConstant()));
}


TARGET_TEST_P(ChangeLoweringCommonTest, ChangeBoolToBit) {
  Node* value = Parameter(Type::Number());
  Reduction r =
      Reduce(graph()->NewNode(simplified()->ChangeBoolToBit(), value));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsWordEqual(value, IsTrueConstant()));
}


TARGET_TEST_P(ChangeLoweringCommonTest, ChangeFloat64ToTagged) {
  Node* value = Parameter(Type::Number());
  Reduction r =
      Reduce(graph()->NewNode(simplified()->ChangeFloat64ToTagged(), value));
  ASSERT_TRUE(r.Changed());
  Capture<Node*> heap_number;
  EXPECT_THAT(
      r.replacement(),
      IsFinish(
          AllOf(CaptureEq(&heap_number),
                IsAllocateHeapNumber(IsValueEffect(value), graph()->start())),
          IsStore(StoreRepresentation(kMachFloat64, kNoWriteBarrier),
                  CaptureEq(&heap_number),
                  IsIntPtrConstant(HeapNumber::kValueOffset - kHeapObjectTag),
                  value, CaptureEq(&heap_number), graph()->start())));
}


TARGET_TEST_P(ChangeLoweringCommonTest, ChangeInt32ToTaggedWithSignedSmall) {
  Node* value = Parameter(Type::SignedSmall());
  Reduction r =
      Reduce(graph()->NewNode(simplified()->ChangeInt32ToTagged(), value));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsChangeInt32ToSmi(value));
}


TARGET_TEST_P(ChangeLoweringCommonTest, ChangeUint32ToTaggedWithUnsignedSmall) {
  Node* value = Parameter(Type::UnsignedSmall());
  Reduction r =
      Reduce(graph()->NewNode(simplified()->ChangeUint32ToTagged(), value));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsChangeUint32ToSmi(value));
}


TARGET_TEST_P(ChangeLoweringCommonTest, ChangeTaggedToInt32WithTaggedSigned) {
  Node* value = Parameter(Type::TaggedSigned());
  Reduction r =
      Reduce(graph()->NewNode(simplified()->ChangeTaggedToInt32(), value));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsChangeSmiToInt32(value));
}


TARGET_TEST_P(ChangeLoweringCommonTest, ChangeTaggedToInt32WithTaggedPointer) {
  Node* value = Parameter(Type::TaggedPointer());
  Reduction r =
      Reduce(graph()->NewNode(simplified()->ChangeTaggedToInt32(), value));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsChangeFloat64ToInt32(
                                   IsLoadHeapNumber(value, graph()->start())));
}


TARGET_TEST_P(ChangeLoweringCommonTest, ChangeTaggedToUint32WithTaggedSigned) {
  Node* value = Parameter(Type::TaggedSigned());
  Reduction r =
      Reduce(graph()->NewNode(simplified()->ChangeTaggedToUint32(), value));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsChangeSmiToInt32(value));
}


TARGET_TEST_P(ChangeLoweringCommonTest, ChangeTaggedToUint32WithTaggedPointer) {
  Node* value = Parameter(Type::TaggedPointer());
  Reduction r =
      Reduce(graph()->NewNode(simplified()->ChangeTaggedToUint32(), value));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsChangeFloat64ToUint32(
                                   IsLoadHeapNumber(value, graph()->start())));
}


INSTANTIATE_TEST_CASE_P(ChangeLoweringTest, ChangeLoweringCommonTest,
                        ::testing::Values(kRepWord32, kRepWord64));


// -----------------------------------------------------------------------------
// 32-bit


class ChangeLowering32Test : public ChangeLoweringTest {
 public:
  ~ChangeLowering32Test() override {}
  MachineType WordRepresentation() const final { return kRepWord32; }
};


TARGET_TEST_F(ChangeLowering32Test, ChangeInt32ToTagged) {
  Node* value = Parameter(Type::Integral32());
  Node* node = graph()->NewNode(simplified()->ChangeInt32ToTagged(), value);
  Reduction r = Reduce(node);
  ASSERT_TRUE(r.Changed());
  Capture<Node*> add, branch, heap_number, if_true;
  EXPECT_THAT(
      r.replacement(),
      IsPhi(kMachAnyTagged,
            IsFinish(AllOf(CaptureEq(&heap_number),
                           IsAllocateHeapNumber(_, CaptureEq(&if_true))),
                     IsStore(StoreRepresentation(kMachFloat64, kNoWriteBarrier),
                             CaptureEq(&heap_number),
                             IsIntPtrConstant(HeapNumber::kValueOffset -
                                              kHeapObjectTag),
                             IsChangeInt32ToFloat64(value),
                             CaptureEq(&heap_number), CaptureEq(&if_true))),
            IsProjection(0, AllOf(CaptureEq(&add),
                                  IsInt32AddWithOverflow(value, value))),
            IsMerge(AllOf(CaptureEq(&if_true), IsIfTrue(CaptureEq(&branch))),
                    IsIfFalse(AllOf(CaptureEq(&branch),
                                    IsBranch(IsProjection(1, CaptureEq(&add)),
                                             graph()->start()))))));
}


TARGET_TEST_F(ChangeLowering32Test, ChangeTaggedToFloat64) {
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);

  Node* value = Parameter(Type::Number());
  Node* node = graph()->NewNode(simplified()->ChangeTaggedToFloat64(), value);
  Reduction r = Reduce(node);
  ASSERT_TRUE(r.Changed());
  Capture<Node*> branch, if_true;
  EXPECT_THAT(
      r.replacement(),
      IsPhi(kMachFloat64, IsLoadHeapNumber(value, CaptureEq(&if_true)),
            IsChangeInt32ToFloat64(IsWord32Sar(
                value, IsInt32Constant(kSmiTagSize + kSmiShiftSize))),
            IsMerge(AllOf(CaptureEq(&if_true),
                          IsIfTrue(AllOf(
                              CaptureEq(&branch),
                              IsBranch(IsWord32And(
                                           value, IsInt32Constant(kSmiTagMask)),
                                       graph()->start())))),
                    IsIfFalse(CaptureEq(&branch)))));
}


TARGET_TEST_F(ChangeLowering32Test, ChangeTaggedToInt32) {
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);

  Node* value = Parameter(Type::Signed32());
  Node* node = graph()->NewNode(simplified()->ChangeTaggedToInt32(), value);
  Reduction r = Reduce(node);
  ASSERT_TRUE(r.Changed());
  Capture<Node*> branch, if_true;
  EXPECT_THAT(
      r.replacement(),
      IsPhi(
          kMachInt32,
          IsChangeFloat64ToInt32(IsLoadHeapNumber(value, CaptureEq(&if_true))),
          IsWord32Sar(value, IsInt32Constant(kSmiTagSize + kSmiShiftSize)),
          IsMerge(AllOf(CaptureEq(&if_true), IsIfTrue(CaptureEq(&branch))),
                  IsIfFalse(AllOf(
                      CaptureEq(&branch),
                      IsBranch(IsWord32And(value, IsInt32Constant(kSmiTagMask)),
                               graph()->start()))))));
}


TARGET_TEST_F(ChangeLowering32Test, ChangeTaggedToUint32) {
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);

  Node* value = Parameter(Type::Unsigned32());
  Node* node = graph()->NewNode(simplified()->ChangeTaggedToUint32(), value);
  Reduction r = Reduce(node);
  ASSERT_TRUE(r.Changed());
  Capture<Node*> branch, if_true;
  EXPECT_THAT(
      r.replacement(),
      IsPhi(
          kMachUint32,
          IsChangeFloat64ToUint32(IsLoadHeapNumber(value, CaptureEq(&if_true))),
          IsWord32Sar(value, IsInt32Constant(kSmiTagSize + kSmiShiftSize)),
          IsMerge(AllOf(CaptureEq(&if_true), IsIfTrue(CaptureEq(&branch))),
                  IsIfFalse(AllOf(
                      CaptureEq(&branch),
                      IsBranch(IsWord32And(value, IsInt32Constant(kSmiTagMask)),
                               graph()->start()))))));
}


TARGET_TEST_F(ChangeLowering32Test, ChangeUint32ToTagged) {
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);

  Node* value = Parameter(Type::Number());
  Node* node = graph()->NewNode(simplified()->ChangeUint32ToTagged(), value);
  Reduction r = Reduce(node);
  ASSERT_TRUE(r.Changed());
  Capture<Node*> branch, heap_number, if_false;
  EXPECT_THAT(
      r.replacement(),
      IsPhi(
          kMachAnyTagged,
          IsWord32Shl(value, IsInt32Constant(kSmiTagSize + kSmiShiftSize)),
          IsFinish(AllOf(CaptureEq(&heap_number),
                         IsAllocateHeapNumber(_, CaptureEq(&if_false))),
                   IsStore(StoreRepresentation(kMachFloat64, kNoWriteBarrier),
                           CaptureEq(&heap_number),
                           IsInt32Constant(HeapNumber::kValueOffset -
                                           kHeapObjectTag),
                           IsChangeUint32ToFloat64(value),
                           CaptureEq(&heap_number), CaptureEq(&if_false))),
          IsMerge(IsIfTrue(AllOf(
                      CaptureEq(&branch),
                      IsBranch(IsUint32LessThanOrEqual(
                                   value, IsInt32Constant(Smi::kMaxValue)),
                               graph()->start()))),
                  AllOf(CaptureEq(&if_false), IsIfFalse(CaptureEq(&branch))))));
}


// -----------------------------------------------------------------------------
// 64-bit


class ChangeLowering64Test : public ChangeLoweringTest {
 public:
  ~ChangeLowering64Test() override {}
  MachineType WordRepresentation() const final { return kRepWord64; }
};


TARGET_TEST_F(ChangeLowering64Test, ChangeInt32ToTagged) {
  Node* value = Parameter(Type::Signed32());
  Node* node = graph()->NewNode(simplified()->ChangeInt32ToTagged(), value);
  Reduction r = Reduce(node);
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsChangeInt32ToSmi(value));
}


TARGET_TEST_F(ChangeLowering64Test, ChangeTaggedToFloat64) {
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);

  Node* value = Parameter(Type::Number());
  Node* node = graph()->NewNode(simplified()->ChangeTaggedToFloat64(), value);
  Reduction r = Reduce(node);
  ASSERT_TRUE(r.Changed());
  Capture<Node*> branch, if_true;
  EXPECT_THAT(
      r.replacement(),
      IsPhi(kMachFloat64, IsLoadHeapNumber(value, CaptureEq(&if_true)),
            IsChangeInt32ToFloat64(IsTruncateInt64ToInt32(IsWord64Sar(
                value, IsInt64Constant(kSmiTagSize + kSmiShiftSize)))),
            IsMerge(AllOf(CaptureEq(&if_true),
                          IsIfTrue(AllOf(
                              CaptureEq(&branch),
                              IsBranch(IsWord64And(
                                           value, IsInt64Constant(kSmiTagMask)),
                                       graph()->start())))),
                    IsIfFalse(CaptureEq(&branch)))));
}


TARGET_TEST_F(ChangeLowering64Test, ChangeTaggedToInt32) {
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);

  Node* value = Parameter(Type::Signed32());
  Node* node = graph()->NewNode(simplified()->ChangeTaggedToInt32(), value);
  Reduction r = Reduce(node);
  ASSERT_TRUE(r.Changed());
  Capture<Node*> branch, if_true;
  EXPECT_THAT(
      r.replacement(),
      IsPhi(
          kMachInt32,
          IsChangeFloat64ToInt32(IsLoadHeapNumber(value, CaptureEq(&if_true))),
          IsTruncateInt64ToInt32(
              IsWord64Sar(value, IsInt64Constant(kSmiTagSize + kSmiShiftSize))),
          IsMerge(AllOf(CaptureEq(&if_true), IsIfTrue(CaptureEq(&branch))),
                  IsIfFalse(AllOf(
                      CaptureEq(&branch),
                      IsBranch(IsWord64And(value, IsInt64Constant(kSmiTagMask)),
                               graph()->start()))))));
}


TARGET_TEST_F(ChangeLowering64Test, ChangeTaggedToUint32) {
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);

  Node* value = Parameter(Type::Unsigned32());
  Node* node = graph()->NewNode(simplified()->ChangeTaggedToUint32(), value);
  Reduction r = Reduce(node);
  ASSERT_TRUE(r.Changed());
  Capture<Node*> branch, if_true;
  EXPECT_THAT(
      r.replacement(),
      IsPhi(
          kMachUint32,
          IsChangeFloat64ToUint32(IsLoadHeapNumber(value, CaptureEq(&if_true))),
          IsTruncateInt64ToInt32(
              IsWord64Sar(value, IsInt64Constant(kSmiTagSize + kSmiShiftSize))),
          IsMerge(AllOf(CaptureEq(&if_true), IsIfTrue(CaptureEq(&branch))),
                  IsIfFalse(AllOf(
                      CaptureEq(&branch),
                      IsBranch(IsWord64And(value, IsInt64Constant(kSmiTagMask)),
                               graph()->start()))))));
}


TARGET_TEST_F(ChangeLowering64Test, ChangeUint32ToTagged) {
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);

  Node* value = Parameter(Type::Number());
  Node* node = graph()->NewNode(simplified()->ChangeUint32ToTagged(), value);
  Reduction r = Reduce(node);
  ASSERT_TRUE(r.Changed());
  Capture<Node*> branch, heap_number, if_false;
  EXPECT_THAT(
      r.replacement(),
      IsPhi(
          kMachAnyTagged,
          IsWord64Shl(IsChangeUint32ToUint64(value),
                      IsInt64Constant(kSmiTagSize + kSmiShiftSize)),
          IsFinish(AllOf(CaptureEq(&heap_number),
                         IsAllocateHeapNumber(_, CaptureEq(&if_false))),
                   IsStore(StoreRepresentation(kMachFloat64, kNoWriteBarrier),
                           CaptureEq(&heap_number),
                           IsInt64Constant(HeapNumber::kValueOffset -
                                           kHeapObjectTag),
                           IsChangeUint32ToFloat64(value),
                           CaptureEq(&heap_number), CaptureEq(&if_false))),
          IsMerge(IsIfTrue(AllOf(
                      CaptureEq(&branch),
                      IsBranch(IsUint32LessThanOrEqual(
                                   value, IsInt32Constant(Smi::kMaxValue)),
                               graph()->start()))),
                  AllOf(CaptureEq(&if_false), IsIfFalse(CaptureEq(&branch))))));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

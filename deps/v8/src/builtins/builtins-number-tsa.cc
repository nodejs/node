// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-descriptors.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/number-builtins-reducer-inl.h"
#include "src/codegen/turboshaft-builtins-assembler-inl.h"
#include "src/common/globals.h"
#include "src/execution/protectors.h"

namespace v8::internal {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

using namespace compiler::turboshaft;  // NOLINT(build/namespaces)

class NumberBuiltinsAssemblerTS
    : public TurboshaftBuiltinsAssembler<NumberBuiltinsReducer,
                                         FeedbackCollectorReducer> {
 public:
  using Base = TurboshaftBuiltinsAssembler<NumberBuiltinsReducer,
                                           FeedbackCollectorReducer>;

  using Base::Asm;
  using Base::Base;

  V<Float64> SmiToFloat64(V<Smi> smi) {
    return ChangeInt32ToFloat64(UntagSmi(smi));
  }

  V<Word32> IsAdditiveSafeIntegerFeedbackEnabled() {
    if (Is64()) {
      return LoadRuntimeFlag(
          ExternalReference::additive_safe_int_feedback_flag());
    } else {
      return __ Word32Constant(0);
    }
  }

  V<Word32> LoadRuntimeFlag(ExternalReference address_of_flag) {
    V<Word32> flag_value = LoadOffHeap(ExternalConstant(address_of_flag),
                                       MemoryRepresentation::Uint8());
    return Word32Equal(Word32Equal(flag_value, 0), 0);
  }

  V<Float64> LoadHeapNumberValue(V<HeapNumber> heap_number) {
    return __ LoadField(heap_number, AccessBuilderTS::ForHeapNumberValue());
  }

  V<Smi> TrySmiAdd(V<Smi> lhs, V<Smi> rhs, Label<>& if_overflow,
                   bool overflow_unlikely = false) {
    static_assert(OverflowCheckedBinopOp::kValueIndex == 0);
    static_assert(OverflowCheckedBinopOp::kOverflowIndex == 1);
    if (SmiValuesAre32Bits()) {
      V<Tuple<WordPtr, Word32>> result =
          IntPtrAddCheckOverflow(BitcastTaggedToWordPtrForTagAndSmiBits(lhs),
                                 BitcastTaggedToWordPtrForTagAndSmiBits(rhs));
      GOTO_IF(ConditionWithHint(Projection<1>(result), overflow_unlikely
                                                           ? BranchHint::kFalse
                                                           : BranchHint::kNone),
              if_overflow);
      return BitcastWordPtrToSmi(Projection<0>(result));
    } else {
      DCHECK(SmiValuesAre31Bits());
      V<Tuple<Word32, Word32>> result = Int32AddCheckOverflow(
          TruncateWordPtrToWord32(BitcastTaggedToWordPtrForTagAndSmiBits(lhs)),
          TruncateWordPtrToWord32(BitcastTaggedToWordPtrForTagAndSmiBits(rhs)));
      GOTO_IF(ConditionWithHint(Projection<1>(result), overflow_unlikely
                                                           ? BranchHint::kFalse
                                                           : BranchHint::kNone),
              if_overflow);
      return BitcastWordPtrToSmi(ChangeInt32ToIntPtr(Projection<0>(result)));
    }
  }

  // TODO(nicohartmann): It might be useful to have this is in a more general
  // location.
  V<Word64> TryFloat64ToAdditiveSafeInteger(V<Float64> value,
                                            Label<>& if_fail) {
    DCHECK(Is64());

    Label<> if_int(this);

    V<Word64> value_i64 = TruncateFloat64ToInt64OverflowToMin(value);
    V<Float64> value_roundtrip = ChangeInt64ToFloat64(value_i64);

    GOTO_IF_NOT(Float64Equal(value, value_roundtrip), if_fail);
    GOTO_IF_NOT(Word64Equal(value_i64, 0), if_int);

    // if (value == -0.0)
    GOTO_IF(Word64Equal(BitcastFloat64ToWord64(value), 0), if_int);
    GOTO(if_fail);

    BIND(if_int);
    {
      // Check if AdditiveSafeInteger: (value - kMinAdditiveSafeInteger) >> 53
      // == 0
      V<Word64> shifted_value =
          Word64ShiftRightLogical(Word64Sub(value_i64, kMinAdditiveSafeInteger),
                                  kAdditiveSafeIntegerBitLength);
      GOTO_IF_NOT(Word64Equal(shifted_value, 0), if_fail);
      return value_i64;
    }
  }

  V<Word32> IsAdditiveSafeInteger(V<Float64> value) {
    if (!Is64()) return Word32Constant(0);
    Label<Word32> done(this);
    Label<> not_additive_safe(this);
    TryFloat64ToAdditiveSafeInteger(value, not_additive_safe);
    GOTO(done, 1);

    BIND(not_additive_safe);
    GOTO(done, 0);

    BIND(done, result);
    return result;
  }

  V<Word32> IsAdditiveSafeIntegerFeedback(V<Float64> value) {
    Label<Word32> done(this);
    IF_NOT (IsAdditiveSafeIntegerFeedbackEnabled()) {
      GOTO(done, 0);
    }
    GOTO(done, IsAdditiveSafeInteger(value));

    BIND(done, result);
    return result;
  }

  V<Word32> LoadInstanceType(V<Object> object) {
    V<Map> map = LoadMapField(object);
    return LoadInstanceTypeField(map);
  }

  V<Word32> IsStringInstanceType(V<Word32> instance_type) {
    static_assert(INTERNALIZED_TWO_BYTE_STRING_TYPE == FIRST_TYPE);
    return Uint32LessThan(instance_type, FIRST_NONSTRING_TYPE);
  }

  V<Word32> IsBigIntInstanceType(V<Word32> instance_type) {
    return InstanceTypeEqual(instance_type, BIGINT_TYPE);
  }

  V<Word32> IsStringWrapper(V<HeapObject> object) {
    V<Map> map = LoadMapField(object);
    V<Word32> kind = LoadElementsKind(map);
    return Word32BitwiseOr(Word32Equal(kind, FAST_STRING_WRAPPER_ELEMENTS),
                           Word32Equal(kind, SLOW_STRING_WRAPPER_ELEMENTS));
  }

  void GotoIfLargeBigInt(V<BigInt> bigint, Label<>& if_large) {
    DCHECK(Is64());
    // Small BigInts are BigInts in the range [-2^63 + 1, 2^63 - 1] so that
    // they can fit in 64-bit registers. Excluding -2^63 from the range makes
    // the check simpler and faster. The other BigInts are seen as "large".
    // TODO(panq): We might need to reevaluate of the range of small BigInts.
    Label<> if_not_large(this);
    V<Word32> bitfield =
        LoadField<Word32>(bigint, AccessBuilderTS::ForBigIntBitfield());
    V<Word32> length = DecodeWord32<BigIntBase::LengthBits>(bitfield);
    GOTO_IF(Word32Equal(length, 0), if_not_large);
    GOTO_IF_NOT(Word32Equal(length, 1), if_large);
    V<WordPtr> digit = LoadField<WordPtr>(
        bigint, AccessBuilderTS::ForBigIntLeastSignificantDigit64());
    V<WordPtr> msb = WordPtrBitwiseAnd(
        digit, static_cast<uintptr_t>(1ULL << (sizeof(uintptr_t) * 8 - 1)));
    GOTO_IF(WordPtrEqual(msb, 0), if_not_large);
    GOTO(if_large);

    BIND(if_not_large);
  }

  void ThrowRangeError(V<Context> context, MessageTemplate message) {
    V<Smi> template_index =
        SmiConstant(Smi::FromInt(static_cast<int>(message)));
    CallRuntime_ThrowRangeError(isolate(), context, template_index);
  }

  V<Object> AddWithFeedback(V<Context> context, V<Object> lhs, V<Object> rhs,
                            UpdateFeedbackMode update_feedback_mode,
                            bool rhs_known_smi) {
    Label<Object> done(this);
    Label<Float64, Float64> do_fadd(this);
    Label<> check_rhsisoddball(this);
    Label<> call_with_any_feedback(this);
    Label<> call_add_stub(this);
    Label<> bigint(this);
    Label<> bigint64(this);

    IF (ObjectIsSmi(lhs)) {
      Comment("lhs is Smi");
      V<Smi> lhs_smi = V<Smi>::Cast(lhs);

      if (!rhs_known_smi) {
        IF_NOT (ObjectIsSmi(rhs)) {
          // Check if the {rhs} is a HeapNumber.
          V<HeapObject> rhs_heap_object = V<HeapObject>::Cast(rhs);
          GOTO_IF_NOT(HeapObjectIsNumber(rhs_heap_object), check_rhsisoddball);

          V<Float64> rhs_f64 =
              LoadHeapNumberValue(V<HeapNumber>::Cast(rhs_heap_object));
          IF (IsAdditiveSafeIntegerFeedback(rhs_f64)) {
            CombineFeedback(BinaryOperationFeedback::kAdditiveSafeInteger);
          } ELSE {
            CombineFeedback(BinaryOperationFeedback::kNumber);
          }

          GOTO(do_fadd, SmiToFloat64(lhs_smi), rhs_f64);
        }
      }

      // rhs is smi
      Comment("perform smi operation");
      Label<> if_overflow(this);
      // If rhs is known to be an Smi we want to fast path Smi operation. This
      // is for AddSmi operation. For the normal Add operation, we want to fast
      // path both Smi and Number operations, so this path should not be marked
      // as Deferred.
      V<Smi> rhs_smi = V<Smi>::Cast(rhs);
      V<Smi> smi_result = TrySmiAdd(lhs_smi, rhs_smi, if_overflow,
                                    /*overflow unlikely*/ rhs_known_smi);
      // Not overflowed.
      CombineFeedback(BinaryOperationFeedback::kSignedSmall);
      GOTO(done, smi_result);

      BIND(if_overflow);
      {
        IF (IsAdditiveSafeIntegerFeedbackEnabled()) {
          CombineFeedback(BinaryOperationFeedback::kAdditiveSafeInteger);
        } ELSE {
          CombineFeedback(BinaryOperationFeedback::kNumber);
        }
        GOTO(do_fadd, SmiToFloat64(lhs_smi), SmiToFloat64(rhs_smi));
      }
    } ELSE IF (HeapObjectIsNumber(V<HeapObject>::Cast(lhs))) {
      V<HeapNumber> lhs_heap_number = V<HeapNumber>::Cast(lhs);
      if (!rhs_known_smi) {
        IF_NOT (ObjectIsSmi(rhs)) {
          // Check if the {rhs} is a HeapNumber.
          V<HeapObject> rhs_heap_object = V<HeapObject>::Cast(rhs);
          GOTO_IF_NOT(HeapObjectIsNumber(rhs_heap_object), check_rhsisoddball);
          V<Float64> lhs_f64 = LoadHeapNumberValue(lhs_heap_number);
          V<Float64> rhs_f64 = LoadHeapNumberValue(V<HeapNumber>::Cast(rhs));

          CombineFeedback(BinaryOperationFeedback::kNumber);
          GOTO_IF_NOT(IsAdditiveSafeIntegerFeedback(lhs_f64), do_fadd, lhs_f64,
                      rhs_f64);
          GOTO_IF_NOT(IsAdditiveSafeIntegerFeedback(rhs_f64), do_fadd, lhs_f64,
                      rhs_f64);

          OverwriteFeedback(BinaryOperationFeedback::kAdditiveSafeInteger);
          GOTO(do_fadd, lhs_f64, rhs_f64);
        }
      }

      IF (IsAdditiveSafeIntegerFeedbackEnabled()) {
        CombineFeedback(BinaryOperationFeedback::kAdditiveSafeInteger);
      } ELSE {
        CombineFeedback(BinaryOperationFeedback::kNumber);
      }
      GOTO(do_fadd, LoadHeapNumberValue(lhs_heap_number),
           SmiToFloat64(V<Smi>::Cast(rhs)));
    } ELSE IF (InstanceTypeEqual(LoadInstanceType(lhs), ODDBALL_TYPE)) {
      // No checks on rhs are done yet. We just know lhs is an Oddball.
      IF (ObjectIsNumber(rhs)) {
        CombineFeedback(BinaryOperationFeedback::kNumberOrOddball);
        GOTO(call_add_stub);
      }
      GOTO(check_rhsisoddball);
    } ELSE {
      // Check if the {rhs} is a smi, and exit the string and bigint check early
      // if it is.
      GOTO_IF(ObjectIsSmi(rhs), call_with_any_feedback);
      V<Word32> lhs_instance_type = LoadInstanceType(lhs);
      V<Word32> rhs_instance_type = LoadInstanceType(rhs);
      V<HeapObject> rhs_heap_object = V<HeapObject>::Cast(rhs);

      IF (IsStringInstanceType(lhs_instance_type)) {
        IF (IsStringInstanceType(rhs_instance_type)) {
          // Fast path where both {lhs} and {rhs} are strings. Since {lhs} is a
          // string we no longer need an Oddball check.
          CombineFeedback(BinaryOperationFeedback::kString);
          V<Object> result = CallBuiltin_StringAdd_CheckNone(
              isolate(), context, V<String>::Cast(lhs), V<String>::Cast(rhs));
          GOTO(done, result);
        } ELSE IF (IsStringWrapper(rhs_heap_object)) {
          // lhs is a string and rhs is a string wrapper.
          V<PropertyCell> to_primitive_protector =
              StringWrapperToPrimitiveProtectorConstant();
          GOTO_IF(TaggedEqual(
                      LoadField(to_primitive_protector,
                                AccessBuilderTS::ForPropertyCellValue()),
                      SmiConstant(Smi::FromInt(Protectors::kProtectorInvalid))),
                  call_with_any_feedback);

          CombineFeedback(BinaryOperationFeedback::kStringOrStringWrapper);
          V<String> rhs_string = V<String>::Cast(LoadField(
              rhs_heap_object, AccessBuilderTS::ForJSPrimitiveWrapperValue()));
          V<Object> result = CallBuiltin_StringAdd_CheckNone(
              isolate(), context, V<String>::Cast(lhs), rhs_string);
          GOTO(done, result);
        } ELSE {
          GOTO(call_with_any_feedback);
        }
      } ELSE IF (IsBigIntInstanceType(lhs_instance_type)) {
        GOTO_IF_NOT(IsBigIntInstanceType(rhs_instance_type),
                    call_with_any_feedback);
        if (Is64()) {
          GotoIfLargeBigInt(V<BigInt>::Cast(lhs), bigint);
          GotoIfLargeBigInt(V<BigInt>::Cast(rhs), bigint);
          GOTO(bigint64);
        } else {
          GOTO(bigint);
        }
      } ELSE {
        GOTO(call_with_any_feedback);
      }
    }

    BIND(do_fadd, lhs_f64, rhs_f64);
    {
      V<Float64> result_f64 = Float64Add(lhs_f64, rhs_f64);
      V<Object> result = AllocateHeapNumberWithValue(result_f64, factory());

      IF (IsAdditiveSafeIntegerFeedbackEnabled()) {
        IF (FeedbackIs(BinaryOperationFeedback::kAdditiveSafeInteger)) {
          IF_NOT (IsAdditiveSafeIntegerFeedback(result_f64)) {
            CombineFeedback(BinaryOperationFeedback::kNumber);
          }
        }
      }

      GOTO(done, result);
    }

    BIND(check_rhsisoddball);
    {
      // Check if rhs is an oddball. At this point we know lhs is either a
      // Smi or number or oddball and rhs is not a number or Smi.
      V<Word32> rhs_instance_type = LoadInstanceType(rhs);
      IF (InstanceTypeEqual(rhs_instance_type, ODDBALL_TYPE)) {
        CombineFeedback(BinaryOperationFeedback::kNumberOrOddball);
        GOTO(call_add_stub);
      }
      GOTO(call_with_any_feedback);
    }

    if (Is64()) {
      BIND(bigint64);
      {
        // Both {lhs} and {rhs} are of BigInt type and can fit in 64-bit
        // registers.
        V<Word64> lhs_raw = TruncateBigIntToWord64(V<BigInt>::Cast(lhs));
        V<Word64> rhs_raw = TruncateBigIntToWord64(V<BigInt>::Cast(rhs));
        V<Tuple<Word64, Word32>> results =
            Int64AddCheckOverflow(lhs_raw, rhs_raw);
        GOTO_IF(Projection<1>(results), bigint);

        CombineFeedback(BinaryOperationFeedback::kBigInt64);
        GOTO(done, ConvertInt64ToBigInt(Projection<0>(results)));
      }
    }

    BIND(bigint);
    {
      // Both {lhs} and {rhs} are of BigInt type.
      CombineFeedbackOnException(BinaryOperationFeedback::kAny);
      V<BigInt> result = CallBuiltin_BigIntAdd(
          isolate(), context, V<BigInt>::Cast(lhs), V<BigInt>::Cast(rhs));
      GOTO(done, result);
    }

    BIND(call_with_any_feedback);
    {
      CombineFeedback(BinaryOperationFeedback::kAny);
      GOTO(call_add_stub);
    }

    BIND(call_add_stub);
    {
      V<Object> result =
          CallBuiltin_Add(isolate(), FrameStateForCall::NoFrameState(this),
                          context, lhs, rhs, compiler::LazyDeoptOnThrow::kNo);
      GOTO(done, result);
    }

    BIND(done, result);
    return result;
  }
};

#ifdef V8_ENABLE_EXPERIMENTAL_TSA_BUILTINS

TS_BUILTIN(Add_WithFeedback, NumberBuiltinsAssemblerTS) {
  // TODO(nicohartmann): It would be great to deduce the parameter type from the
  // Descriptor directly.
  V<Object> lhs = Parameter<Object>(Descriptor::kLeft);
  V<Object> rhs = Parameter<Object>(Descriptor::kRight);
  V<Context> context = Parameter<Context>(Descriptor::kContext);
  V<FeedbackVector> feedback_vector =
      Parameter<FeedbackVector>(Descriptor::kFeedbackVector);
  V<WordPtr> slot = Parameter<WordPtr>(Descriptor::kSlot);

  SetFeedbackSlot(slot);
  SetFeedbackVector(feedback_vector);

  V<Object> result = AddWithFeedback(
      context, lhs, rhs, UpdateFeedbackMode::kGuaranteedFeedback, false);
  Return(result);
}

TS_BUILTIN(BitwiseNot_WithFeedback, NumberBuiltinsAssemblerTS) {
  // TODO(nicohartmann): It would be great to deduce the parameter type from the
  // Descriptor directly.
  V<Object> value = Parameter<Object>(Descriptor::kValue);
  V<Context> context = Parameter<Context>(Descriptor::kContext);
  V<FeedbackVector> feedback_vector =
      Parameter<FeedbackVector>(Descriptor::kFeedbackVector);
  V<WordPtr> slot = Parameter<WordPtr>(Descriptor::kSlot);

  SetFeedbackSlot(slot);
  SetFeedbackVector(feedback_vector);

  V<Object> result = BitwiseNot(context, value);
  Return(result);
}

#endif  // V8_ENABLE_EXPERIMENTAL_TSA_BUILTINS

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal

// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_SIMPLIFIED_OPTIMIZATION_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_SIMPLIFIED_OPTIMIZATION_REDUCER_H_

#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/opmasks.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/numbers/conversions.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

template <class Next>
class SimplifiedOptimizationReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE(SimplifiedOptimization)

  // TODO(dmercadier): support more optimization of Untagged->Tagged->Untagged
  // and Tagged->Untagged->Tagged.

  V<Untagged> REDUCE(TruncateJSPrimitiveToUntagged)(
      V<JSPrimitive> input, TruncateJSPrimitiveToUntaggedOp::UntaggedKind kind,
      TruncateJSPrimitiveToUntaggedOp::InputAssumptions input_assumptions) {
    LABEL_BLOCK(no_change) {
      return Next::ReduceTruncateJSPrimitiveToUntagged(input, kind,
                                                       input_assumptions);
    }
    if (ShouldSkipOptimizationStep()) goto no_change;

    if (const ConvertUntaggedToJSPrimitiveOp* convert =
            matcher_.TryCast<ConvertUntaggedToJSPrimitiveOp>(input)) {
      switch (convert->kind) {
        case ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind::kHeapNumber:
        case ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind::
            kHeapNumberOrUndefined:
        case ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind::kNumber: {
          if (input_assumptions == TruncateJSPrimitiveToUntaggedOp::
                                       InputAssumptions::kNumberOrOddball) {
            // There is an unnecessary convertion to JSPrimitive. We'll bypass
            // it and simply to an untagged conversion/truncation.
            switch (multi(convert->input_rep, kind)) {
              case multi(RegisterRepresentation::Float64(),
                         TruncateJSPrimitiveToUntaggedOp::UntaggedKind::kInt32):
                return __ JSTruncateFloat64ToWord32(convert->input<Float64>());
              default:
                // TODO(dmercadier): support more cases.
                break;
            }
          }
          if (convert->input_interpretation ==
                  ConvertUntaggedToJSPrimitiveOp::InputInterpretation::
                      kSigned &&
              convert->input_rep == RegisterRepresentation::Word32() &&
              kind == TruncateJSPrimitiveToUntaggedOp::UntaggedKind::kInt32) {
            // Signed conversion from Word32 to Number and back to Word32 can be
            // completely replaced with the original input.
            return convert->input<Word32>();
          }
          // When converting to a single bit, signedness doesn't matter.
          if (convert->input_rep == RegisterRepresentation::Word32() &&
              kind == TruncateJSPrimitiveToUntaggedOp::UntaggedKind::kBit) {
            return __ Word32Select(__ Word32Equal(convert->input<Word32>(), 0),
                                   0, 1);
          }
          break;
        }

        case ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind::kSmi: {
          if (convert->input_rep == RegisterRepresentation::Word32() &&
              kind == TruncateJSPrimitiveToUntaggedOp::UntaggedKind::kInt32) {
            return convert->input<Word32>();
          }
          break;
        }

        case ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind::kBigInt:
        case ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind::kBoolean:
        case ConvertUntaggedToJSPrimitiveOp::JSPrimitiveKind::kString:
          break;
      }
    }

    Handle<HeapObject> cst;
    if (kind == TruncateJSPrimitiveToUntaggedOp::UntaggedKind::kInt32 &&
        matcher_.MatchHeapConstant(input, &cst)) {
      if (Tagged<OptimizedOut> optimized_out; TryCast(*cst, &optimized_out)) {
        // OptimizedOut happens in dead code, so this truncation doesn't matter.
        return __ Word32Constant(0);
      } else if (Tagged<TheHole> hole; TryCast(*cst, &hole)) {
        // Holes are allowed for InputAssumptions::kNumberOrOddballOrHole.
        // Hole->Undefined->NaN->0.
        return __ Word32Constant(0);
      } else if (Tagged<HeapNumber> heap_number; TryCast(*cst, &heap_number)) {
        return __ Word32Constant(DoubleToInt32(heap_number->value()));
      } else if (Tagged<Oddball> oddball; TryCast(*cst, &oddball)) {
        return __ Word32Constant(DoubleToInt32(oddball->to_number_raw()));
      }
    }

    goto no_change;
  }

  V<Untagged> REDUCE(ChangeOrDeopt)(V<Untagged> input,
                                    V<FrameState> frame_state,
                                    ChangeOrDeoptOp::Kind kind,
                                    CheckForMinusZeroMode minus_zero_mode,
                                    const FeedbackSource& feedback) {
    LABEL_BLOCK(no_change) {
      return Next::ReduceChangeOrDeopt(input, frame_state, kind,
                                       minus_zero_mode, feedback);
    }
    if (ShouldSkipOptimizationStep()) goto no_change;

    if (kind == ChangeOrDeoptOp::Kind::kFloat64ToInt32) {
      double cst;
      if (matcher_.MatchFloat64Constant(input, &cst) && IsInt32Double(cst)) {
        return __ Word32Constant(static_cast<int32_t>(cst));
      }
    }

    goto no_change;
  }

  OpIndex REDUCE(ObjectIs)(V<Object> input, ObjectIsOp::Kind kind,
                           ObjectIsOp::InputAssumptions input_assumptions) {
    LABEL_BLOCK(no_change) {
      return Next::ReduceObjectIs(input, kind, input_assumptions);
    }
    if (ShouldSkipOptimizationStep()) goto no_change;

    switch (kind) {
      case ObjectIsOp::Kind::kSmi:
        switch (__ DecideObjectIsSmi(input)) {
          case IsSmiDecision::kTrue:
            return __ Word32Constant(1);
          case IsSmiDecision::kFalse:
            return __ Word32Constant(0);
          case IsSmiDecision::kUnknown:
            goto no_change;
        }
      case ObjectIsOp::Kind::kArrayBufferView:
      case ObjectIsOp::Kind::kBigInt:
      case ObjectIsOp::Kind::kBigInt64:
      case ObjectIsOp::Kind::kCallable:
      case ObjectIsOp::Kind::kConstructor:
      case ObjectIsOp::Kind::kDetectableCallable:
      case ObjectIsOp::Kind::kInternalizedString:
      case ObjectIsOp::Kind::kReceiver:
      case ObjectIsOp::Kind::kReceiverOrNullOrUndefined:
      case ObjectIsOp::Kind::kString:
      case ObjectIsOp::Kind::kStringOrStringWrapper:
      case ObjectIsOp::Kind::kStringOrOddball:
      case ObjectIsOp::Kind::kSymbol:
      case ObjectIsOp::Kind::kUndetectable:
      case ObjectIsOp::Kind::kNonCallable:
        switch (__ DecideObjectIsSmi(input)) {
          case IsSmiDecision::kTrue:
            return __ Word32Constant(0);
          case IsSmiDecision::kFalse:
          case IsSmiDecision::kUnknown:
            goto no_change;
        }

      case ObjectIsOp::Kind::kNumber:
      case ObjectIsOp::Kind::kNumberOrUndefined:
      case ObjectIsOp::Kind::kNumberFitsInt32:
      case ObjectIsOp::Kind::kNumberOrBigInt:
        switch (__ DecideObjectIsSmi(input)) {
          case IsSmiDecision::kTrue:
            return __ Word32Constant(1);
          case IsSmiDecision::kFalse:
          case IsSmiDecision::kUnknown:
            goto no_change;
        }

        break;
    }

    UNREACHABLE();
  }

  V<Word> REDUCE(WordBinopDeoptOnOverflow)(
      V<Word> left, V<Word> right, V<FrameState> frame_state,
      WordBinopDeoptOnOverflowOp::Kind kind, WordRepresentation rep,
      FeedbackSource feedback, CheckForMinusZeroMode mode) {
    LABEL_BLOCK(no_change) {
      return Next::ReduceWordBinopDeoptOnOverflow(left, right, frame_state,
                                                  kind, rep, feedback, mode);
    }
    if (ShouldSkipOptimizationStep()) goto no_change;

    if (WordBinopDeoptOnOverflowOp::IsCommutative(kind) &&
        matcher_.Is<ConstantOp>(left) && !matcher_.Is<ConstantOp>(right)) {
      return __ WordBinopDeoptOnOverflow(right, left, frame_state, kind, rep,
                                         feedback, mode);
    }

    // Trying to remove unnecessary checks for minus 0 in multiplications. Note
    // that the only way to get a -0 here is if we have a negative number
    // multiplied by 0.
    if (mode == CheckForMinusZeroMode::kCheckForMinusZero &&
        kind == WordBinopDeoptOnOverflowOp::Kind::kSignedMul) {
      int32_t cst;
      if ((matcher_.MatchIntegralWord32Constant(left, &cst) && cst > 0) ||
          (matcher_.MatchIntegralWord32Constant(right, &cst) && cst > 0)) {
        // At least one side is strictly positive ==> this multiplication cannot
        // produce -0.
        return __ WordBinopDeoptOnOverflow(
            left, right, frame_state, kind, rep, feedback,
            CheckForMinusZeroMode::kDontCheckForMinusZero);
      }
    }

    // TODO(dmercadier): consider also supporting Word64 rep for this
    // optimization.
    int32_t right_cst;
    if (rep == WordRepresentation::Word32() &&
        kind == WordBinopDeoptOnOverflowOp::Kind::kSignedAdd &&
        matcher_.MatchIntegralWord32Constant(right, &right_cst)) {
      // (x + a) + b => x + (a + b) where a and b are constants and have the
      // same sign.
      if (const WordBinopDeoptOnOverflowOp* left_input =
              matcher_.TryCast<Opmask::kWordSignedAddDeoptOnOverflow>(left)) {
        int32_t left_cst;
        if (matcher_.MatchIntegralWord32Constant(left_input->right(),
                                                 &left_cst) &&
            (right_cst >= 0) == (left_cst >= 0)) {
          int32_t left_plus_right;
          bool overflow = base::bits::SignedAddOverflow32(left_cst, right_cst,
                                                          &left_plus_right);
          if (!overflow) {
            return __ Word32SignedAddDeoptOnOverflow(left_input->left<Word32>(),
                                                     left_plus_right,
                                                     frame_state, feedback);
          }
        }
      }
    }

    goto no_change;
  }

 private:
  const OperationMatcher& matcher_ = __ matcher();
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_SIMPLIFIED_OPTIMIZATION_REDUCER_H_

// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/arguments-inl.h"
#include "src/objects/bigint.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_BigIntCompareToBigInt) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(3, args.length());
  int mode = args.smi_value_at(0);
  Handle<BigInt> lhs = args.at<BigInt>(1);
  Handle<BigInt> rhs = args.at<BigInt>(2);
  bool result = ComparisonResultToBool(static_cast<Operation>(mode),
                                       BigInt::CompareToBigInt(lhs, rhs));
  return *isolate->factory()->ToBoolean(result);
}

RUNTIME_FUNCTION(Runtime_BigIntCompareToNumber) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(3, args.length());
  int mode = args.smi_value_at(0);
  Handle<BigInt> lhs = args.at<BigInt>(1);
  Handle<Object> rhs = args.at(2);
  bool result = ComparisonResultToBool(static_cast<Operation>(mode),
                                       BigInt::CompareToNumber(lhs, rhs));
  return *isolate->factory()->ToBoolean(result);
}

RUNTIME_FUNCTION(Runtime_BigIntCompareToString) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  int mode = args.smi_value_at(0);
  Handle<BigInt> lhs = args.at<BigInt>(1);
  Handle<String> rhs = args.at<String>(2);
  Maybe<ComparisonResult> maybe_result =
      BigInt::CompareToString(isolate, lhs, rhs);
  MAYBE_RETURN(maybe_result, ReadOnlyRoots(isolate).exception());
  bool result = ComparisonResultToBool(static_cast<Operation>(mode),
                                       maybe_result.FromJust());
  return *isolate->factory()->ToBoolean(result);
}

RUNTIME_FUNCTION(Runtime_BigIntEqualToBigInt) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(2, args.length());
  Handle<BigInt> lhs = args.at<BigInt>(0);
  Handle<BigInt> rhs = args.at<BigInt>(1);
  bool result = BigInt::EqualToBigInt(*lhs, *rhs);
  return *isolate->factory()->ToBoolean(result);
}

RUNTIME_FUNCTION(Runtime_BigIntEqualToNumber) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(2, args.length());
  Handle<BigInt> lhs = args.at<BigInt>(0);
  Handle<Object> rhs = args.at(1);
  bool result = BigInt::EqualToNumber(lhs, rhs);
  return *isolate->factory()->ToBoolean(result);
}

RUNTIME_FUNCTION(Runtime_BigIntEqualToString) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<BigInt> lhs = args.at<BigInt>(0);
  Handle<String> rhs = args.at<String>(1);
  Maybe<bool> maybe_result = BigInt::EqualToString(isolate, lhs, rhs);
  MAYBE_RETURN(maybe_result, ReadOnlyRoots(isolate).exception());
  return *isolate->factory()->ToBoolean(maybe_result.FromJust());
}

RUNTIME_FUNCTION(Runtime_BigIntToBoolean) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  Handle<BigInt> bigint = args.at<BigInt>(0);
  return *isolate->factory()->ToBoolean(bigint->ToBoolean());
}

RUNTIME_FUNCTION(Runtime_BigIntToNumber) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<BigInt> x = args.at<BigInt>(0);
  return *BigInt::ToNumber(isolate, x);
}

RUNTIME_FUNCTION(Runtime_ToBigInt) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<Object> x = args.at(0);
  RETURN_RESULT_OR_FAILURE(isolate, BigInt::FromObject(isolate, x));
}

RUNTIME_FUNCTION(Runtime_ToBigIntConvertNumber) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<Object> x = args.at(0);

  if (IsJSReceiver(*x)) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, x,
        JSReceiver::ToPrimitive(isolate, Handle<JSReceiver>::cast(x),
                                ToPrimitiveHint::kNumber));
  }

  if (IsNumber(*x)) {
    RETURN_RESULT_OR_FAILURE(isolate, BigInt::FromNumber(isolate, x));
  } else {
    RETURN_RESULT_OR_FAILURE(isolate, BigInt::FromObject(isolate, x));
  }
}

RUNTIME_FUNCTION(Runtime_BigIntBinaryOp) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  Handle<Object> left_obj = args.at(0);
  Handle<Object> right_obj = args.at(1);
  int opcode = args.smi_value_at(2);
  Operation op = static_cast<Operation>(opcode);

  if (!IsBigInt(*left_obj) || !IsBigInt(*right_obj)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kBigIntMixedTypes));
  }
  Handle<BigInt> left(Handle<BigInt>::cast(left_obj));
  Handle<BigInt> right(Handle<BigInt>::cast(right_obj));
  MaybeHandle<BigInt> result;
  switch (op) {
    case Operation::kAdd:
      result = BigInt::Add(isolate, left, right);
      break;
    case Operation::kSubtract:
      result = BigInt::Subtract(isolate, left, right);
      break;
    case Operation::kMultiply:
      result = BigInt::Multiply(isolate, left, right);
      break;
    case Operation::kDivide:
      result = BigInt::Divide(isolate, left, right);
      break;
    case Operation::kModulus:
      result = BigInt::Remainder(isolate, left, right);
      break;
    case Operation::kExponentiate:
      result = BigInt::Exponentiate(isolate, left, right);
      break;
    case Operation::kBitwiseAnd:
      result = BigInt::BitwiseAnd(isolate, left, right);
      break;
    case Operation::kBitwiseOr:
      result = BigInt::BitwiseOr(isolate, left, right);
      break;
    case Operation::kBitwiseXor:
      result = BigInt::BitwiseXor(isolate, left, right);
      break;
    case Operation::kShiftLeft:
      result = BigInt::LeftShift(isolate, left, right);
      break;
    case Operation::kShiftRight:
      result = BigInt::SignedRightShift(isolate, left, right);
      break;
    case Operation::kShiftRightLogical:
      result = BigInt::UnsignedRightShift(isolate, left, right);
      break;
    default:
      UNREACHABLE();
  }
  RETURN_RESULT_OR_FAILURE(isolate, result);
}

RUNTIME_FUNCTION(Runtime_BigIntUnaryOp) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<BigInt> x = args.at<BigInt>(0);
  int opcode = args.smi_value_at(1);
  Operation op = static_cast<Operation>(opcode);

  MaybeHandle<BigInt> result;
  switch (op) {
    case Operation::kBitwiseNot:
      result = BigInt::BitwiseNot(isolate, x);
      break;
    case Operation::kNegate:
      result = BigInt::UnaryMinus(isolate, x);
      break;
    case Operation::kIncrement:
      result = BigInt::Increment(isolate, x);
      break;
    case Operation::kDecrement:
      result = BigInt::Decrement(isolate, x);
      break;
    default:
      UNREACHABLE();
  }
  RETURN_RESULT_OR_FAILURE(isolate, result);
}

}  // namespace internal
}  // namespace v8

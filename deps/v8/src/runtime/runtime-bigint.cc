// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/counters.h"
#include "src/objects-inl.h"
#include "src/objects/bigint.h"
#include "src/parsing/token.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_BigIntEqual) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  bool result = lhs->IsBigInt() && rhs->IsBigInt() &&
                BigInt::Equal(BigInt::cast(*lhs), BigInt::cast(*rhs));
  return *isolate->factory()->ToBoolean(result);
}

RUNTIME_FUNCTION(Runtime_BigIntToBoolean) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(BigInt, bigint, 0);
  return *isolate->factory()->ToBoolean(bigint->ToBoolean());
}

RUNTIME_FUNCTION(Runtime_BigIntBinaryOp) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, left_obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, right_obj, 1);
  CONVERT_SMI_ARG_CHECKED(opcode, 2);

  if (!left_obj->IsBigInt() || !right_obj->IsBigInt()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kBigIntMixedTypes));
  }
  Handle<BigInt> left(Handle<BigInt>::cast(left_obj));
  Handle<BigInt> right(Handle<BigInt>::cast(right_obj));
  MaybeHandle<BigInt> result;
  switch (opcode) {
    case Token::ADD:
      result = BigInt::Add(left, right);
      break;
    case Token::SUB:
      result = BigInt::Subtract(left, right);
      break;
    case Token::MUL:
      result = BigInt::Multiply(left, right);
      break;
    case Token::DIV:
      result = BigInt::Divide(left, right);
      break;
    case Token::MOD:
      result = BigInt::Remainder(left, right);
      break;
    default:
      UNREACHABLE();
  }
  RETURN_RESULT_OR_FAILURE(isolate, result);
}

}  // namespace internal
}  // namespace v8

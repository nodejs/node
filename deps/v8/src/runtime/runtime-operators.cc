// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arguments.h"
#include "src/isolate-inl.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_Multiply) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     Object::Multiply(isolate, lhs, rhs));
  return *result;
}


RUNTIME_FUNCTION(Runtime_Multiply_Strong) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, Object::Multiply(isolate, lhs, rhs, Strength::STRONG));
  return *result;
}


RUNTIME_FUNCTION(Runtime_Divide) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     Object::Divide(isolate, lhs, rhs));
  return *result;
}


RUNTIME_FUNCTION(Runtime_Divide_Strong) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, Object::Divide(isolate, lhs, rhs, Strength::STRONG));
  return *result;
}


RUNTIME_FUNCTION(Runtime_Modulus) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     Object::Modulus(isolate, lhs, rhs));
  return *result;
}


RUNTIME_FUNCTION(Runtime_Modulus_Strong) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, Object::Modulus(isolate, lhs, rhs, Strength::STRONG));
  return *result;
}


RUNTIME_FUNCTION(Runtime_Add) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     Object::Add(isolate, lhs, rhs));
  return *result;
}


RUNTIME_FUNCTION(Runtime_Add_Strong) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, Object::Add(isolate, lhs, rhs, Strength::STRONG));
  return *result;
}


RUNTIME_FUNCTION(Runtime_Subtract) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     Object::Subtract(isolate, lhs, rhs));
  return *result;
}


RUNTIME_FUNCTION(Runtime_Subtract_Strong) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, Object::Subtract(isolate, lhs, rhs, Strength::STRONG));
  return *result;
}


RUNTIME_FUNCTION(Runtime_ShiftLeft) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     Object::ShiftLeft(isolate, lhs, rhs));
  return *result;
}


RUNTIME_FUNCTION(Runtime_ShiftLeft_Strong) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, Object::ShiftLeft(isolate, lhs, rhs, Strength::STRONG));
  return *result;
}


RUNTIME_FUNCTION(Runtime_ShiftRight) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     Object::ShiftRight(isolate, lhs, rhs));
  return *result;
}


RUNTIME_FUNCTION(Runtime_ShiftRight_Strong) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, Object::ShiftRight(isolate, lhs, rhs, Strength::STRONG));
  return *result;
}


RUNTIME_FUNCTION(Runtime_ShiftRightLogical) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, Object::ShiftRightLogical(isolate, lhs, rhs));
  return *result;
}


RUNTIME_FUNCTION(Runtime_ShiftRightLogical_Strong) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Object::ShiftRightLogical(isolate, lhs, rhs, Strength::STRONG));
  return *result;
}


RUNTIME_FUNCTION(Runtime_BitwiseAnd) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     Object::BitwiseAnd(isolate, lhs, rhs));
  return *result;
}


RUNTIME_FUNCTION(Runtime_BitwiseAnd_Strong) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, Object::BitwiseAnd(isolate, lhs, rhs, Strength::STRONG));
  return *result;
}


RUNTIME_FUNCTION(Runtime_BitwiseOr) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     Object::BitwiseOr(isolate, lhs, rhs));
  return *result;
}


RUNTIME_FUNCTION(Runtime_BitwiseOr_Strong) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, Object::BitwiseOr(isolate, lhs, rhs, Strength::STRONG));
  return *result;
}


RUNTIME_FUNCTION(Runtime_BitwiseXor) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     Object::BitwiseXor(isolate, lhs, rhs));
  return *result;
}


RUNTIME_FUNCTION(Runtime_BitwiseXor_Strong) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, Object::BitwiseXor(isolate, lhs, rhs, Strength::STRONG));
  return *result;
}

}  // namespace internal
}  // namespace v8

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arguments.h"
#include "src/isolate-inl.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_Add) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, lhs, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, rhs, 1);
  RETURN_RESULT_OR_FAILURE(isolate, Object::Add(isolate, lhs, rhs));
}


RUNTIME_FUNCTION(Runtime_Equal) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, x, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, y, 1);
  Maybe<bool> result = Object::Equals(x, y);
  if (result.IsNothing()) return isolate->heap()->exception();
  return isolate->heap()->ToBoolean(result.FromJust());
}

RUNTIME_FUNCTION(Runtime_NotEqual) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, x, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, y, 1);
  Maybe<bool> result = Object::Equals(x, y);
  if (result.IsNothing()) return isolate->heap()->exception();
  return isolate->heap()->ToBoolean(!result.FromJust());
}

RUNTIME_FUNCTION(Runtime_StrictEqual) {
  SealHandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_CHECKED(Object, x, 0);
  CONVERT_ARG_CHECKED(Object, y, 1);
  return isolate->heap()->ToBoolean(x->StrictEquals(y));
}

RUNTIME_FUNCTION(Runtime_StrictNotEqual) {
  SealHandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_CHECKED(Object, x, 0);
  CONVERT_ARG_CHECKED(Object, y, 1);
  return isolate->heap()->ToBoolean(!x->StrictEquals(y));
}

RUNTIME_FUNCTION(Runtime_LessThan) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, x, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, y, 1);
  Maybe<bool> result = Object::LessThan(x, y);
  if (result.IsNothing()) return isolate->heap()->exception();
  return isolate->heap()->ToBoolean(result.FromJust());
}

RUNTIME_FUNCTION(Runtime_GreaterThan) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, x, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, y, 1);
  Maybe<bool> result = Object::GreaterThan(x, y);
  if (result.IsNothing()) return isolate->heap()->exception();
  return isolate->heap()->ToBoolean(result.FromJust());
}

RUNTIME_FUNCTION(Runtime_LessThanOrEqual) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, x, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, y, 1);
  Maybe<bool> result = Object::LessThanOrEqual(x, y);
  if (result.IsNothing()) return isolate->heap()->exception();
  return isolate->heap()->ToBoolean(result.FromJust());
}

RUNTIME_FUNCTION(Runtime_GreaterThanOrEqual) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, x, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, y, 1);
  Maybe<bool> result = Object::GreaterThanOrEqual(x, y);
  if (result.IsNothing()) return isolate->heap()->exception();
  return isolate->heap()->ToBoolean(result.FromJust());
}

}  // namespace internal
}  // namespace v8

// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_DISPOSABLE_STACK_INL_H_
#define V8_OBJECTS_JS_DISPOSABLE_STACK_INL_H_

#include "src/execution/isolate.h"
#include "src/handles/handles.h"
#include "src/heap/factory.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/js-disposable-stack.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-disposable-stack-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(JSDisposableStack)

CAST_ACCESSOR(JSDisposableStack)

BIT_FIELD_ACCESSORS(JSDisposableStack, status, state,
                    JSDisposableStack::StateBit)
BIT_FIELD_ACCESSORS(JSDisposableStack, status, length,
                    JSDisposableStack::LengthBits)

inline void JSDisposableStack::Add(Isolate* isolate,
                                   Handle<JSDisposableStack> disposable_stack,
                                   Handle<Object> value, Handle<Object> method,
                                   DisposeMethodCallType type) {
  DCHECK(!IsUndefined(disposable_stack->stack()));
  int length = disposable_stack->length();
  Handle<Smi> call_type(Smi::FromEnum(type), isolate);
  Handle<FixedArray> array(disposable_stack->stack(), isolate);
  array = FixedArray::SetAndGrow(isolate, array, length++, value);
  array = FixedArray::SetAndGrow(isolate, array, length++, method);
  array = FixedArray::SetAndGrow(isolate, array, length++, call_type);

  disposable_stack->set_length(length);
  disposable_stack->set_stack(*array);
}

// part of
// https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-createdisposableresource
inline MaybeHandle<Object> JSDisposableStack::CheckValueAndGetDisposeMethod(
    Isolate* isolate, Handle<Object> value) {
  // 1. If method is not present, then
  //   a. If V is either null or undefined, then
  //    i. Set V to undefined.
  //    ii. Set method to undefined.
  // We has already returned from the caller if V is null or undefined.
  DCHECK(!IsNullOrUndefined(*value));

  //   b. Else,
  //    i. If V is not an Object, throw a TypeError exception.
  if (!IsJSReceiver(*value)) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kExpectAnObjectWithUsing),
                    Object);
  }

  //   ii. Set method to ? GetDisposeMethod(V, hint).
  Handle<Object> method;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, method,
      Object::GetProperty(isolate, value, isolate->factory()->dispose_symbol()),
      Object);
  //   (GetMethod)3. If IsCallable(func) is false, throw a TypeError exception.
  if (!IsJSFunction(*method)) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kNotCallable,
                                 isolate->factory()->dispose_symbol()),
                    Object);
  }

  //   iii. If method is undefined, throw a TypeError exception.
  //   It is already checked in step ii.

  return method;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_DISPOSABLE_STACK_INL_H_

// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/isolate-inl.h"

namespace v8 {
namespace internal {


RUNTIME_FUNCTION(Runtime_InterpreterEquals) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, x, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, y, 1);
  Maybe<bool> result = Object::Equals(x, y);
  if (result.IsJust()) {
    return isolate->heap()->ToBoolean(result.FromJust());
  } else {
    return isolate->heap()->exception();
  }
}


RUNTIME_FUNCTION(Runtime_InterpreterNotEquals) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, x, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, y, 1);
  Maybe<bool> result = Object::Equals(x, y);
  if (result.IsJust()) {
    return isolate->heap()->ToBoolean(!result.FromJust());
  } else {
    return isolate->heap()->exception();
  }
}


RUNTIME_FUNCTION(Runtime_InterpreterLessThan) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, x, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, y, 1);
  Maybe<bool> result = Object::LessThan(x, y);
  if (result.IsJust()) {
    return isolate->heap()->ToBoolean(result.FromJust());
  } else {
    return isolate->heap()->exception();
  }
}


RUNTIME_FUNCTION(Runtime_InterpreterGreaterThan) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, x, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, y, 1);
  Maybe<bool> result = Object::GreaterThan(x, y);
  if (result.IsJust()) {
    return isolate->heap()->ToBoolean(result.FromJust());
  } else {
    return isolate->heap()->exception();
  }
}


RUNTIME_FUNCTION(Runtime_InterpreterLessThanOrEqual) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, x, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, y, 1);
  Maybe<bool> result = Object::LessThanOrEqual(x, y);
  if (result.IsJust()) {
    return isolate->heap()->ToBoolean(result.FromJust());
  } else {
    return isolate->heap()->exception();
  }
}


RUNTIME_FUNCTION(Runtime_InterpreterGreaterThanOrEqual) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, x, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, y, 1);
  Maybe<bool> result = Object::GreaterThanOrEqual(x, y);
  if (result.IsJust()) {
    return isolate->heap()->ToBoolean(result.FromJust());
  } else {
    return isolate->heap()->exception();
  }
}


RUNTIME_FUNCTION(Runtime_InterpreterStrictEquals) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_CHECKED(Object, x, 0);
  CONVERT_ARG_CHECKED(Object, y, 1);
  return isolate->heap()->ToBoolean(x->StrictEquals(y));
}


RUNTIME_FUNCTION(Runtime_InterpreterStrictNotEquals) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_CHECKED(Object, x, 0);
  CONVERT_ARG_CHECKED(Object, y, 1);
  return isolate->heap()->ToBoolean(!x->StrictEquals(y));
}


RUNTIME_FUNCTION(Runtime_InterpreterToBoolean) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, x, 0);
  return isolate->heap()->ToBoolean(x->BooleanValue());
}


RUNTIME_FUNCTION(Runtime_InterpreterLogicalNot) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, x, 0);
  return isolate->heap()->ToBoolean(!x->BooleanValue());
}


RUNTIME_FUNCTION(Runtime_InterpreterTypeOf) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, x, 0);
  return Object::cast(*Object::TypeOf(isolate, x));
}


RUNTIME_FUNCTION(Runtime_InterpreterNewClosure) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(SharedFunctionInfo, shared, 0);
  CONVERT_SMI_ARG_CHECKED(pretenured_flag, 1);
  Handle<Context> context(isolate->context(), isolate);
  return *isolate->factory()->NewFunctionFromSharedFunctionInfo(
      shared, context, static_cast<PretenureFlag>(pretenured_flag));
}


RUNTIME_FUNCTION(Runtime_InterpreterForInPrepare) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);

  Object* property_names = Runtime_GetPropertyNamesFast(
      1, Handle<Object>::cast(receiver).location(), isolate);
  if (isolate->has_pending_exception()) {
    return property_names;
  }

  Handle<Object> cache_type(property_names, isolate);
  Handle<FixedArray> cache_array;
  int cache_length;

  Handle<Map> receiver_map = handle(receiver->map(), isolate);
  if (cache_type->IsMap()) {
    Handle<Map> cache_type_map =
        handle(Handle<Map>::cast(cache_type)->map(), isolate);
    DCHECK(cache_type_map.is_identical_to(isolate->factory()->meta_map()));
    int enum_length = cache_type_map->EnumLength();
    DescriptorArray* descriptors = receiver_map->instance_descriptors();
    if (enum_length > 0 && descriptors->HasEnumCache()) {
      cache_array = handle(descriptors->GetEnumCache(), isolate);
      cache_length = cache_array->length();
    } else {
      cache_array = isolate->factory()->empty_fixed_array();
      cache_length = 0;
    }
  } else {
    cache_array = Handle<FixedArray>::cast(cache_type);
    cache_length = cache_array->length();

    STATIC_ASSERT(JS_PROXY_TYPE == FIRST_JS_RECEIVER_TYPE);
    if (receiver_map->instance_type() == JS_PROXY_TYPE) {
      // Zero indicates proxy
      cache_type = Handle<Object>(Smi::FromInt(0), isolate);
    } else {
      // One entails slow check
      cache_type = Handle<Object>(Smi::FromInt(1), isolate);
    }
  }

  Handle<FixedArray> result = isolate->factory()->NewFixedArray(3);
  result->set(0, *cache_type);
  result->set(1, *cache_array);
  result->set(2, Smi::FromInt(cache_length));
  return *result;
}

}  // namespace internal
}  // namespace v8

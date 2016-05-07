// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/elements.h"
#include "src/factory.h"
#include "src/isolate-inl.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {


// ES6 9.5.13 [[Call]] (thisArgument, argumentsList)
RUNTIME_FUNCTION(Runtime_JSProxyCall) {
  HandleScope scope(isolate);
  DCHECK_LE(2, args.length());
  // thisArgument == receiver
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSProxy, proxy, args.length() - 1);
  Handle<String> trap_name = isolate->factory()->apply_string();
  // 1. Let handler be the value of the [[ProxyHandler]] internal slot of O.
  Handle<Object> handler(proxy->handler(), isolate);
  // 2. If handler is null, throw a TypeError exception.
  if (proxy->IsRevoked()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kProxyRevoked, trap_name));
  }
  // 3. Assert: Type(handler) is Object.
  DCHECK(handler->IsJSReceiver());
  // 4. Let target be the value of the [[ProxyTarget]] internal slot of O.
  Handle<JSReceiver> target(proxy->target(), isolate);
  // 5. Let trap be ? GetMethod(handler, "apply").
  Handle<Object> trap;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, trap,
      Object::GetMethod(Handle<JSReceiver>::cast(handler), trap_name));
  // 6. If trap is undefined, then
  int const arguments_length = args.length() - 2;
  if (trap->IsUndefined()) {
    // 6.a. Return Call(target, thisArgument, argumentsList).
    ScopedVector<Handle<Object>> argv(arguments_length);
    for (int i = 0; i < arguments_length; ++i) {
      argv[i] = args.at<Object>(i + 1);
    }
    Handle<Object> result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, Execution::Call(isolate, target, receiver,
                                         arguments_length, argv.start()));
    return *result;
  }
  // 7. Let argArray be CreateArrayFromList(argumentsList).
  Handle<JSArray> arg_array = isolate->factory()->NewJSArray(
      FAST_ELEMENTS, arguments_length, arguments_length);
  ElementsAccessor* accessor = arg_array->GetElementsAccessor();
  {
    DisallowHeapAllocation no_gc;
    for (int i = 0; i < arguments_length; i++) {
      accessor->Set(arg_array, i, args[i + 1]);
    }
  }
  // 8. Return Call(trap, handler, «target, thisArgument, argArray»).
  Handle<Object> trap_result;
  Handle<Object> trap_args[] = {target, receiver, arg_array};
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, trap_result,
      Execution::Call(isolate, trap, handler, arraysize(trap_args), trap_args));
  return *trap_result;
}


// 9.5.14 [[Construct]] (argumentsList, newTarget)
RUNTIME_FUNCTION(Runtime_JSProxyConstruct) {
  HandleScope scope(isolate);
  DCHECK_LE(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSProxy, proxy, args.length() - 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, new_target, args.length() - 1);
  Handle<String> trap_name = isolate->factory()->construct_string();

  // 1. Let handler be the value of the [[ProxyHandler]] internal slot of O.
  Handle<Object> handler(proxy->handler(), isolate);
  // 2. If handler is null, throw a TypeError exception.
  if (proxy->IsRevoked()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kProxyRevoked, trap_name));
  }
  // 3. Assert: Type(handler) is Object.
  DCHECK(handler->IsJSReceiver());
  // 4. Let target be the value of the [[ProxyTarget]] internal slot of O.
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
  // 5. Let trap be ? GetMethod(handler, "construct").
  Handle<Object> trap;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, trap,
      Object::GetMethod(Handle<JSReceiver>::cast(handler), trap_name));
  // 6. If trap is undefined, then
  int const arguments_length = args.length() - 3;
  if (trap->IsUndefined()) {
    // 6.a. Assert: target has a [[Construct]] internal method.
    DCHECK(target->IsConstructor());
    // 6.b. Return Construct(target, argumentsList, newTarget).
    ScopedVector<Handle<Object>> argv(arguments_length);
    for (int i = 0; i < arguments_length; ++i) {
      argv[i] = args.at<Object>(i + 1);
    }
    Handle<Object> result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, Execution::New(isolate, target, new_target,
                                        arguments_length, argv.start()));
    return *result;
  }
  // 7. Let argArray be CreateArrayFromList(argumentsList).
  Handle<JSArray> arg_array = isolate->factory()->NewJSArray(
      FAST_ELEMENTS, arguments_length, arguments_length);
  ElementsAccessor* accessor = arg_array->GetElementsAccessor();
  {
    DisallowHeapAllocation no_gc;
    for (int i = 0; i < arguments_length; i++) {
      accessor->Set(arg_array, i, args[i + 1]);
    }
  }
  // 8. Let newObj be ? Call(trap, handler, «target, argArray, newTarget »).
  Handle<Object> new_object;
  Handle<Object> trap_args[] = {target, arg_array, new_target};
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, new_object,
      Execution::Call(isolate, trap, handler, arraysize(trap_args), trap_args));
  // 9. If Type(newObj) is not Object, throw a TypeError exception.
  if (!new_object->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(MessageTemplate::kProxyConstructNonObject, new_object));
  }
  // 10. Return newObj.
  return *new_object;
}


RUNTIME_FUNCTION(Runtime_IsJSProxy) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSProxy());
}


RUNTIME_FUNCTION(Runtime_JSProxyGetHandler) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSProxy, proxy, 0);
  return proxy->handler();
}


RUNTIME_FUNCTION(Runtime_JSProxyGetTarget) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSProxy, proxy, 0);
  return proxy->target();
}


RUNTIME_FUNCTION(Runtime_JSProxyRevoke) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSProxy, proxy, 0);
  JSProxy::Revoke(proxy);
  return isolate->heap()->undefined_value();
}

}  // namespace internal
}  // namespace v8

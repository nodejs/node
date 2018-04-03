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


RUNTIME_FUNCTION(Runtime_IsJSProxy) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSProxy());
}


RUNTIME_FUNCTION(Runtime_JSProxyGetHandler) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(JSProxy, proxy, 0);
  return proxy->handler();
}


RUNTIME_FUNCTION(Runtime_JSProxyGetTarget) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(JSProxy, proxy, 0);
  return proxy->target();
}


RUNTIME_FUNCTION(Runtime_GetPropertyWithReceiver) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 2);

  bool success = false;
  LookupIterator it = LookupIterator::PropertyOrElement(isolate, receiver, key,
                                                        &success, holder);
  if (!success) {
    DCHECK(isolate->has_pending_exception());
    return isolate->heap()->exception();
  }
  RETURN_RESULT_OR_FAILURE(isolate, Object::GetProperty(&it));
}

RUNTIME_FUNCTION(Runtime_SetPropertyWithReceiver) {
  HandleScope scope(isolate);

  DCHECK_EQ(5, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 3);
  CONVERT_LANGUAGE_MODE_ARG_CHECKED(language_mode, 4);

  bool success = false;
  LookupIterator it = LookupIterator::PropertyOrElement(isolate, receiver, key,
                                                        &success, holder);
  if (!success) {
    DCHECK(isolate->has_pending_exception());
    return isolate->heap()->exception();
  }
  Maybe<bool> result = Object::SetSuperProperty(
      &it, value, language_mode, Object::MAY_BE_STORE_FROM_KEYED);
  MAYBE_RETURN(result, isolate->heap()->exception());
  return *isolate->factory()->ToBoolean(result.FromJust());
}

RUNTIME_FUNCTION(Runtime_CheckProxyGetSetTrapResult) {
  HandleScope scope(isolate);

  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, target, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, trap_result, 2);
  CONVERT_NUMBER_CHECKED(int64_t, access_kind, Int64, args[3]);

  RETURN_RESULT_OR_FAILURE(isolate, JSProxy::CheckGetSetTrapResult(
                                        isolate, name, target, trap_result,
                                        JSProxy::AccessKind(access_kind)));
}

RUNTIME_FUNCTION(Runtime_CheckProxyHasTrap) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, target, 1);

  Maybe<bool> result = JSProxy::CheckHasTrap(isolate, name, target);
  if (!result.IsJust()) return isolate->heap()->exception();
  return isolate->heap()->ToBoolean(result.FromJust());
}

}  // namespace internal
}  // namespace v8

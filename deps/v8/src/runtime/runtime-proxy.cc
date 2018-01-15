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


RUNTIME_FUNCTION(Runtime_JSProxyRevoke) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSProxy, proxy, 0);
  JSProxy::Revoke(proxy);
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_GetPropertyWithReceiver) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 2);

  bool success;
  LookupIterator it = LookupIterator::PropertyOrElement(isolate, receiver, name,
                                                        &success, holder);
  if (!success) {
    DCHECK(isolate->has_pending_exception());
    return isolate->heap()->exception();
  }
  RETURN_RESULT_OR_FAILURE(isolate, Object::GetProperty(&it));
}

RUNTIME_FUNCTION(Runtime_CheckProxyGetTrapResult) {
  HandleScope scope(isolate);

  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, target, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, trap_result, 2);

  RETURN_RESULT_OR_FAILURE(
      isolate, JSProxy::CheckGetTrapResult(isolate, name, target, trap_result));
}

RUNTIME_FUNCTION(Runtime_CheckProxyHasTrap) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, target, 1);

  RETURN_ON_SCHEDULED_EXCEPTION_VALUE(
      isolate, JSProxy::CheckHasTrap(isolate, name, target),
      *isolate->factory()->undefined_value());
  return *isolate->factory()->undefined_value();
}

}  // namespace internal
}  // namespace v8

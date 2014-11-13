// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/arguments.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_CreateJSProxy) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, handler, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 1);
  if (!prototype->IsJSReceiver()) prototype = isolate->factory()->null_value();
  return *isolate->factory()->NewJSProxy(handler, prototype);
}


RUNTIME_FUNCTION(Runtime_CreateJSFunctionProxy) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, handler, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, call_trap, 1);
  RUNTIME_ASSERT(call_trap->IsJSFunction() || call_trap->IsJSFunctionProxy());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, construct_trap, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 3);
  if (!prototype->IsJSReceiver()) prototype = isolate->factory()->null_value();
  return *isolate->factory()->NewJSFunctionProxy(handler, call_trap,
                                                 construct_trap, prototype);
}


RUNTIME_FUNCTION(RuntimeReference_IsJSProxy) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSProxy());
}


RUNTIME_FUNCTION(Runtime_IsJSFunctionProxy) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSFunctionProxy());
}


RUNTIME_FUNCTION(Runtime_GetHandler) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSProxy, proxy, 0);
  return proxy->handler();
}


RUNTIME_FUNCTION(Runtime_GetCallTrap) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunctionProxy, proxy, 0);
  return proxy->call_trap();
}


RUNTIME_FUNCTION(Runtime_GetConstructTrap) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunctionProxy, proxy, 0);
  return proxy->construct_trap();
}


RUNTIME_FUNCTION(Runtime_Fix) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSProxy, proxy, 0);
  JSProxy::Fix(proxy);
  return isolate->heap()->undefined_value();
}
}
}  // namespace v8::internal

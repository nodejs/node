// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/api.h"
#include "src/arguments.h"
#include "src/counters.h"
#include "src/debug/debug.h"
#include "src/elements.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_PromiseRejectEventFromStack) {
  DCHECK_EQ(2, args.length());
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);

  Handle<Object> rejected_promise = promise;
  if (isolate->debug()->is_active()) {
    // If the Promise.reject() call is caught, then this will return
    // undefined, which we interpret as being a caught exception event.
    rejected_promise = isolate->GetPromiseOnStackOnThrow();
  }
  isolate->RunPromiseHook(PromiseHookType::kResolve, promise,
                          isolate->factory()->undefined_value());
  isolate->debug()->OnPromiseReject(rejected_promise, value);

  // Report only if we don't actually have a handler.
  if (!promise->has_handler()) {
    isolate->ReportPromiseReject(promise, value,
                                 v8::kPromiseRejectWithNoHandler);
  }
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_PromiseRevokeReject) {
  DCHECK_EQ(1, args.length());
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  // At this point, no revocation has been issued before
  CHECK(!promise->has_handler());
  isolate->ReportPromiseReject(promise, Handle<Object>(),
                               v8::kPromiseHandlerAddedAfterReject);
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_EnqueueMicrotask) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  Handle<CallableTask> microtask =
      isolate->factory()->NewCallableTask(function, isolate->native_context());
  isolate->EnqueueMicrotask(microtask);
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_RunMicrotasks) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  isolate->RunMicrotasks();
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_RunMicrotaskCallback) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_CHECKED(Object, microtask_callback, 0);
  CONVERT_ARG_CHECKED(Object, microtask_data, 1);
  MicrotaskCallback callback = ToCData<MicrotaskCallback>(microtask_callback);
  void* data = ToCData<void*>(microtask_data);
  callback(data);
  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_PromiseStatus) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);

  return Smi::FromInt(promise->status());
}

RUNTIME_FUNCTION(Runtime_PromiseResult) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  return promise->result();
}

RUNTIME_FUNCTION(Runtime_PromiseMarkAsHandled) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(JSPromise, promise, 0);

  promise->set_has_handler(true);
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_PromiseHookInit) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, parent, 1);
  isolate->RunPromiseHook(PromiseHookType::kInit, promise, parent);
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_PromiseHookBefore) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(HeapObject, payload, 0);
  Handle<JSPromise> promise;
  if (JSPromise::From(payload).ToHandle(&promise)) {
    if (isolate->debug()->is_active()) isolate->PushPromise(promise);
    if (promise->IsJSPromise()) {
      isolate->RunPromiseHook(PromiseHookType::kBefore, promise,
                              isolate->factory()->undefined_value());
    }
  }
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_PromiseHookAfter) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(HeapObject, payload, 0);
  Handle<JSPromise> promise;
  if (JSPromise::From(payload).ToHandle(&promise)) {
    if (isolate->debug()->is_active()) isolate->PopPromise();
    if (promise->IsJSPromise()) {
      isolate->RunPromiseHook(PromiseHookType::kAfter, promise,
                              isolate->factory()->undefined_value());
    }
  }
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_RejectPromise) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, reason, 1);
  CONVERT_ARG_HANDLE_CHECKED(Oddball, debug_event, 2);
  return *JSPromise::Reject(promise, reason, debug_event->BooleanValue());
}

RUNTIME_FUNCTION(Runtime_ResolvePromise) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, resolution, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     JSPromise::Resolve(promise, resolution));
  return *result;
}

}  // namespace internal
}  // namespace v8

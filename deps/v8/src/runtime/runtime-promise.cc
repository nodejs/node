// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/counters.h"
#include "src/debug/debug.h"
#include "src/elements.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

namespace {

void PromiseRejectEvent(Isolate* isolate, Handle<JSPromise> promise,
                        Handle<Object> rejected_promise, Handle<Object> value,
                        bool debug_event) {
  isolate->RunPromiseHook(PromiseHookType::kResolve, promise,
                          isolate->factory()->undefined_value());

  if (isolate->debug()->is_active() && debug_event) {
    isolate->debug()->OnPromiseReject(rejected_promise, value);
  }

  // Report only if we don't actually have a handler.
  if (!promise->has_handler()) {
    isolate->ReportPromiseReject(promise, value,
                                 v8::kPromiseRejectWithNoHandler);
  }
}

}  // namespace

RUNTIME_FUNCTION(Runtime_PromiseRejectEventFromStack) {
  DCHECK_EQ(2, args.length());
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);

  Handle<Object> rejected_promise = promise;
  if (isolate->debug()->is_active()) {
    // If the Promise.reject call is caught, then this will return
    // undefined, which will be interpreted by PromiseRejectEvent
    // as being a caught exception event.
    rejected_promise = isolate->GetPromiseOnStackOnThrow();
  }
  PromiseRejectEvent(isolate, promise, rejected_promise, value, true);
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_ReportPromiseReject) {
  DCHECK_EQ(2, args.length());
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);
  isolate->ReportPromiseReject(promise, value, v8::kPromiseRejectWithNoHandler);
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

RUNTIME_FUNCTION(Runtime_EnqueuePromiseReactionJob) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(PromiseReactionJobInfo, info, 0);
  isolate->EnqueueMicrotask(info);
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_EnqueuePromiseResolveThenableJob) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 1);
  CONVERT_ARG_HANDLE_CHECKED(PromiseResolveThenableJobInfo, info, 0);
  isolate->EnqueueMicrotask(info);
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_EnqueueMicrotask) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, microtask, 0);
  isolate->EnqueueMicrotask(microtask);
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_RunMicrotasks) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  isolate->RunMicrotasks();
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

RUNTIME_FUNCTION(Runtime_PromiseHookResolve) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  isolate->RunPromiseHook(PromiseHookType::kResolve, promise,
                          isolate->factory()->undefined_value());
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_PromiseHookBefore) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, promise, 0);
  if (promise->IsJSPromise()) {
    isolate->RunPromiseHook(PromiseHookType::kBefore,
                            Handle<JSPromise>::cast(promise),
                            isolate->factory()->undefined_value());
  }
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_PromiseHookAfter) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, promise, 0);
  if (promise->IsJSPromise()) {
    isolate->RunPromiseHook(PromiseHookType::kAfter,
                            Handle<JSPromise>::cast(promise),
                            isolate->factory()->undefined_value());
  }
  return isolate->heap()->undefined_value();
}

}  // namespace internal
}  // namespace v8

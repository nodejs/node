// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "src/runtime/runtime-utils.h"

#include "src/debug/debug.h"
#include "src/elements.h"

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
    isolate->ReportPromiseReject(Handle<JSObject>::cast(promise), value,
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
    isolate->debug()->OnAsyncTaskEvent(
        debug::kDebugEnqueuePromiseReject,
        isolate->debug()->NextAsyncTaskId(promise));
  }
  PromiseRejectEvent(isolate, promise, rejected_promise, value, true);
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_ReportPromiseReject) {
  DCHECK_EQ(2, args.length());
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);
  isolate->ReportPromiseReject(Handle<JSObject>::cast(promise), value,
                               v8::kPromiseRejectWithNoHandler);
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

namespace {

// In an async function, reuse the existing stack related to the outer
// Promise. Otherwise, e.g. in a direct call to then, save a new stack.
// Promises with multiple reactions with one or more of them being async
// functions will not get a good stack trace, as async functions require
// different stacks from direct Promise use, but we save and restore a
// stack once for all reactions.
//
// If this isn't a case of async function, we return false, otherwise
// we set the correct id and return true.
//
// TODO(littledan): Improve this case.
bool GetDebugIdForAsyncFunction(Isolate* isolate,
                                Handle<PromiseReactionJobInfo> info,
                                int* debug_id) {
  // deferred_promise can be Undefined, FixedArray or userland promise object.
  if (!info->deferred_promise()->IsJSPromise()) {
    return false;
  }

  Handle<JSPromise> deferred_promise(JSPromise::cast(info->deferred_promise()),
                                     isolate);
  Handle<Symbol> handled_by_symbol =
      isolate->factory()->promise_handled_by_symbol();
  Handle<Object> handled_by_promise =
      JSObject::GetDataProperty(deferred_promise, handled_by_symbol);

  if (!handled_by_promise->IsJSPromise()) {
    return false;
  }

  Handle<JSPromise> handled_by_promise_js =
      Handle<JSPromise>::cast(handled_by_promise);
  Handle<Symbol> async_stack_id_symbol =
      isolate->factory()->promise_async_stack_id_symbol();
  Handle<Object> id =
      JSObject::GetDataProperty(handled_by_promise_js, async_stack_id_symbol);

  // id can be Undefined or Smi.
  if (!id->IsSmi()) {
    return false;
  }

  *debug_id = Handle<Smi>::cast(id)->value();
  return true;
}

void SetDebugInfo(Isolate* isolate, Handle<JSPromise> promise,
                  Handle<PromiseReactionJobInfo> info, int status) {
  int id = kDebugPromiseNoID;
  if (!GetDebugIdForAsyncFunction(isolate, info, &id)) {
    id = isolate->debug()->NextAsyncTaskId(promise);
    DCHECK(status != v8::Promise::kPending);
  }
  info->set_debug_id(id);
}

void EnqueuePromiseReactionJob(Isolate* isolate, Handle<JSPromise> promise,
                               Handle<PromiseReactionJobInfo> info,
                               int status) {
  if (isolate->debug()->is_active()) {
    SetDebugInfo(isolate, promise, info, status);
  }

  isolate->EnqueueMicrotask(info);
}

}  // namespace

RUNTIME_FUNCTION(Runtime_EnqueuePromiseReactionJob) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  CONVERT_ARG_HANDLE_CHECKED(PromiseReactionJobInfo, info, 1);
  CONVERT_SMI_ARG_CHECKED(status, 2);
  EnqueuePromiseReactionJob(isolate, promise, info, status);
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_EnqueuePromiseResolveThenableJob) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
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

RUNTIME_FUNCTION(Runtime_PromiseMarkHandledHint) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(JSPromise, promise, 0);

  promise->set_handled_hint(true);
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
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  isolate->RunPromiseHook(PromiseHookType::kBefore, promise,
                          isolate->factory()->undefined_value());
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_PromiseHookAfter) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  isolate->RunPromiseHook(PromiseHookType::kAfter, promise,
                          isolate->factory()->undefined_value());
  return isolate->heap()->undefined_value();
}

}  // namespace internal
}  // namespace v8

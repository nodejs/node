// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/arguments.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_IsObserved) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);

  if (!args[0]->IsJSReceiver()) return isolate->heap()->false_value();
  CONVERT_ARG_CHECKED(JSReceiver, obj, 0);
  DCHECK(!obj->IsJSGlobalProxy() || !obj->map()->is_observed());
  return isolate->heap()->ToBoolean(obj->map()->is_observed());
}


RUNTIME_FUNCTION(Runtime_SetIsObserved) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, obj, 0);
  RUNTIME_ASSERT(!obj->IsJSGlobalProxy());
  if (obj->IsJSProxy()) return isolate->heap()->undefined_value();
  RUNTIME_ASSERT(!obj->map()->is_observed());

  DCHECK(obj->IsJSObject());
  JSObject::SetObserved(Handle<JSObject>::cast(obj));
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_EnqueueMicrotask) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, microtask, 0);
  isolate->EnqueueMicrotask(microtask);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_RunMicrotasks) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  isolate->RunMicrotasks();
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DeliverObservationChangeRecords) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, callback, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, argument, 1);
  v8::TryCatch catcher;
  // We should send a message on uncaught exception thrown during
  // Object.observe delivery while not interrupting further delivery, thus
  // we make a call inside a verbose TryCatch.
  catcher.SetVerbose(true);
  Handle<Object> argv[] = {argument};
  USE(Execution::Call(isolate, callback, isolate->factory()->undefined_value(),
                      arraysize(argv), argv));
  if (isolate->has_pending_exception()) {
    isolate->ReportPendingMessages();
    isolate->clear_pending_exception();
    isolate->set_external_caught_exception(false);
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_GetObservationState) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  return isolate->heap()->observation_state();
}


static bool ContextsHaveSameOrigin(Handle<Context> context1,
                                   Handle<Context> context2) {
  return context1->security_token() == context2->security_token();
}


RUNTIME_FUNCTION(Runtime_ObserverObjectAndRecordHaveSameOrigin) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, observer, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, record, 2);

  Handle<Context> observer_context(observer->context()->native_context());
  Handle<Context> object_context(object->GetCreationContext());
  Handle<Context> record_context(record->GetCreationContext());

  return isolate->heap()->ToBoolean(
      ContextsHaveSameOrigin(object_context, observer_context) &&
      ContextsHaveSameOrigin(object_context, record_context));
}


RUNTIME_FUNCTION(Runtime_ObjectWasCreatedInCurrentOrigin) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);

  Handle<Context> creation_context(object->GetCreationContext(), isolate);
  return isolate->heap()->ToBoolean(
      ContextsHaveSameOrigin(creation_context, isolate->native_context()));
}


RUNTIME_FUNCTION(Runtime_GetObjectContextObjectObserve) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);

  Handle<Context> context(object->GetCreationContext(), isolate);
  return context->native_object_observe();
}


RUNTIME_FUNCTION(Runtime_GetObjectContextObjectGetNotifier) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);

  Handle<Context> context(object->GetCreationContext(), isolate);
  return context->native_object_get_notifier();
}


RUNTIME_FUNCTION(Runtime_GetObjectContextNotifierPerformChange) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object_info, 0);

  Handle<Context> context(object_info->GetCreationContext(), isolate);
  return context->native_object_notifier_perform_change();
}
}
}  // namespace v8::internal

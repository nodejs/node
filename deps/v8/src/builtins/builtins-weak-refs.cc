// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/logging/counters.h"
#include "src/objects/js-weak-refs-inl.h"

namespace v8 {
namespace internal {

BUILTIN(FinalizationGroupConstructor) {
  HandleScope scope(isolate);
  Handle<JSFunction> target = args.target();
  if (args.new_target()->IsUndefined(isolate)) {  // [[Call]]
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kConstructorNotFunction,
                              handle(target->shared().Name(), isolate)));
  }
  // [[Construct]]
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());
  Handle<Object> cleanup = args.atOrUndefined(isolate, 1);

  if (!cleanup->IsCallable()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kWeakRefsCleanupMustBeCallable));
  }

  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::New(target, new_target, Handle<AllocationSite>::null()));

  Handle<JSFinalizationGroup> finalization_group =
      Handle<JSFinalizationGroup>::cast(result);
  finalization_group->set_native_context(*isolate->native_context());
  finalization_group->set_cleanup(*cleanup);
  finalization_group->set_flags(
      JSFinalizationGroup::ScheduledForCleanupField::encode(false));

  DCHECK(finalization_group->active_cells().IsUndefined(isolate));
  DCHECK(finalization_group->cleared_cells().IsUndefined(isolate));
  DCHECK(finalization_group->key_map().IsUndefined(isolate));
  return *finalization_group;
}

BUILTIN(FinalizationGroupRegister) {
  HandleScope scope(isolate);
  const char* method_name = "FinalizationGroup.prototype.register";

  //  1. Let finalizationGroup be the this value.
  //
  //  2. If Type(finalizationGroup) is not Object, throw a TypeError
  //  exception.
  //
  //  4. If finalizationGroup does not have a [[Cells]] internal slot,
  //  throw a TypeError exception.
  CHECK_RECEIVER(JSFinalizationGroup, finalization_group, method_name);

  Handle<Object> target = args.atOrUndefined(isolate, 1);

  //  3. If Type(target) is not Object, throw a TypeError exception.
  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(MessageTemplate::kWeakRefsRegisterTargetMustBeObject));
  }

  Handle<Object> holdings = args.atOrUndefined(isolate, 2);
  if (target->SameValue(*holdings)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(
            MessageTemplate::kWeakRefsRegisterTargetAndHoldingsMustNotBeSame));
  }

  Handle<Object> unregister_token = args.atOrUndefined(isolate, 3);

  //  5. If Type(unregisterToken) is not Object,
  //    a. If unregisterToken is not undefined, throw a TypeError exception.
  if (!unregister_token->IsJSReceiver() && !unregister_token->IsUndefined()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(MessageTemplate::kWeakRefsUnregisterTokenMustBeObject,
                     unregister_token));
  }
  // TODO(marja): Realms.

  JSFinalizationGroup::Register(finalization_group,
                                Handle<JSReceiver>::cast(target), holdings,
                                unregister_token, isolate);
  return ReadOnlyRoots(isolate).undefined_value();
}

BUILTIN(FinalizationGroupUnregister) {
  HandleScope scope(isolate);
  const char* method_name = "FinalizationGroup.prototype.unregister";

  // 1. Let finalizationGroup be the this value.
  //
  // 2. If Type(finalizationGroup) is not Object, throw a TypeError
  //    exception.
  //
  // 3. If finalizationGroup does not have a [[Cells]] internal slot,
  //    throw a TypeError exception.
  CHECK_RECEIVER(JSFinalizationGroup, finalization_group, method_name);

  Handle<Object> unregister_token = args.atOrUndefined(isolate, 1);

  // 4. If Type(unregisterToken) is not Object, throw a TypeError exception.
  if (!unregister_token->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(MessageTemplate::kWeakRefsUnregisterTokenMustBeObject,
                     unregister_token));
  }

  bool success = JSFinalizationGroup::Unregister(
      finalization_group, Handle<JSReceiver>::cast(unregister_token), isolate);

  return *isolate->factory()->ToBoolean(success);
}

BUILTIN(FinalizationGroupCleanupSome) {
  HandleScope scope(isolate);
  const char* method_name = "FinalizationGroup.prototype.cleanupSome";

  // 1. Let finalizationGroup be the this value.
  //
  // 2. If Type(finalizationGroup) is not Object, throw a TypeError
  //    exception.
  //
  // 3. If finalizationGroup does not have a [[Cells]] internal slot,
  //    throw a TypeError exception.
  CHECK_RECEIVER(JSFinalizationGroup, finalization_group, method_name);

  Handle<Object> callback(finalization_group->cleanup(), isolate);
  Handle<Object> callback_obj = args.atOrUndefined(isolate, 1);

  // 4. If callback is not undefined and IsCallable(callback) is
  //    false, throw a TypeError exception.
  if (!callback_obj->IsUndefined(isolate)) {
    if (!callback_obj->IsCallable()) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate,
          NewTypeError(MessageTemplate::kWeakRefsCleanupMustBeCallable));
    }
    callback = callback_obj;
  }

  // Don't do set_scheduled_for_cleanup(false); we still have the microtask
  // scheduled and don't want to schedule another one in case the user never
  // executes microtasks.
  if (JSFinalizationGroup::Cleanup(isolate, finalization_group, callback)
          .IsNothing()) {
    DCHECK(isolate->has_pending_exception());
    return ReadOnlyRoots(isolate).exception();
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

BUILTIN(FinalizationGroupCleanupIteratorNext) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSFinalizationGroupCleanupIterator, iterator, "next");

  Handle<JSFinalizationGroup> finalization_group(iterator->finalization_group(),
                                                 isolate);
  if (!finalization_group->NeedsCleanup()) {
    return *isolate->factory()->NewJSIteratorResult(
        handle(ReadOnlyRoots(isolate).undefined_value(), isolate), true);
  }
  Handle<Object> holdings = handle(
      JSFinalizationGroup::PopClearedCellHoldings(finalization_group, isolate),
      isolate);

  return *isolate->factory()->NewJSIteratorResult(holdings, false);
}

BUILTIN(WeakRefConstructor) {
  HandleScope scope(isolate);
  Handle<JSFunction> target = args.target();
  if (args.new_target()->IsUndefined(isolate)) {  // [[Call]]
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kConstructorNotFunction,
                              handle(target->shared().Name(), isolate)));
  }
  // [[Construct]]
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());
  Handle<Object> target_object = args.atOrUndefined(isolate, 1);
  if (!target_object->IsJSReceiver()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(
            MessageTemplate::kWeakRefsWeakRefConstructorTargetMustBeObject));
  }
  Handle<JSReceiver> target_receiver =
      handle(JSReceiver::cast(*target_object), isolate);
  isolate->heap()->KeepDuringJob(target_receiver);

  // TODO(marja): Realms.

  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::New(target, new_target, Handle<AllocationSite>::null()));

  Handle<JSWeakRef> weak_ref = Handle<JSWeakRef>::cast(result);
  weak_ref->set_target(*target_receiver);
  return *weak_ref;
}

BUILTIN(WeakRefDeref) {
  HandleScope scope(isolate);
  CHECK_RECEIVER(JSWeakRef, weak_ref, "WeakRef.prototype.deref");
  if (weak_ref->target().IsJSReceiver()) {
    Handle<JSReceiver> target =
        handle(JSReceiver::cast(weak_ref->target()), isolate);
    // KeepDuringJob might allocate and cause a GC, but it won't clear
    // weak_ref since we hold a Handle to its target.
    isolate->heap()->KeepDuringJob(target);
  } else {
    DCHECK(weak_ref->target().IsUndefined(isolate));
  }
  return weak_ref->target();
}

}  // namespace internal
}  // namespace v8

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/execution/arguments-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/heap/factory.h"
#include "src/heap/heap-inl.h"  // For ToBoolean. TODO(jkummerow): Drop.
#include "src/logging/counters.h"
#include "src/objects/elements.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {


RUNTIME_FUNCTION(Runtime_GetPropertyWithReceiver) {
  HandleScope scope(isolate);

  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 2);
  CONVERT_ARG_HANDLE_CHECKED(Smi, on_non_existent, 3);

  bool success = false;
  LookupIterator it = LookupIterator::PropertyOrElement(isolate, receiver, key,
                                                        &success, holder);
  if (!success) {
    DCHECK(isolate->has_pending_exception());
    return ReadOnlyRoots(isolate).exception();
  }

  RETURN_RESULT_OR_FAILURE(
      isolate, Object::GetProperty(
                   &it, static_cast<OnNonExistent>(on_non_existent->value())));
}

RUNTIME_FUNCTION(Runtime_SetPropertyWithReceiver) {
  HandleScope scope(isolate);

  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 3);

  bool success = false;
  LookupIterator it = LookupIterator::PropertyOrElement(isolate, receiver, key,
                                                        &success, holder);
  if (!success) {
    DCHECK(isolate->has_pending_exception());
    return ReadOnlyRoots(isolate).exception();
  }
  Maybe<bool> result =
      Object::SetSuperProperty(&it, value, StoreOrigin::kMaybeKeyed);
  MAYBE_RETURN(result, ReadOnlyRoots(isolate).exception());
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

RUNTIME_FUNCTION(Runtime_CheckProxyHasTrapResult) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, target, 1);

  Maybe<bool> result = JSProxy::CheckHasTrap(isolate, name, target);
  if (!result.IsJust()) return ReadOnlyRoots(isolate).exception();
  return isolate->heap()->ToBoolean(result.FromJust());
}

}  // namespace internal
}  // namespace v8

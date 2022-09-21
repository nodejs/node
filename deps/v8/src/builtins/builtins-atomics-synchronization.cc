// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/objects/js-atomics-synchronization-inl.h"

namespace v8 {
namespace internal {

BUILTIN(AtomicsMutexConstructor) {
  DCHECK(v8_flags.harmony_struct);
  HandleScope scope(isolate);
  return *isolate->factory()->NewJSAtomicsMutex();
}

BUILTIN(AtomicsMutexLock) {
  DCHECK(v8_flags.harmony_struct);
  constexpr char method_name[] = "Atomics.Mutex.lock";
  HandleScope scope(isolate);

  Handle<Object> js_mutex_obj = args.atOrUndefined(isolate, 1);
  if (!js_mutex_obj->IsJSAtomicsMutex()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  method_name)));
  }
  Handle<JSAtomicsMutex> js_mutex = Handle<JSAtomicsMutex>::cast(js_mutex_obj);
  Handle<Object> run_under_lock = args.atOrUndefined(isolate, 2);
  if (!run_under_lock->IsCallable()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotCallable, run_under_lock));
  }

  // Like Atomics.wait, synchronous locking may block, and so is disallowed on
  // the main thread.
  //
  // This is not a recursive lock, so also throw if recursively locking.
  if (!isolate->allow_atomics_wait() || js_mutex->IsCurrentThreadOwner()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kAtomicsOperationNotAllowed,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  method_name)));
  }

  Handle<Object> result;
  {
    JSAtomicsMutex::LockGuard lock_guard(isolate, js_mutex);
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result,
        Execution::Call(isolate, run_under_lock,
                        isolate->factory()->undefined_value(), 0, nullptr));
  }

  return *result;
}

BUILTIN(AtomicsMutexTryLock) {
  DCHECK(v8_flags.harmony_struct);
  constexpr char method_name[] = "Atomics.Mutex.tryLock";
  HandleScope scope(isolate);

  Handle<Object> js_mutex_obj = args.atOrUndefined(isolate, 1);
  if (!js_mutex_obj->IsJSAtomicsMutex()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  method_name)));
  }
  Handle<JSAtomicsMutex> js_mutex = Handle<JSAtomicsMutex>::cast(js_mutex_obj);
  Handle<Object> run_under_lock = args.atOrUndefined(isolate, 2);
  if (!run_under_lock->IsCallable()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotCallable, run_under_lock));
  }

  JSAtomicsMutex::TryLockGuard try_lock_guard(isolate, js_mutex);
  if (try_lock_guard.locked()) {
    Handle<Object> result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result,
        Execution::Call(isolate, run_under_lock,
                        isolate->factory()->undefined_value(), 0, nullptr));
    return ReadOnlyRoots(isolate).true_value();
  }

  return ReadOnlyRoots(isolate).false_value();
}

BUILTIN(AtomicsConditionConstructor) {
  DCHECK(v8_flags.harmony_struct);
  HandleScope scope(isolate);
  return *isolate->factory()->NewJSAtomicsCondition();
}

BUILTIN(AtomicsConditionWait) {
  DCHECK(v8_flags.harmony_struct);
  constexpr char method_name[] = "Atomics.Condition.wait";
  HandleScope scope(isolate);

  Handle<Object> js_condition_obj = args.atOrUndefined(isolate, 1);
  Handle<Object> js_mutex_obj = args.atOrUndefined(isolate, 2);
  Handle<Object> timeout_obj = args.atOrUndefined(isolate, 3);
  if (!js_condition_obj->IsJSAtomicsCondition() ||
      !js_mutex_obj->IsJSAtomicsMutex()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  method_name)));
  }

  base::Optional<base::TimeDelta> timeout = base::nullopt;
  if (!timeout_obj->IsUndefined(isolate)) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, timeout_obj,
                                       Object::ToNumber(isolate, timeout_obj));
    double ms = timeout_obj->Number();
    if (!std::isnan(ms)) {
      if (ms < 0) ms = 0;
      if (ms <= static_cast<double>(std::numeric_limits<int64_t>::max())) {
        timeout = base::TimeDelta::FromMilliseconds(static_cast<int64_t>(ms));
      }
    }
  }

  if (!isolate->allow_atomics_wait()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kAtomicsOperationNotAllowed,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  method_name)));
  }

  Handle<JSAtomicsCondition> js_condition =
      Handle<JSAtomicsCondition>::cast(js_condition_obj);
  Handle<JSAtomicsMutex> js_mutex = Handle<JSAtomicsMutex>::cast(js_mutex_obj);

  if (!js_mutex->IsCurrentThreadOwner()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(MessageTemplate::kAtomicsMutexNotOwnedByCurrentThread));
  }

  return isolate->heap()->ToBoolean(
      JSAtomicsCondition::WaitFor(isolate, js_condition, js_mutex, timeout));
}

BUILTIN(AtomicsConditionNotify) {
  DCHECK(v8_flags.harmony_struct);
  constexpr char method_name[] = "Atomics.Condition.notify";
  HandleScope scope(isolate);

  Handle<Object> js_condition_obj = args.atOrUndefined(isolate, 1);
  Handle<Object> count_obj = args.atOrUndefined(isolate, 2);
  if (!js_condition_obj->IsJSAtomicsCondition()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  method_name)));
  }

  uint32_t count;
  if (count_obj->IsUndefined(isolate)) {
    count = JSAtomicsCondition::kAllWaiters;
  } else {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, count_obj,
                                       Object::ToInteger(isolate, count_obj));
    double count_double = count_obj->Number();
    if (count_double < 0) {
      count_double = 0;
    } else if (count_double > JSAtomicsCondition::kAllWaiters) {
      count_double = JSAtomicsCondition::kAllWaiters;
    }
    count = static_cast<uint32_t>(count_double);
  }

  Handle<JSAtomicsCondition> js_condition =
      Handle<JSAtomicsCondition>::cast(js_condition_obj);
  return *isolate->factory()->NewNumberFromUint(
      js_condition->Notify(isolate, count));
}

}  // namespace internal
}  // namespace v8

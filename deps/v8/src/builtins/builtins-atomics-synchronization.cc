// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/objects/js-atomics-synchronization-inl.h"

namespace v8 {
namespace internal {
namespace {

std::optional<base::TimeDelta> GetTimeoutDelta(
    DirectHandle<Object> timeout_obj) {
  double ms = Object::NumberValue(*timeout_obj);
  if (!std::isnan(ms)) {
    if (ms < 0) ms = 0;
    if (ms <= static_cast<double>(std::numeric_limits<int64_t>::max())) {
      return base::TimeDelta::FromMilliseconds(static_cast<int64_t>(ms));
    }
  }
  return std::nullopt;
}

}  // namespace

BUILTIN(AtomicsMutexConstructor) {
  DCHECK(v8_flags.harmony_struct);
  HandleScope scope(isolate);
  return *isolate->factory()->NewJSAtomicsMutex();
}

BUILTIN(AtomicsMutexLock) {
  DCHECK(v8_flags.harmony_struct);
  constexpr char method_name[] = "Atomics.Mutex.lock";
  HandleScope scope(isolate);

  DirectHandle<Object> js_mutex_obj = args.atOrUndefined(isolate, 1);
  if (!IsJSAtomicsMutex(*js_mutex_obj)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  method_name)));
  }
  DirectHandle<JSAtomicsMutex> js_mutex = Cast<JSAtomicsMutex>(js_mutex_obj);
  DirectHandle<Object> run_under_lock = args.atOrUndefined(isolate, 2);
  if (!IsCallable(*run_under_lock)) {
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

  DirectHandle<Object> result;
  {
    JSAtomicsMutex::LockGuard lock_guard(isolate, js_mutex);
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result,
        Execution::Call(isolate, run_under_lock,
                        isolate->factory()->undefined_value(), {}));
  }

  return *result;
}

BUILTIN(AtomicsMutexTryLock) {
  DCHECK(v8_flags.harmony_struct);
  constexpr char method_name[] = "Atomics.Mutex.tryLock";
  HandleScope scope(isolate);

  DirectHandle<Object> js_mutex_obj = args.atOrUndefined(isolate, 1);
  if (!IsJSAtomicsMutex(*js_mutex_obj)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  method_name)));
  }
  DirectHandle<JSAtomicsMutex> js_mutex = Cast<JSAtomicsMutex>(js_mutex_obj);
  DirectHandle<Object> run_under_lock = args.atOrUndefined(isolate, 2);
  if (!IsCallable(*run_under_lock)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotCallable, run_under_lock));
  }

  DirectHandle<Object> callback_result;
  bool success;
  {
    JSAtomicsMutex::TryLockGuard try_lock_guard(isolate, js_mutex);
    if (try_lock_guard.locked()) {
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, callback_result,
          Execution::Call(isolate, run_under_lock,
                          isolate->factory()->undefined_value(), {}));
      success = true;
    } else {
      callback_result = isolate->factory()->undefined_value();
      success = false;
    }
  }
  DirectHandle<JSObject> result =
      JSAtomicsMutex::CreateResultObject(isolate, callback_result, success);
  return *result;
}

BUILTIN(AtomicsMutexLockWithTimeout) {
  DCHECK(v8_flags.harmony_struct);
  constexpr char method_name[] = "Atomics.Mutex.lockWithTimeout";
  HandleScope scope(isolate);

  DirectHandle<Object> js_mutex_obj = args.atOrUndefined(isolate, 1);
  if (!IsJSAtomicsMutex(*js_mutex_obj)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  method_name)));
  }
  DirectHandle<JSAtomicsMutex> js_mutex = Cast<JSAtomicsMutex>(js_mutex_obj);
  DirectHandle<Object> run_under_lock = args.atOrUndefined(isolate, 2);
  if (!IsCallable(*run_under_lock)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kNotCallable, run_under_lock));
  }

  DirectHandle<Object> timeout_obj = args.atOrUndefined(isolate, 3);
  std::optional<base::TimeDelta> timeout;
  if (!IsNumber(*timeout_obj)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kIsNotNumber, timeout_obj,
                              Object::TypeOf(isolate, timeout_obj)));
  }
  timeout = GetTimeoutDelta(timeout_obj);

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

  DirectHandle<Object> callback_result;
  bool success;
  {
    JSAtomicsMutex::LockGuard lock_guard(isolate, js_mutex, timeout);
    if (V8_LIKELY(lock_guard.locked())) {
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, callback_result,
          Execution::Call(isolate, run_under_lock,
                          isolate->factory()->undefined_value(), {}));
      success = true;
    } else {
      callback_result = isolate->factory()->undefined_value();
      success = false;
    }
  }
  DirectHandle<JSObject> result =
      JSAtomicsMutex::CreateResultObject(isolate, callback_result, success);
  return *result;
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

  DirectHandle<Object> js_condition_obj = args.atOrUndefined(isolate, 1);
  DirectHandle<Object> js_mutex_obj = args.atOrUndefined(isolate, 2);
  DirectHandle<Object> timeout_obj = args.atOrUndefined(isolate, 3);
  if (!IsJSAtomicsCondition(*js_condition_obj) ||
      !IsJSAtomicsMutex(*js_mutex_obj)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  method_name)));
  }

  std::optional<base::TimeDelta> timeout = std::nullopt;
  if (!IsUndefined(*timeout_obj, isolate)) {
    if (!IsNumber(*timeout_obj)) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewTypeError(MessageTemplate::kIsNotNumber, timeout_obj,
                                Object::TypeOf(isolate, timeout_obj)));
    }
    timeout = GetTimeoutDelta(timeout_obj);
  }

  if (!isolate->allow_atomics_wait()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kAtomicsOperationNotAllowed,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  method_name)));
  }

  auto js_condition = Cast<JSAtomicsCondition>(js_condition_obj);
  auto js_mutex = Cast<JSAtomicsMutex>(js_mutex_obj);

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

  DirectHandle<Object> js_condition_obj = args.atOrUndefined(isolate, 1);
  DirectHandle<Object> count_obj = args.atOrUndefined(isolate, 2);
  if (!IsJSAtomicsCondition(*js_condition_obj)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  method_name)));
  }

  uint32_t count;
  if (IsUndefined(*count_obj, isolate)) {
    count = JSAtomicsCondition::kAllWaiters;
  } else {
    double count_double;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, count_double, Object::IntegerValue(isolate, count_obj));
    if (count_double <= 0) {
      return Smi::zero();
    } else if (count_double > JSAtomicsCondition::kAllWaiters) {
      count_double = JSAtomicsCondition::kAllWaiters;
    }
    count = static_cast<uint32_t>(count_double);
  }

  auto js_condition = Cast<JSAtomicsCondition>(js_condition_obj);
  return *isolate->factory()->NewNumberFromUint(
      JSAtomicsCondition::Notify(isolate, js_condition, count));
}

}  // namespace internal
}  // namespace v8

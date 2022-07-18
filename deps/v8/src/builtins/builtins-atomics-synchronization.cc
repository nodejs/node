// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/objects/js-atomics-synchronization-inl.h"

namespace v8 {
namespace internal {

BUILTIN(AtomicsMutexConstructor) {
  DCHECK(FLAG_harmony_struct);
  HandleScope scope(isolate);
  return *JSAtomicsMutex::Create(isolate);
}

BUILTIN(AtomicsMutexLock) {
  DCHECK(FLAG_harmony_struct);
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
    THROW_NEW_ERROR_RETURN_FAILURE(isolate,
                                   NewTypeError(MessageTemplate::kNotCallable));
  }

  // Like Atomics.wait, synchronous locking may block, and so is disallowed on
  // the main thread.
  //
  // This is not a recursive lock, so also throw if recursively locking.
  if (!isolate->allow_atomics_wait() || js_mutex->IsCurrentThreadOwner()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kAtomicsMutexLockNotAllowed));
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
  DCHECK(FLAG_harmony_struct);
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
    THROW_NEW_ERROR_RETURN_FAILURE(isolate,
                                   NewTypeError(MessageTemplate::kNotCallable));
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

}  // namespace internal
}  // namespace v8

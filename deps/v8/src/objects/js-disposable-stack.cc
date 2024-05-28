// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-maybe.h"
#include "src/execution/isolate.h"
#include "src/handles/handles.h"
#include "src/handles/maybe-handles.h"
#include "src/heap/factory.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/js-disposable-stack-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

// https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-disposeresources
Maybe<bool> JSDisposableStack::DisposeResources(
    Isolate* isolate, Handle<JSDisposableStack> disposable_stack,
    MaybeHandle<Object> maybe_original_error) {
  DCHECK(!IsUndefined(disposable_stack->stack()));
  DCHECK_NE(disposable_stack->state(), DisposableStackState::kDisposed);

  disposable_stack->set_state(DisposableStackState::kDisposed);

  Handle<FixedArray> stack(disposable_stack->stack(), isolate);

  int length = disposable_stack->length();

  MaybeHandle<Object> result;
  MaybeHandle<Object> maybe_error = maybe_original_error;
  Handle<Object> existing_error;

  // 1. For each element resource of
  // disposeCapability.[[DisposableResourceStack]], in reverse list order, do
  while (length > 0) {
    Tagged<Object> call_type = stack->get(--length);

    Tagged<Object> tagged_method = stack->get(--length);
    Handle<Object> method(tagged_method, isolate);

    Tagged<Object> tagged_value = stack->get(--length);
    Handle<Object> value(tagged_value, isolate);

    //  a. Let result be Completion(Dispose(resource.[[ResourceValue]],
    //  resource.[[Hint]], resource.[[DisposeMethod]])).
    auto call_type_case =
        static_cast<DisposeMethodCallType>(Smi::cast(call_type).value());
    switch (call_type_case) {
      case DisposeMethodCallType::kValueIsReceiver:
        result = Execution::Call(isolate, method, value, 0, nullptr);
        break;
      case DisposeMethodCallType::kValueIsArgument:
        Handle<Object> argv[] = {value};
        result = Execution::Call(
            isolate, method, ReadOnlyRoots(isolate).undefined_value_handle(), 1,
            argv);
        break;
    }

    //  b. If result is a throw completion, then
    if (result.is_null()) {
      DCHECK(isolate->has_exception());
      Handle<Object> current_error(isolate->exception(), isolate);
      isolate->clear_internal_exception();

      //   i. If completion is a throw completion, then
      if (maybe_error.ToHandle(&existing_error)) {
        //    1. Set result to result.[[Value]].
        //    2. Let suppressed be completion.[[Value]].
        //    3. Let error be a newly created SuppressedError object.
        //    4. Perform CreateNonEnumerableDataPropertyOrThrow(error, "error",
        //    result).
        //    5. Perform CreateNonEnumerableDataPropertyOrThrow(error,
        //    "suppressed", suppressed).
        //    6. Set completion to ThrowCompletion(error).
        maybe_error = isolate->factory()->NewSuppressedErrorAtDisposal(
            isolate, current_error, existing_error);

      } else {
        //   ii. Else,
        //    1. Set completion to result.
        maybe_error = current_error;
      }
    }
  }

  // 2. NOTE: After disposeCapability has been disposed, it will never be used
  // again. The contents of disposeCapability.[[DisposableResourceStack]] can be
  // discarded in implementations, such as by garbage collection, at this point.
  // 3. Set disposeCapability.[[DisposableResourceStack]] to a new empty List.
  disposable_stack->set_stack(ReadOnlyRoots(isolate).empty_fixed_array());
  disposable_stack->set_length(0);

  // 4. Return ? completion.
  if (maybe_error.ToHandle(&existing_error) &&
      !(maybe_error.equals(maybe_original_error))) {
    isolate->Throw(*existing_error);
    return Nothing<bool>();
  }
  return Just(true);
}

}  // namespace internal
}  // namespace v8

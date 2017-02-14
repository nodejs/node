// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"

#include "src/promise-utils.h"

namespace v8 {
namespace internal {

// ES#sec-promise-resolve-functions
// Promise Resolve Functions
BUILTIN(PromiseResolveClosure) {
  HandleScope scope(isolate);

  Handle<Context> context(isolate->context(), isolate);

  if (PromiseUtils::HasAlreadyVisited(context)) {
    return isolate->heap()->undefined_value();
  }

  PromiseUtils::SetAlreadyVisited(context);
  Handle<JSObject> promise = handle(PromiseUtils::GetPromise(context), isolate);
  Handle<Object> value = args.atOrUndefined(isolate, 1);

  MaybeHandle<Object> maybe_result;
  Handle<Object> argv[] = {promise, value};
  RETURN_FAILURE_ON_EXCEPTION(
      isolate, Execution::Call(isolate, isolate->promise_resolve(),
                               isolate->factory()->undefined_value(),
                               arraysize(argv), argv));
  return isolate->heap()->undefined_value();
}

// ES#sec-promise-reject-functions
// Promise Reject Functions
BUILTIN(PromiseRejectClosure) {
  HandleScope scope(isolate);

  Handle<Context> context(isolate->context(), isolate);

  if (PromiseUtils::HasAlreadyVisited(context)) {
    return isolate->heap()->undefined_value();
  }

  PromiseUtils::SetAlreadyVisited(context);
  Handle<Object> value = args.atOrUndefined(isolate, 1);
  Handle<JSObject> promise = handle(PromiseUtils::GetPromise(context), isolate);
  Handle<Object> debug_event =
      handle(PromiseUtils::GetDebugEvent(context), isolate);
  MaybeHandle<Object> maybe_result;
  Handle<Object> argv[] = {promise, value, debug_event};
  RETURN_FAILURE_ON_EXCEPTION(
      isolate, Execution::Call(isolate, isolate->promise_internal_reject(),
                               isolate->factory()->undefined_value(),
                               arraysize(argv), argv));
  return isolate->heap()->undefined_value();
}

// ES#sec-createresolvingfunctions
// CreateResolvingFunctions ( promise )
BUILTIN(CreateResolvingFunctions) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());

  Handle<JSObject> promise = args.at<JSObject>(1);
  Handle<Object> debug_event = args.at<Object>(2);
  Handle<JSFunction> resolve, reject;

  PromiseUtils::CreateResolvingFunctions(isolate, promise, debug_event,
                                         &resolve, &reject);

  Handle<FixedArray> result = isolate->factory()->NewFixedArray(2);
  result->set(0, *resolve);
  result->set(1, *reject);

  return *isolate->factory()->NewJSArrayWithElements(result, FAST_ELEMENTS, 2,
                                                     NOT_TENURED);
}

}  // namespace internal
}  // namespace v8

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let rejected_error;
function CustomPromise(executor) {
  executor(() => {}, (err) => { rejected_error = err; });
}

let captured_reject_closure;
CustomPromise.resolve = function(thenable) {
  return thenable;
};

const pending_thenables = [
  { then(resolve, reject) { captured_reject_closure = reject; } },
  { then() {} },
  { then() {} },
];

Promise.any.call(CustomPromise, pending_thenables);

// Store the reject element closure on a constant holder so that inlining
// invoke_reject_closure into call_reject_closure constant-folds the closure
// target and its initial FunctionContext.
const closure_holder = { reject: captured_reject_closure };

function invoke_reject_closure(closure, should_call) {
  if (should_call) closure('err');
}

function call_reject_closure(should_call) {
  invoke_reject_closure(closure_holder.reject, should_call);
}

// Make the call site inside invoke_reject_closure polymorphic before
// calling captured_reject_closure so Maglev/TurboFan compile the call without
// invoking captured_reject_closure during warmup.
%PrepareFunctionForOptimization(invoke_reject_closure);
invoke_reject_closure(() => {}, true);
invoke_reject_closure((x) => x, true);

%PrepareFunctionForOptimization(call_reject_closure);
call_reject_closure(false);
%OptimizeFunctionOnNextCall(call_reject_closure);
call_reject_closure(false);

// Repeated calls to the same reject element closure must be ignored after the
// first call and must not decrement remainingElementsCount to 0.
call_reject_closure(true);
call_reject_closure(true);
call_reject_closure(true);

assertEquals(undefined, rejected_error);

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --trace-allocation-stack-interval=1

// Printing the stack on every allocation during
// Deoptimizer::MaterializeHeapObjects() encounters reconstructed unoptimized
// frames whose escape-analyzed receiver or closure slots still hold
// `arguments_marker`.

function inlinedCallee(x) {
  // Deopts on a non-Smi input while `this` is still unmaterialized.
  return x + 1;
}

function callerWithVirtualReceiver(x) {
  // Escape-analyzed object passed as `this` to the inlined callee so the
  // deoptimized callee frame temporarily holds `arguments_marker` as receiver.
  let virtualReceiver = { a: 1, b: 2 };
  return inlinedCallee.call(virtualReceiver, x);
}

%PrepareFunctionForOptimization(inlinedCallee);
%PrepareFunctionForOptimization(callerWithVirtualReceiver);
callerWithVirtualReceiver(1);
callerWithVirtualReceiver(2);
%OptimizeFunctionOnNextCall(callerWithVirtualReceiver);
callerWithVirtualReceiver(3);
callerWithVirtualReceiver("deopt");

function callerWithVirtualClosure(x) {
  // Capturing `x` creates an escape-analyzed closure that is inlined into the
  // loop; deopting inside it leaves `arguments_marker` in the frame's function
  // slot while MaterializeHeapObjects allocates the closure and its context.
  let captured = x;
  let virtualClosure = (v) => v + captured;
  let sum = 0;
  for (let i = 0; i < 20; i++) {
    sum += virtualClosure(x);
  }
  return sum;
}

%PrepareFunctionForOptimization(callerWithVirtualClosure);
callerWithVirtualClosure(1);
callerWithVirtualClosure(2);
%OptimizeFunctionOnNextCall(callerWithVirtualClosure);
callerWithVirtualClosure(3);
callerWithVirtualClosure("deopt");

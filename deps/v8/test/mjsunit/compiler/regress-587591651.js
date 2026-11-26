// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const receiver = { a: 1 };
// Make receiver's map unstable via an outgoing transition.
const transition = { a: 1 };
transition.b = 2;

const forInObj = { x: 1 };
const hasOwn = Object.prototype.hasOwnProperty;

function foo(argList) {
  // Emit a CheckMaps(receiver) before the loop so MapInference inside the loop
  // finds receiver's map across the loop EffectPhi as unreliable.
  const a = receiver.a;
  let res = false;
  for (let name in forInObj) {
    // bound.apply(null, argList) emits a JSCall with invalid FeedbackSource
    // on the null/undefined branch, which unwraps to hasOwn(receiver, name).
    res = hasOwn.bind(receiver, name).apply(null, argList);
  }
  return res || a !== 1;
}

%PrepareFunctionForOptimization(foo);
assertFalse(foo(null));
assertFalse(foo(undefined));
%OptimizeFunctionOnNextCall(foo);
assertFalse(foo(null));
assertFalse(foo(undefined));

receiver.x = 2;
assertTrue(foo(null));

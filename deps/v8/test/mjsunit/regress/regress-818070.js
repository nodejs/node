// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --opt --no-always-opt --allow-natives-syntax

function f(a) {
  Math.imul(a);
}

x = { [Symbol.toPrimitive]: () => FAIL };
%PrepareFunctionForOptimization(f);
f(1);
f(1);
%OptimizeFunctionOnNextCall(f);
assertThrows(() => f(x), ReferenceError);

function f(a) {
  Math.pow(a);
}

x = { [Symbol.toPrimitive]: () => FAIL };
%PrepareFunctionForOptimization(f);
f(1);
f(1);
%OptimizeFunctionOnNextCall(f);
assertThrows(() => f(x), ReferenceError);

function f(a) {
  Math.atan2(a);
}

x = { [Symbol.toPrimitive]: () => FAIL };
%PrepareFunctionForOptimization(f);
f(1);
f(1);
%OptimizeFunctionOnNextCall(f);
assertThrows(() => f(x), ReferenceError);

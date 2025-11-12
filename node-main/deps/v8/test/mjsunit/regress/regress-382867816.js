// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


const v9 = new Int16Array(122);

function f20() {}
const prom = new Promise(f20);

function test(x) {
  new prom.constructor(x);
}

function foo(x) {
  %PrepareFunctionForOptimization(test);
  test(x);
  %OptimizeFunctionOnNextCall(test);
  return foo;
}

const c = foo(foo);
assertThrows(() => v9.find(c, {}), TypeError, "Promise resolver 0 is not a function");

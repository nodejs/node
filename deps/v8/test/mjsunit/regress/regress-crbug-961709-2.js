// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

function foo() {
  const a = [];
  a[0] = 1;
  return a[0];
};

function bar() {
  const a = new Array(10);
  a[0] = 1;
  return a[0];
};

Object.setPrototypeOf(Array.prototype, new Int8Array());
%PrepareFunctionForOptimization(foo);
assertEquals(undefined, foo());
assertEquals(undefined, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(undefined, foo());
assertOptimized(foo);

%PrepareFunctionForOptimization(bar);
assertEquals(undefined, bar());
assertEquals(undefined, bar());
%OptimizeFunctionOnNextCall(bar);
assertEquals(undefined, bar());
assertOptimized(bar);

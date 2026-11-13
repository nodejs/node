// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

globalThis.g = 1.5;

function foo(arr) {
  let x = arr.length + 2;
  globalThis.g = x;
  return arr.push(1, 2);
}

%PrepareFunctionForOptimization(foo);
let a1 = [1];
assertEquals(3, foo(a1));
assertEquals([1, 1, 2], a1);
assertEquals(3, foo([1]));

delete globalThis.g;
globalThis.g = 1.5;
globalThis.g = 2.5;

%OptimizeMaglevOnNextCall(foo);
let a2 = [1];
assertEquals(3, foo(a2));
assertEquals([1, 1, 2], a2);
assertEquals(3, globalThis.g);
assertUnoptimized(foo);

// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

// Test side effects on arguments evaluation.
(function() {
  const a = [];
  const bar = x => { a.push(x); return x; };
  const foo = x => a.push(bar(x), bar(x));
  %PrepareFunctionForOptimization(foo);
  foo(1);
  foo(2);
  %OptimizeFunctionOnNextCall(foo);
  foo(3);
  assertEquals([1,1,1,1, 2,2,2,2, 3,3,3,3], a);
})();

// Test invalidation on arguments evaluation.
(function() {
  let y = 1;
  const a = [];
  const bar = x => { a.push(y); return x; }
  const foo = x => a.push(bar(x), bar(x));
  %PrepareFunctionForOptimization(foo);
  foo(1);
  y = 2;
  foo(2);
  %OptimizeFunctionOnNextCall(foo);
  y = 3;
  foo(3);
  assertOptimized(foo);
  y = 4.4;
  foo(4);
  assertEquals([1,1,1,1, 2,2,2,2, 3,3,3,3, 4.4,4.4,4,4], a);
})();
(function() {
  let y = 1;
  const a = [0.5];
  const bar = x => { a.push(y); return x; }
  const foo = x => a.push(bar(x), bar(x));
  %PrepareFunctionForOptimization(foo);
  foo(1);
  y = 2;
  foo(2);
  %OptimizeFunctionOnNextCall(foo);
  y = 3;
  foo(3);
  assertOptimized(foo);
  y = '4';
  foo(4);
  assertEquals([0.5, 1,1,1,1, 2,2,2,2, 3,3,3,3, '4','4',4,4], a);
})();

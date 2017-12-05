// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

// Test elements transition from SMI to DOUBLE.
(function() {
  const a = [];
  const foo = (x, y) => a.push(x, y);
  foo(1, 2);
  foo(3, 4);
  %OptimizeFunctionOnNextCall(foo);
  foo(5, 6.6);
  assertEquals([1, 2, 3, 4, 5, 6.6], a);
})();
(function() {
  const a = [];
  const foo = (x, y) => a.push(x, y);
  foo(1, 2);
  foo(3, 4);
  %OptimizeFunctionOnNextCall(foo);
  foo(5.5, 6.6);
  assertEquals([1, 2, 3, 4, 5.5, 6.6], a);
})();

// Test elements transition from SMI to OBJECT.
(function() {
  const a = [];
  const foo = (x, y) => a.push(x, y);
  foo(1, 2);
  foo(3, 4);
  %OptimizeFunctionOnNextCall(foo);
  foo(5, '6');
  assertEquals([1, 2, 3, 4, 5, '6'], a);
})();
(function() {
  const a = [];
  const foo = (x, y) => a.push(x, y);
  foo(1, 2);
  foo(3, 4);
  %OptimizeFunctionOnNextCall(foo);
  foo('5', '6');
  assertEquals([1, 2, 3, 4, '5', '6'], a);
})();

// Test elements transition from DOUBLE to OBJECT.
(function() {
  const a = [0.5];
  const foo = (x, y) => a.push(x, y);
  foo(1, 2);
  foo(3, 4);
  %OptimizeFunctionOnNextCall(foo);
  foo(5, '6');
  assertEquals([0.5, 1, 2, 3, 4, 5, '6'], a);
})();
(function() {
  const a = [0.5];
  const foo = (x, y) => a.push(x, y);
  foo(1, 2);
  foo(3, 4);
  %OptimizeFunctionOnNextCall(foo);
  foo('5', '6');
  assertEquals([0.5, 1, 2, 3, 4, '5', '6'], a);
})();

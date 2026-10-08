// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

function myfun() {
  // Ensures myfun bytecode size > mycall.
  // So mycall is inlined first.
  for (let i = 0; i < 5; i++);
  return Math.sqrt;
}
function mycall(f, x) {
  return f(x);
}

function foo(x) {
  return mycall(myfun(), x);
}

%PrepareFunctionForOptimization(myfun);
%PrepareFunctionForOptimization(mycall);
%PrepareFunctionForOptimization(foo);

assertEquals(2, foo(4));
assertEquals(3, foo(9));

// Polymorphic feedback on f, otherwise we imemdiately call known node
// info based on the target feedback.
mycall(() => 0, 0, 0);

%OptimizeFunctionOnNextCall(foo);
assertEquals(2, foo(4));
assertEquals(3, foo(9));

// Math.sqrt was inlined during graph optimizer and able to speculate.
assertOptimized(foo);
assertEquals(NaN, foo({}));
assertUnoptimized(foo);

// Don't speculate now.
%OptimizeFunctionOnNextCall(foo);
assertEquals(2, foo(4));
assertEquals(3, foo(9));
assertEquals(NaN, foo({}));
assertOptimized(foo);

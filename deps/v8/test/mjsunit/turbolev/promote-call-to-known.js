// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

function add(x, y) { return x + y; }
function myfun() {
  // Ensures myfun bytecode size > mycall.
  // So mycall is inlined first.
  for (let i = 0; i < 5; i++);
  return add;
}
function mycall(f, x, y) { return f(x, y); }

function foo(x, y) { return mycall(myfun(), x, y); }

%PrepareFunctionForOptimization(add);
%PrepareFunctionForOptimization(myfun);
%PrepareFunctionForOptimization(mycall);
%PrepareFunctionForOptimization(foo);

assertEquals(3, foo(1, 2));
assertEquals(-1, foo(-3, 2));

// Polymorphic feedback on f, otherwise we imemdiately call known node
// info based on the target feedback.
mycall(() => 0, 0, 0);

%OptimizeFunctionOnNextCall(foo);
assertEquals(3, foo(1, 2));
assertEquals(-1, foo(-3, 2));

// add was added to the inliner during graph optimizer.
// If we inlined it properly, we should be speculate on smi feedback.
assertOptimized(foo);
assertEquals(9.7, foo(4.2, 5.5));
assertUnoptimized(foo);

// Re-optimizing. We are still able to inline add, but
// now it has a different feedback.
%OptimizeFunctionOnNextCall(foo);
assertEquals(3, foo(1, 2));
assertEquals(-1, foo(-3, 2));
assertEquals(9.7, foo(4.2, 5.5));
assertOptimized(foo);

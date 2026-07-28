// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

function bar(x) {
  // ~ 43 bytecodes
  return [1, 2, x, x, x, x, 7, 8, 9, 10];
}

function transition(o) {
  o.x = 1.11;
}
%NeverOptimizeFunction(transition)

function baz(o) {
  // ~ 67 bytecodes
  if (o.x == 553) {
    o.x + o.X + o.y + o.Y + o.z + o.Z;
  }
  return o.x == 1.11;
}

function foo() {
  let o = bar(); // Will be inlined first
  transition(o); // Will never be inlined
  return baz(o); // Will be inlined second and will thus see the
                 // InilinedAllocation for {o}, but won't see the transitions.
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(baz);
%PrepareFunctionForOptimization(foo);
assertEquals(true, foo());
assertEquals(true, foo());

%OptimizeFunctionOnNextCall(foo);
assertEquals(true, foo());
assertOptimized(foo);

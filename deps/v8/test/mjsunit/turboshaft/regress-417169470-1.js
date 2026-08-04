// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan

function foo(a, b, c) {
  a.x = 42;
  b.y = 99; // Transitioning store
  c.x = 17;
  return a.x;
}

// Using a constructor so that we don't go out of object when creating the .y
// property in foo.
function MyObject() { this.x = 27; }
let o1 = new MyObject();
let o2 = new MyObject();

%PrepareFunctionForOptimization(foo);
assertEquals(17, foo(o1, o1, o1));

%OptimizeFunctionOnNextCall(foo);
assertEquals(17, foo(o2, o2, o2));

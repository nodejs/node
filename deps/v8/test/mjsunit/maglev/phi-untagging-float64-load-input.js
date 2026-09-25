// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(c, o) {
  let phi = c ? o.x : 15.2;
  return phi + 2.7;
}

let o = { x : 42 };
o.x = 25;

%PrepareFunctionForOptimization(foo);
assertEquals(27.7, foo(true, o));
assertEquals(17.9, foo(false, o));

%OptimizeFunctionOnNextCall(foo);
assertEquals(27.7, foo(true, o));
assertEquals(17.9, foo(false, o));

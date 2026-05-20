// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

val = "hello";
function foo(i) {
  return val[i];
}
assertEquals(undefined, foo(8));
Object.prototype[4294967295] = "boom";
assertEquals("boom", foo(4294967295));
%PrepareFunctionForOptimization(foo);
assertEquals(undefined, foo(8));
assertEquals("boom", foo(4294967295));
%OptimizeFunctionOnNextCall(foo);
assertEquals(undefined, foo(8));
assertEquals("boom", foo(4294967295));

// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-do-expressions

(function DoTryCatchInsideBinop() {
  function f(a, b) {
    return a + do { try { throw "boom" } catch(e) { b } }
  }
  assertEquals(3, f(1, 2));
  assertEquals(3, f(1, 2));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(3, f(1, 2));
})();

(function DoTryCatchInsideCall() {
  function f(a, b) {
    return Math.max(a, do { try { throw a } catch(e) { e + b } })
  }
  assertEquals(3, f(1, 2));
  assertEquals(3, f(1, 2));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(3, f(1, 2));
})();

(function DoTryCatchInsideTry() {
  function f(a, b) {
    try { return do { try { throw a } catch(e) { e + b } } } catch(e) {}
  }
  assertEquals(3, f(1, 2));
  assertEquals(3, f(1, 2));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(3, f(1, 2));
})();

(function DoTryCatchInsideFinally() {
  function f(a, b) {
    try {} finally { return do { try { throw a } catch(e) { e + b } } }
  }
  assertEquals(3, f(1, 2));
  assertEquals(3, f(1, 2));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(3, f(1, 2));
})();

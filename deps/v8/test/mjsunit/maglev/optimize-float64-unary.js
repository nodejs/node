// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function testAbs() {
  function cst() { return -1.5; }
  function f() { return Math.abs(cst()); }
  %PrepareFunctionForOptimization(cst);
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeFunctionOnNextCall(f);
  assertEquals(1.5, f());
}
testAbs();

function testSqrt() {
  function cst() { return 2.25; }
  function f() { return Math.sqrt(cst()); }
  %PrepareFunctionForOptimization(cst);
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeFunctionOnNextCall(f);
  assertEquals(1.5, f());
}
testSqrt();

function testFloor() {
  function cst() { return 3.7; }
  function f() { return Math.floor(cst()); }
  %PrepareFunctionForOptimization(cst);
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeFunctionOnNextCall(f);
  assertEquals(3.0, f());
}
testFloor();

function testCeil() {
  function cst() { return 3.2; }
  function f() { return Math.ceil(cst()); }
  %PrepareFunctionForOptimization(cst);
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeFunctionOnNextCall(f);
  assertEquals(4.0, f());
}
testCeil();

function testCompare() {
  function cst() { return 5.5; }
  function f() { return 3.0 < cst(); }
  %PrepareFunctionForOptimization(cst);
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeFunctionOnNextCall(f);
  assertTrue(f());
}
testCompare();

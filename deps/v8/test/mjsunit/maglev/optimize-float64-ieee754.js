// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function testAcos() {
  function cst() { return 0.5; }
  function f() { return Math.acos(cst()); }
  %PrepareFunctionForOptimization(cst);
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeMaglevOnNextCall(f);
  assertEquals(Math.acos(0.5), f());
}
testAcos();

function testAsin() {
  function cst() { return 0.5; }
  function f() { return Math.asin(cst()); }
  %PrepareFunctionForOptimization(cst);
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeMaglevOnNextCall(f);
  assertEquals(Math.asin(0.5), f());
}
testAsin();

function testAtan() {
  function cst() { return 1; }
  function f() { return Math.atan(cst()); }
  %PrepareFunctionForOptimization(cst);
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeMaglevOnNextCall(f);
  assertEquals(Math.atan(1.0), f());
}
testAtan();

function testCbrt() {
  function cst() { return 8.0; }
  function f() { return Math.cbrt(cst()); }
  %PrepareFunctionForOptimization(cst);
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeMaglevOnNextCall(f);
  assertEquals(2.0, f());
}
testCbrt();

function testCos() {
  function f() { return Math.cos(Math.PI); }
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeMaglevOnNextCall(f);
  assertEquals(-1.0, f());
}
testCos();

function testExp() {
  function cst() { return 0; }
  function f() { return Math.exp(cst()); }
  %PrepareFunctionForOptimization(cst);
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeMaglevOnNextCall(f);
  assertEquals(1.0, f());
}
testExp();

function testLog() {
  function cst() { return 1; }
  function f() { return Math.log(cst()); }
  %PrepareFunctionForOptimization(cst);
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeMaglevOnNextCall(f);
  assertEquals(0.0, f());
}
testLog();

function testSin() {
  function f() { return Math.sin(Math.PI / 2); }
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeMaglevOnNextCall(f);
  assertEquals(1.0, f());
}
testSin();

function testTan() {
  function cst() { return 0; }
  function f() { return Math.tan(cst()); }
  %PrepareFunctionForOptimization(cst);
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeMaglevOnNextCall(f);
  assertEquals(0.0, f());
}
testTan();

function testAtan2() {
  function cst() { return 1; }
  function f() { return Math.atan2(cst(), cst()); }
  %PrepareFunctionForOptimization(cst);
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeMaglevOnNextCall(f);
  assertEquals(Math.atan2(1, 1), f());
}
testAtan2();

function testPow() {
  function cst() { return 2; }
  function f() { return Math.pow(cst(), 3); }
  %PrepareFunctionForOptimization(cst);
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeMaglevOnNextCall(f);
  assertEquals(8.0, f());
}
testPow();

function testClz32Int32() {
  function cst() { return 1; }
  function f() { return Math.clz32(cst()); }
  %PrepareFunctionForOptimization(cst);
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeMaglevOnNextCall(f);
  assertEquals(31, f());
}
testClz32Int32();

function testClz32Float64() {
  function cst() { return 0.5; }
  function f() { return Math.clz32(cst()); }
  %PrepareFunctionForOptimization(cst);
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeMaglevOnNextCall(f);
  assertEquals(32, f()); // ToUint32(0.5) is 0, clz32(0) is 32
}
testClz32Float64();

function testClz32Float64Positive() {
  function cst() { return 1.5; }
  function f() { return Math.clz32(cst()); }
  %PrepareFunctionForOptimization(cst);
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeMaglevOnNextCall(f);
  assertEquals(31, f()); // ToUint32(1.5) is 1, clz32(1) is 31
}
testClz32Float64Positive();

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// === Test 1: Basic -0 miscompilation ===
function test1(a, b) {
  if (a >= 0 && a <= 10 && b >= -5 && b <= -1) {
    return 1 / (a * b);  // 0 * -1 = -0 → 1/-0 = -Infinity
  }
  return 0;
}
%PrepareFunctionForOptimization(test1);
for (let i = 0; i < 100; i++) test1(i % 10 + 1, -(i % 5 + 1));
%OptimizeFunctionOnNextCall(test1);
test1(5, -3);
assertEquals(-Infinity, test1(0, -1));

// === Test 2: Wrong branch execution ===
function test2(a, b) {
  if (a >= 0 && a <= 10 && b >= -5 && b <= -1) {
    return (1 / (a * b) < 0) ? "correct" : "wrong";
  }
  return "out";
}
%PrepareFunctionForOptimization(test2);
for (let i = 0; i < 100; i++) test2(i % 10 + 1, -(i % 5 + 1));
%OptimizeFunctionOnNextCall(test2);
test2(5, -3);
assertEquals("correct", test2(0, -1));

// === Test 3: Object.is(-0) broken ===
function test3(a, b) {
  if (a >= 0 && a <= 10 && b >= -5 && b <= -1) {
    return Object.is(a * b, -0);
  }
  return null;
}
%PrepareFunctionForOptimization(test3);
for (let i = 0; i < 100; i++) test3(i % 10 + 1, -(i % 5 + 1));
%OptimizeFunctionOnNextCall(test3);
test3(5, -3);
assertEquals(true, test3(0, -1));

// === Test 4: Float64Array sign bit corruption ===
function test4(a, b) {
  if (a >= 0 && a <= 10 && b >= -5 && b <= -1) {
    const buf = new ArrayBuffer(8);
    const view = new DataView(buf);

    // Write value in little-endian form
    view.setFloat64(0, a * b, true);
    return view.getUint8(7); // Sign byte
  }
  return -1;
}
%PrepareFunctionForOptimization(test4);
for (let i = 0; i < 100; i++) test4(i % 10 + 1, -(i % 5 + 1));
%OptimizeFunctionOnNextCall(test4);
test4(5, -3);
assertEquals(0x80, test4(0, -1));

// === Test 5: Index amplification (0 → 1000) ===
function test5(a, b) {
  if (a >= 0 && a <= 10 && b >= -5 && b <= -1) {
    return ((1 / (a * b) > 0) | 0) * 1000;
  }
  return -1;
}
%PrepareFunctionForOptimization(test5);
for (let i = 0; i < 100; i++) test5(i % 10 + 1, -(i % 5 + 1));
%OptimizeFunctionOnNextCall(test5);
test5(5, -3);
assertEquals(0, test5(0, -1));

// === Test 6: Permanent miscompilation (5 recompilation cycles) ===
function test6(a, b) {
  if (a >= 0 && a <= 10 && b >= -5 && b <= -1) {
    return Object.is(a * b, -0) ? "neg" : "pos";
  }
  return "out";
}
let allCorrect = true;
for (let trial = 0; trial < 5; trial++) {
  %PrepareFunctionForOptimization(test6);
  for (let i = 0; i < 100; i++) test6(i % 10 + 1, -(i % 5 + 1));
  %OptimizeFunctionOnNextCall(test6);
  test6(5, -3);
  if (test6(0, -1) !== "neg") allCorrect = false;
  %DeoptimizeFunction(test6);
}
assertEquals(true, allCorrect);

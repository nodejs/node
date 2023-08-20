// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test(expected, f) {
  %PrepareFunctionForOptimization(f);
  assertEquals(expected, f());
  assertEquals(expected, f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(expected, f());
  assertEquals(expected, f());
}

function testThrows(f) {
  %PrepareFunctionForOptimization(f);
  assertThrows(f);
  assertThrows(f);
  %OptimizeFunctionOnNextCall(f);
  assertThrows(f);
  assertThrows(f);
}

// --- Constant case.
a = 11;

function f1() { return a; }
test(11, f1);

delete a;

testThrows(f1);


// --- SMI case.

b = 11;
b = 12;
b = 13;

function f2() { return b; }
test(13, f2);

delete b;

testThrows(f2);


// --- double case.

c = 11;
c = 12.25;
c = 13.25;

function f3() { return c; }
test(13.25, f3);

delete c;

testThrows(f3);


// --- tagged case.

d = 11;
d = 12.25;
d = "hello";

function f4() { return d; }
test("hello", f4);

delete d;

testThrows(f4);

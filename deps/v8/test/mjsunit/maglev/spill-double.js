// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// This tests that we can lazy deopt with a double spilled.

const value = 2.2;

function test(x) {
  return x;
}

function g(x) {
  assertEquals(value + 1, x);
}

function f(b, a) {
  var x = a + 1;
  if (test(b)) { // This forces x to be spilled.
    g(x); // When we lazy deopt, we call g with the materialized
          // value from the stack.
  }
  return x + 1;
}

%PrepareFunctionForOptimization(f);
assertEquals(4.2, f(false, value));

%OptimizeMaglevOnNextCall(f);
assertEquals(4.2, f(false, value));
assertTrue(isMaglevved(f));

// We should deopt here.
assertEquals(4.2, f(true, value));
assertFalse(isMaglevved(f));

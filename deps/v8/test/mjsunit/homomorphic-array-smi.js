// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --homomorphic-ic --homomorphic-ic-count=8 --turbofan

function load_length(a) {
  return a.length;
}

class A1 extends Array {}
class A2 extends Array {}
class A3 extends Array {}
class A4 extends Array {}
class A5 extends Array {}

let a1 = new A1();
let a2 = new A2();
let a3 = new A3();
let a4 = new A4();
let a5 = new A5();

%PrepareFunctionForOptimization(load_length);

// Warm up with all 5 maps
for (let i = 0; i < 20; i++) {
  load_length(a1);
  load_length(a2);
  load_length(a3);
  load_length(a4);
  load_length(a5);
}

// Assert that the feedback is homomorphic
let fb = %GetFeedback(load_length);
if (fb !== undefined) {
  assertEquals(1, fb.length);
  assertEquals("LoadProperty", fb[0][0])
  assertContains("HOMOMORPHIC", fb[0][1]);
}

%OptimizeFunctionOnNextCall(load_length);

// Test return values for arrays
assertEquals(0, load_length(a1));
assertEquals(0, load_length(a2));
assertEquals(0, load_length(a3));
assertEquals(0, load_length(a4));
assertEquals(0, load_length(a5));

assertOptimized(load_length);

// Pass a Smi. This should deopt and return undefined (if fixed).
assertEquals(undefined, load_length(42));

// Check if it deopted.
assertUnoptimized(load_length);

print("Test passed!");

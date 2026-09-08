// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --homomorphic-ic --homomorphic-ic-count=8 --turbofan

function load_length(a) {
  return a.length;
}

const arrays = [];
for (let i = 0; i < 11; i++) {
  const A = class extends Array {};
  arrays.push(new A());
}

%PrepareFunctionForOptimization(load_length);

// Warm up with all 11 maps
for (let i = 0; i < 20; i++) {
  for (const a of arrays) {
    load_length(a);
  }
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
for (const a of arrays) {
  assertEquals(0, load_length(a));
}

assertOptimized(load_length);

// Pass a Smi. This should deopt and return undefined (if fixed).
assertEquals(undefined, load_length(42));

// Check if it deopted.
assertUnoptimized(load_length);

print("Test passed!");

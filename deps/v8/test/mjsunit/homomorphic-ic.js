// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --homomorphic-ic --homomorphic-ic-count=8 --turbofan

function load(o) {
  return o.x;
}

let objects = [];
for (let i = 0; i < 10; i++) {
  // Create functions with different maps but same shape
  let F = new Function("x", "this.x = x;");
  objects.push(new F(i));
}

// Warm up
%PrepareFunctionForOptimization(load);
for (let i = 0; i < 10; i++) {
  load(objects[i]);
}

// Print feedback and assert homomorphic
let fb = %GetFeedback(load);
if (fb !== undefined) {
  assertEquals(1, fb.length);
  assertEquals("LoadProperty", fb[0][0])
  assertContains("HOMOMORPHIC", fb[0][1]);
}

// Optimize
%OptimizeFunctionOnNextCall(load);
load(objects[0]);
assertOptimized(load);

// Check result
for (let i = 0; i < 10; i++) {
  assertEquals(i, load(objects[i]));
  assertOptimized(load);
}

print("Test passed!");

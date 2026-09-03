// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-lazy-feedback-allocation

// Test that Maglev constant-folds TestUndetectable (obj == null) to false
// for JSReceivers when the NoUndetectableObjects protector is intact.

function test_undetectable(obj) {
  return obj == null;
}

// Warm up with regular objects.
%PrepareFunctionForOptimization(test_undetectable);
assertEquals(false, test_undetectable({}));
assertEquals(false, test_undetectable([]));

// Optimize with Maglev.
%OptimizeMaglevOnNextCall(test_undetectable);
assertEquals(false, test_undetectable({}));

// Instantiating an undetectable object invalidates the NoUndetectableObjects
// protector cell.
const undetectable = %GetUndetectable();
assertEquals(true, undetectable == null);

// Re-optimize after protector invalidation; now Maglev should not constant-fold
// to false, and should correctly identify the undetectable object.
%PrepareFunctionForOptimization(test_undetectable);
%OptimizeMaglevOnNextCall(test_undetectable);
assertEquals(true, test_undetectable(undetectable));

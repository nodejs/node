// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-turbofan --no-stress-maglev
// Flags: --no-optimize-maglev-optimizes-to-turbofan
// Flags: --for-of-optimization

function foo(iterable) {
  let sum = 0;
  for (var i of iterable) {
    sum += i;
  }
  return sum;
}

%PrepareFunctionForOptimization(foo);

let smiArray = [1, 2, 3]; // PACKED_SMI_ELEMENTS
let doubleArray = [1.5, 2.5, 3.5]; // PACKED_DOUBLE_ELEMENTS

// Warmup with both to trigger polymorphic feedback (size 2)
foo(smiArray);
foo(doubleArray);
foo(smiArray);
foo(doubleArray);

%OptimizeMaglevOnNextCall(foo);
assertEquals(6, foo(smiArray));

assertTrue(isMaglevved(foo));

// Run double array. It should NOT deoptimize now because we
// support polymorphic feedback!
assertEquals(7.5, foo(doubleArray));
assertTrue(isMaglevved(foo));

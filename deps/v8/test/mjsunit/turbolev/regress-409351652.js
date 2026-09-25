// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function test() {
  function f4(a5) {
    a5[11]; // Polymorphic: HOLEY_ELEMENTS + FLOAT64ELEMENTS
  }
  %PrepareFunctionForOptimization(f4);
  f4(Int16Array); // Inlined here; Int16Array is just something which is not a TypedArray
  const v12 = new Float64Array(16);
  f4(v12);
}
%PrepareFunctionForOptimization(test);

test();
%OptimizeFunctionOnNextCall(test);
test();

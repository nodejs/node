// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

function main() {
  function f0(v2, v3) {
    // TransitionElementsKindOrCheckMap: PACKED_SMI -> HOLEY_DOUBLE_ELEMENTS
    var v4 = v3[0];

    // TransitionElementsKindOrCheckMap: HOLEY_DOUBLE_ELEMENTS -> HOLEY_ELEMENTS
    var v5 = v2[0];

    // If v2 == v3, v3 doesn't have HOLEY_DOUBLE_ELEMENTS anymore.
    Array.prototype.indexOf.call(v3);
  }
  %PrepareFunctionForOptimization(f0);

  const holey = new Array(1);
  holey[0] = 'tagged'; // HOLEY_ELEMENTS
  f0(holey, [1]);

  const holey_double = new Array(1);
  holey_double[0] = 0.1; // HOLEY_DOUBLE_ELEMENTS

  %OptimizeFunctionOnNextCall(f0);
  f0(holey_double, holey_double);
}
main();
main();

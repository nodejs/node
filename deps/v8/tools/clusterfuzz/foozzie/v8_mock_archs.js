// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is intended for permanent JS behavior changes for mocking out
// non-deterministic behavior. For temporary suppressions, please refer to
// v8_suppressions.js.
// This mocks only architecture specific differences. Refer to v8_mocks.js
// for the general case.
// This file is loaded before each correctness test cases and won't get
// minimized.

// Mock Math function due to precision differences between 32 and 64 bits.
// Math.pow:
// https://crbug.com/380147861
// https://crbug.com/380322452
// https://crbug.com/381129314
// https://crbug.com/417090728
// Math.tanh:
// https://crbug.com/493841436
(function() {
  const origNumber = Number;
  const origToExponential = Number.prototype.toExponential;

  const origMathPow = Math.pow;
  Math.pow = function(a, b) {
    let result = origMathPow(a, b);
    return origNumber(origToExponential.call(result, 10));
  }

  const origMathTanh = Math.tanh;
  Math.tanh = function(a) {
    let result = origMathTanh(a);
    return origNumber(origToExponential.call(result, 10));
  }
})();

// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-inline-array-builtins --turbofan
// Flags: --no-always-turbofan

const arr = [];
const arrHoley = [];
arrHoley[10000] = 42;

let caught;
function testArrayAtWithTryCatch(a, idx) {
  try {
    return a.at(idx);
  } catch (e) { caught = e; }
}

%PrepareFunctionForOptimization(testArrayAtWithTryCatch);
testArrayAtWithTryCatch([]);
testArrayAtWithTryCatch(arrHoley);

Object.defineProperty(arr, 1, {
  get: function () {
    throw "should be caught";
  }
});

%OptimizeFunctionOnNextCall(testArrayAtWithTryCatch);
testArrayAtWithTryCatch(arr, 1);
assertEquals("should be caught", caught);

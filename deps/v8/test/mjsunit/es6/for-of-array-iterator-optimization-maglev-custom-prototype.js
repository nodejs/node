// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-stress-maglev
// Flags: --no-optimize-maglev-optimizes-to-turbofan
// Flags: --for-of-optimization

function testForOf(arr) {
  let results = [];
  let resultsIx = 0;
  for (var i of arr) {
    results[resultsIx++] = i;
  }
  return results;
}

function wrapper() {
  let proto = Object.create(Array.prototype);
  proto[1] = 'element';
  let arr = [1, , 3];
  Object.setPrototypeOf(arr, proto);
  return testForOf(arr);
}

%PrepareFunctionForOptimization(wrapper);
%PrepareFunctionForOptimization(testForOf);

wrapper();
wrapper();

%OptimizeMaglevOnNextCall(wrapper);
let res = wrapper();
assertTrue(isMaglevved(wrapper));

assertEquals([1, 'element', 3], res);

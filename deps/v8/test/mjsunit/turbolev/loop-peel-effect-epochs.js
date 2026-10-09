// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

let shrink = false;
function callback(array, iteration) {
  if (shrink && iteration === 1) array.length = 2;
}
%NeverOptimizeFunction(callback);

function sum(array, n, callback) {
  const at = Array.prototype.at;
  let result = 0;
  // This loop aligns the effect epochs at the second loop header.
  for (let i = 0; i < 3; ++i) {
    callback(null, 9);
    callback(null, 9);
  }
  for (let i = 0; i < n; ++i) {
    if (i === 0) result += array.length;
    result += at.call(array, 3);
    callback(array, i);
    result += at.call(array, 0);
    result += array[0];
  }
  return result;
}

const array = [11, 12, 13, 14];
%PrepareFunctionForOptimization(sum);
assertEquals(148, sum(array, 4, callback));
assertEquals(148, sum(array, 4, callback));
%OptimizeFunctionOnNextCall(sum);
shrink = true;
assertTrue(Number.isNaN(sum(array, 4, callback)));

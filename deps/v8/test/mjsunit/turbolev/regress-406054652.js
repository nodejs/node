// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev --turbofan

let values = [];

function reset() {
  values = [];
}
function mapper(value, ix, thisArg) {
  values.push(value);
  thisArg.length = 2;
}
%PrepareFunctionForOptimization(mapper);

function testForEach() {
  reset();
  [1, 2, 3].forEach(mapper);
  assertEquals([1, 2], values);
}
%PrepareFunctionForOptimization(testForEach);

testForEach();

%OptimizeFunctionOnNextCall(testForEach);
testForEach();

function testMap() {
  reset();
  [1, 2, 3].map(mapper);
  assertEquals([1, 2], values);
}
%PrepareFunctionForOptimization(testMap);

testMap();

%OptimizeFunctionOnNextCall(testMap);
testMap();

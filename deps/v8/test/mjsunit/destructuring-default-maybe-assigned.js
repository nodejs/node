// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function testLetArrayDefault(input) {
  let [x = 10, [y = 20] = []] = input;
  function read() {
    return x + y;
  }
  return read;
}

function testLetObjectDefault(input) {
  let {x = 10, a: y = 20, b: {z = 30} = {}} = input;
  function read() {
    return x + y + z;
  }
  return read;
}

function testVarDefault(input) {
  var [x = 10] = input;
  var {y = 20, a: z = 30} = input[1] || {};
  function read() {
    return x + y + z;
  }
  return read;
}

function testCatchDefault(input) {
  try {
    throw input;
  } catch ([x = 10, {y = 20, a: z = 30} = {}]) {
    return function read() {
      return x + y + z;
    };
  }
}

function testLetDefaultMutatingEarlierBinding(input) {
  let [x = 10, y = (x = 40)] = input;
  function read() {
    return x + y;
  }
  return read;
}

function testVarDefaultSelfAssignment(input) {
  var [x = (x = 42)] = input;
  function read() {
    return x;
  }
  return read;
}

%PrepareFunctionForOptimization(testLetArrayDefault);
%PrepareFunctionForOptimization(testLetObjectDefault);
%PrepareFunctionForOptimization(testVarDefault);
%PrepareFunctionForOptimization(testCatchDefault);
%PrepareFunctionForOptimization(testLetDefaultMutatingEarlierBinding);
%PrepareFunctionForOptimization(testVarDefaultSelfAssignment);

for (let i = 0; i < 3; ++i) {
  assertEquals(30, testLetArrayDefault([])());
  assertEquals(3, testLetArrayDefault([1, [2]])());

  assertEquals(60, testLetObjectDefault({})());
  assertEquals(6, testLetObjectDefault({x: 1, a: 2, b: {z: 3}})());

  assertEquals(60, testVarDefault([])());
  assertEquals(6, testVarDefault([1, {y: 2, a: 3}])());

  assertEquals(60, testCatchDefault([])());
  assertEquals(6, testCatchDefault([1, {y: 2, a: 3}])());

  assertEquals(80, testLetDefaultMutatingEarlierBinding([])());
  assertEquals(5, testLetDefaultMutatingEarlierBinding([2, 3])());

  assertEquals(42, testVarDefaultSelfAssignment([])());
  assertEquals(7, testVarDefaultSelfAssignment([7])());
}

%OptimizeFunctionOnNextCall(testLetArrayDefault);
%OptimizeFunctionOnNextCall(testLetObjectDefault);
%OptimizeFunctionOnNextCall(testVarDefault);
%OptimizeFunctionOnNextCall(testCatchDefault);
%OptimizeFunctionOnNextCall(testLetDefaultMutatingEarlierBinding);
%OptimizeFunctionOnNextCall(testVarDefaultSelfAssignment);

assertEquals(30, testLetArrayDefault([])());
assertEquals(3, testLetArrayDefault([1, [2]])());

assertEquals(60, testLetObjectDefault({})());
assertEquals(6, testLetObjectDefault({x: 1, a: 2, b: {z: 3}})());

assertEquals(60, testVarDefault([])());
assertEquals(6, testVarDefault([1, {y: 2, a: 3}])());

assertEquals(60, testCatchDefault([])());
assertEquals(6, testCatchDefault([1, {y: 2, a: 3}])());

assertEquals(80, testLetDefaultMutatingEarlierBinding([])());
assertEquals(5, testLetDefaultMutatingEarlierBinding([2, 3])());

assertEquals(42, testVarDefaultSelfAssignment([])());
assertEquals(7, testVarDefaultSelfAssignment([7])());

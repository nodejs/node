// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function testPatternAssignment() {
  let obj = { prop: 0 };
  let arr = [0];
  let idx = 0;
  let commaVar = 10;
  let callArg = 20;
  let arrayElem = 30;
  let assignedObj;
  let a = 0, b = 0, c = 0, d = 0;

  let readCaptured = () => {
    return obj.prop + arr[idx] + commaVar + callArg + arrayElem +
           a + b + c + d + (assignedObj === obj ? 100 : 0);
  };

  function id(x, y) { return y; }

  // 1. Comma expression before destructuring assignment.
  (commaVar, [a] = [1]);
  // 2. Call arguments before destructuring assignment.
  id(callArg, [b] = [2]);
  // 3. Array element before destructuring assignment.
  [arrayElem, [c] = [3]];
  // 4. Property and element targets inside destructuring assignment,
  //    plus a real assignment inside a property target's receiver.
  [obj.prop, arr[idx], (assignedObj = obj).prop2, d] = [4, 5, 6, 7];

  return readCaptured();
}

%PrepareFunctionForOptimization(testPatternAssignment);
assertEquals(10 + 20 + 30 + 4 + 5 + 1 + 2 + 3 + 7 + 100, testPatternAssignment());
assertEquals(10 + 20 + 30 + 4 + 5 + 1 + 2 + 3 + 7 + 100, testPatternAssignment());
%OptimizeFunctionOnNextCall(testPatternAssignment);
assertEquals(10 + 20 + 30 + 4 + 5 + 1 + 2 + 3 + 7 + 100, testPatternAssignment());

// Also test for-of destructuring with property targets and inner assignments.
function testForOfPattern() {
  let obj = { x: 0 };
  let target;
  let val = 0;
  let read = () => obj.x + val + (target === obj ? 50 : 0);
  for ([obj.x, (target = obj).y, val] of [[10, 20, 30]]) {}
  return read();
}

%PrepareFunctionForOptimization(testForOfPattern);
assertEquals(90, testForOfPattern());
%OptimizeFunctionOnNextCall(testForOfPattern);
assertEquals(90, testForOfPattern());

assertThrows(() => eval("let [a()] = [];"), SyntaxError);
assertThrows(() => eval("function f([a()]) {}"), SyntaxError);

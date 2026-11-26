// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Conditional expressions in test context.
function cond(a, b, i) {
  let r = 0;
  if (a < b ? i <= b : i >= b) r |= 1;
  while (a < b ? i < b : i > b) {
    i += a < b ? 1 : -1;
    r += 2;
  }
  if (!(a ? b : i)) r |= 128;
  return r;
}

function testCond() {
  assertEquals(7, cond(0, 3, 0));
  assertEquals(135, cond(3, 0, 3));
  assertEquals(129, cond(0, 0, 0));
}

%PrepareFunctionForOptimization(cond);
testCond();
testCond();
%OptimizeMaglevOnNextCall(cond);
testCond();

// Ensure hole checks are not elided across conditional arms in test context.
function testTDZ(c, ready) {
  if (!ready) {
    if (c ? x : 1) return x;
    return x;
  }
  let x = 42;
  return (c ? x : x) ? x : 0;
}

function runTDZ() {
  assertThrows(() => testTDZ(true, false), ReferenceError);
  assertThrows(() => testTDZ(false, false), ReferenceError);
  assertEquals(42, testTDZ(true, true));
  assertEquals(42, testTDZ(false, true));
}

%PrepareFunctionForOptimization(testTDZ);
runTDZ();
runTDZ();
%OptimizeMaglevOnNextCall(testTDZ);
runTDZ();

// Constant-condition ternaries in test context.
function constCond(a, b, c) {
  let r = 0;
  if (true ? a < b : c) r |= 1;
  if (false ? c : a < b) r |= 2;
  if (true ? (a && b) : c) r |= 4;
  if (!(true ? a < b : c)) r |= 8;
  return r;
}

function testConstCond() {
  assertEquals(3, constCond(0, 1, 0));
  assertEquals(7, constCond(1, 2, 0));
  assertEquals(12, constCond(2, 1, 0));
}

%PrepareFunctionForOptimization(constCond);
testConstCond();
testConstCond();
%OptimizeMaglevOnNextCall(constCond);
testConstCond();

// Chained ternaries in test context.
function condChain(c1, c2, a, b, d) {
  let r = 0;
  if (c1 ? a < b : c2 ? b < d : a < d) r |= 1;
  if (!(c1 ? a : c2 ? b : d)) r |= 2;
  if (c1 ? !a : c2 ? !b : !d) r |= 4;
  if (c1 ? a : true ? b < d : a) r |= 8;
  if (false ? a : c2 ? b < d : a < d) r |= 16;
  return r;
}

function testCondChain() {
  assertEquals(9, condChain(true, false, 1, 2, 0));
  assertEquals(25, condChain(false, true, 0, 1, 2));
  assertEquals(6, condChain(false, false, 1, 1, 0));
  assertEquals(17, condChain(false, false, 0, 2, 1));
}

%PrepareFunctionForOptimization(condChain);
testCondChain();
testCondChain();
%OptimizeMaglevOnNextCall(condChain);
testCondChain();

// Ensure hole checks are not elided across chained conditional arms.
function testChainTDZ(c1, c2, ready) {
  if (!ready) {
    if (c1 ? x : c2 ? x : 1) return x;
    return x;
  }
  let x = 42;
  return (c1 ? x : c2 ? x : x) ? x : 0;
}

function runChainTDZ() {
  assertThrows(() => testChainTDZ(true, false, false), ReferenceError);
  assertThrows(() => testChainTDZ(false, true, false), ReferenceError);
  assertThrows(() => testChainTDZ(false, false, false), ReferenceError);
  assertEquals(42, testChainTDZ(true, false, true));
  assertEquals(42, testChainTDZ(false, true, true));
  assertEquals(42, testChainTDZ(false, false, true));
}

%PrepareFunctionForOptimization(testChainTDZ);
runChainTDZ();
runChainTDZ();
%OptimizeMaglevOnNextCall(testChainTDZ);
runChainTDZ();

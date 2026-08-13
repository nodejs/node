// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function getNormalGeneratorObject() {
  return (function*(){ yield 42; })();
}

const next = Object.getPrototypeOf(function*(){}).prototype.next;

// 1. Test normal generator (should pass)
function testNormal(g) {
  return next.call(g);
}
%PrepareFunctionForOptimization(testNormal);
assertEquals({value: 42, done: false}, testNormal(getNormalGeneratorObject()));
assertEquals({value: 42, done: false}, testNormal(getNormalGeneratorObject()));
%OptimizeMaglevOnNextCall(testNormal);
assertEquals({value: 42, done: false}, testNormal(getNormalGeneratorObject()));
assertOptimized(testNormal);

// 2. Test Smi (should throw TypeError)
function testSmi(g) {
  next.call(g);
}
%PrepareFunctionForOptimization(testSmi);
testSmi(getNormalGeneratorObject());
testSmi(getNormalGeneratorObject());
%OptimizeMaglevOnNextCall(testSmi);
testSmi(getNormalGeneratorObject());
assertThrows(() => testSmi(1), TypeError);
assertUnoptimized(testSmi);

// 3. Test ordinary object (should throw TypeError)
function testReceiver(g) {
  next.call(g);
}
%PrepareFunctionForOptimization(testReceiver);
testReceiver(getNormalGeneratorObject());
testReceiver(getNormalGeneratorObject());
%OptimizeMaglevOnNextCall(testReceiver);
testReceiver(getNormalGeneratorObject());
assertThrows(() => testReceiver({}), TypeError);
assertUnoptimized(testReceiver);

// 4. Test AsyncGenerator object (should throw TypeError)
function testAsyncGenerator(g) {
  next.call(g);
}
%PrepareFunctionForOptimization(testAsyncGenerator);
testAsyncGenerator(getNormalGeneratorObject());
testAsyncGenerator(getNormalGeneratorObject());
%OptimizeMaglevOnNextCall(testAsyncGenerator);
testAsyncGenerator(getNormalGeneratorObject());

async function* asyncGen() {}
const asyncGeneneratorObject = asyncGen();
assertThrows(() => testAsyncGenerator(asyncGeneneratorObject), TypeError);
assertUnoptimized(testAsyncGenerator);

// 5. Test AsyncFunction (the function itself, should throw TypeError)
function testAsyncFunction(g) {
  next.call(g);
}
%PrepareFunctionForOptimization(testAsyncFunction);
testAsyncFunction(getNormalGeneratorObject());
testAsyncFunction(getNormalGeneratorObject());
%OptimizeMaglevOnNextCall(testAsyncFunction);
testAsyncFunction(getNormalGeneratorObject());

const asyncFunc = async function() {};
assertThrows(() => testAsyncFunction(asyncFunc), TypeError);
assertUnoptimized(testAsyncFunction);

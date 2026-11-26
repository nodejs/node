// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-simplified-lowering

const indexed = 'a'.repeat(16381);
const minusZeroHolder = [-0];

function checkArrayLengthWithMinusZero(takeWidePath, useWideNumber) {
  const index = takeWidePath ? (useWideNumber ? 2 ** 40 : -0) : 3;
  const ch = indexed[index];
  const arr = new Array(index);
  const len = arr.length;
  return [ch, len, 1 / len, Object.is(len, minusZeroHolder[0])];
}

%PrepareFunctionForOptimization(checkArrayLengthWithMinusZero);
for (let i = 0; i < 5; i++) {
  assertEquals(
      ['a', 3, 1 / 3, false], checkArrayLengthWithMinusZero(false, false));
  assertEquals(
      ['a', 0, Infinity, false], checkArrayLengthWithMinusZero(true, false));
}
%OptimizeFunctionOnNextCall(checkArrayLengthWithMinusZero);
assertEquals(
    ['a', 3, 1 / 3, false], checkArrayLengthWithMinusZero(false, false));
assertEquals(
    ['a', 0, Infinity, false], checkArrayLengthWithMinusZero(true, false));

Object.defineProperty(Number.prototype, 'ident', {
  configurable: true,
  get() {
    'use strict';
    return this;
  },
});

const typedArray = new Uint8Array(16);
typedArray[0] = 7;
typedArray[3] = 9;

function checkTaggedTypedArrayWithMinusZero(
    ta, asString, useWideNumber, useMinusZero) {
  const raw =
      asString ? 'text' : (useWideNumber ? 2 ** 40 : (useMinusZero ? -0 : 3));
  const index = raw.ident;
  return ta[index];
}

%PrepareFunctionForOptimization(checkTaggedTypedArrayWithMinusZero);
for (let i = 0; i < 5; i++) {
  assertEquals(
      9, checkTaggedTypedArrayWithMinusZero(typedArray, false, false, false));
  assertEquals(
      7, checkTaggedTypedArrayWithMinusZero(typedArray, false, false, true));
}
%OptimizeFunctionOnNextCall(checkTaggedTypedArrayWithMinusZero);
assertEquals(
    9, checkTaggedTypedArrayWithMinusZero(typedArray, false, false, false));
assertEquals(
    7, checkTaggedTypedArrayWithMinusZero(typedArray, false, false, true));

function checkBigIntWithMinusZero(useMinusZero) {
  const value = useMinusZero ? -0 : 3;
  return BigInt(value);
}

%PrepareFunctionForOptimization(checkBigIntWithMinusZero);
assertEquals(3n, checkBigIntWithMinusZero(false));
assertEquals(0n, checkBigIntWithMinusZero(true));
%OptimizeFunctionOnNextCall(checkBigIntWithMinusZero);
assertEquals(3n, checkBigIntWithMinusZero(false));
assertEquals(0n, checkBigIntWithMinusZero(true));

function checkTaggedBigIntWithMinusZero(asString, useMinusZero) {
  const raw = asString ? 'text' : (useMinusZero ? -0 : 3);
  const value = raw.ident;
  return BigInt(value);
}

%PrepareFunctionForOptimization(checkTaggedBigIntWithMinusZero);
for (let i = 0; i < 5; i++) {
  assertEquals(3n, checkTaggedBigIntWithMinusZero(false, false));
  assertEquals(0n, checkTaggedBigIntWithMinusZero(false, true));
}
%OptimizeFunctionOnNextCall(checkTaggedBigIntWithMinusZero);
assertEquals(3n, checkTaggedBigIntWithMinusZero(false, false));
assertEquals(0n, checkTaggedBigIntWithMinusZero(false, true));

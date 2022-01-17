// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class MyUint8Array extends Uint8Array {};
class MyBigInt64Array extends BigInt64Array {};

const ctors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Int32Array,
  Float32Array,
  Float64Array,
  Uint8ClampedArray,
  BigUint64Array,
  BigInt64Array,
  MyUint8Array,
  MyBigInt64Array,
];

function CreateResizableArrayBuffer(byteLength, maxByteLength) {
  return new ArrayBuffer(byteLength, {maxByteLength: maxByteLength});
}

function ReadDataFromBuffer(ab, ctor) {
  let result = [];
  const ta = new ctor(ab, 0, ab.byteLength / ctor.BYTES_PER_ELEMENT);
  for (let item of ta) {
    result.push(Number(item));
  }
  return result;
}

function WriteToTypedArray(array, index, value) {
  if (array instanceof BigInt64Array ||
      array instanceof BigUint64Array) {
    array[index] = BigInt(value);
  } else {
    array[index] = value;
  }
}

function ToNumbers(array) {
  let result = [];
  for (let item of array) {
    result.push(Number(item));
  }
  return result;
}

function ToNumbersWithEntries(array) {
  let result = [];
  let expectedKey = 0;
  for (let [key, value] of array.entries()) {
    assertEquals(expectedKey, key);
    ++expectedKey;
    result.push(Number(value));
  }
  return result;
}

function Keys(array) {
  let result = [];
  for (let key of array.keys()) {
    result.push(key);
  }
  return result;
}

function ValuesToNumbers(array) {
  let result = [];
  for (let value of array.values()) {
    result.push(Number(value));
  }
  return result;
}

function AtHelper(array, index) {
  let result = array.at(index);
  if (typeof result == 'bigint') {
    return Number(result);
  }
  return result;
}

function FillHelper(array, n, start, end) {
  if (array instanceof BigInt64Array || array instanceof BigUint64Array) {
    array.fill(BigInt(n), start, end);
  } else {
    array.fill(n, start, end);
  }
}

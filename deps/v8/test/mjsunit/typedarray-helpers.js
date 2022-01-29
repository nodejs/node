// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class MyUint8Array extends Uint8Array {};
class MyFloat32Array extends Float32Array {};
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
  MyFloat32Array,
  MyBigInt64Array,
];

// Each element of the following array is [getter, setter, size, isBigInt].
const dataViewAccessorsAndSizes = [[DataView.prototype.getUint8,
                                    DataView.prototype.setUint8, 1, false],
                                   [DataView.prototype.getInt8,
                                    DataView.prototype.setInt8, 1, false],
                                   [DataView.prototype.getUint16,
                                    DataView.prototype.setUint16, 2, false],
                                   [DataView.prototype.getInt16,
                                    DataView.prototype.setInt16, 2, false],
                                   [DataView.prototype.getInt32,
                                    DataView.prototype.setInt32, 4, false],
                                   [DataView.prototype.getFloat32,
                                    DataView.prototype.setFloat32, 4, false],
                                   [DataView.prototype.getFloat64,
                                    DataView.prototype.setFloat64, 8, false],
                                   [DataView.prototype.getBigUint64,
                                    DataView.prototype.setBigUint64, 8, true],
                                   [DataView.prototype.getBigInt64,
                                    DataView.prototype.setBigInt64, 8, true]];

function CreateResizableArrayBuffer(byteLength, maxByteLength) {
  return new ArrayBuffer(byteLength, {maxByteLength: maxByteLength});
}

function CreateGrowableSharedArrayBuffer(byteLength, maxByteLength) {
  return new SharedArrayBuffer(byteLength, {maxByteLength: maxByteLength});
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
    if (typeof item == 'bigint') {
      result.push(Number(item));
    } else {
      result.push(item);
    }
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

function IncludesHelper(array, n, fromIndex) {
  if (typeof n == 'number' &&
      (array instanceof BigInt64Array || array instanceof BigUint64Array)) {
    return array.includes(BigInt(n), fromIndex);
  }
  return array.includes(n, fromIndex);
}

function testDataViewMethodsUpToSize(view, bufferSize) {
  for (const [getter, setter, size, isBigInt] of dataViewAccessorsAndSizes) {
    for (let i = 0; i <= bufferSize - size; ++i) {
      if (isBigInt) {
        setter.call(view, i, 3n);
      } else {
        setter.call(view, i, 3);
      }
      assertEquals(3, Number(getter.call(view, i)));
    }
    if (isBigInt) {
      assertThrows(() => setter.call(view, bufferSize - size + 1, 0n),
                   RangeError);
    } else {
      assertThrows(() => setter.call(view, bufferSize - size + 1, 0),
                   RangeError);
    }
    assertThrows(() => getter.call(view, bufferSize - size + 1), RangeError);
  }
}

function assertAllDataViewMethodsThrow(view, index, errorType) {
  for (const [getter, setter, size, isBigInt] of dataViewAccessorsAndSizes) {
    if (isBigInt) {
      assertThrows(() => { setter.call(view, index, 3n); }, errorType);
    } else {
      assertThrows(() => { setter.call(view, index, 3); }, errorType);
    }
    assertThrows(() => { getter.call(view, index); }, errorType);
  }
}

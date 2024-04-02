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

const floatCtors = [
  Float32Array,
  Float64Array,
  MyFloat32Array
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

function IsBigIntTypedArray(ta) {
  return (ta instanceof BigInt64Array) || (ta instanceof BigUint64Array);
}

function AllBigIntMatchedCtorCombinations(test) {
  for (let targetCtor of ctors) {
    for (let sourceCtor of ctors) {
      if (IsBigIntTypedArray(new targetCtor()) !=
          IsBigIntTypedArray(new sourceCtor())) {
        // Can't mix BigInt and non-BigInt types.
        continue;
      }
      test(targetCtor, sourceCtor);
    }
  }
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

function IndexOfHelper(array, n, fromIndex) {
  if (typeof n == 'number' &&
      (array instanceof BigInt64Array || array instanceof BigUint64Array)) {
    if (fromIndex == undefined) {
      // Technically, passing fromIndex here would still result in the correct
      // behavior, since "undefined" gets converted to 0 which is a good
      // "default" index.
      return array.indexOf(BigInt(n));
    }
    return array.indexOf(BigInt(n), fromIndex);
  }
  if (fromIndex == undefined) {
    return array.indexOf(n);
  }
  return array.indexOf(n, fromIndex);
}

function LastIndexOfHelper(array, n, fromIndex) {
  if (typeof n == 'number' &&
      (array instanceof BigInt64Array || array instanceof BigUint64Array)) {
    if (fromIndex == undefined) {
      // Shouldn't pass fromIndex here, since passing "undefined" is not the
      // same as not passing the parameter at all. "Undefined" will get
      // converted to 0 which is not a good "default" index, since lastIndexOf
      // iterates from the index downwards.
      return array.lastIndexOf(BigInt(n));
    }
    return array.lastIndexOf(BigInt(n), fromIndex);
  }
  if (fromIndex == undefined) {
    return array.lastIndexOf(n);
  }
  return array.lastIndexOf(n, fromIndex);
}

function SetHelper(target, source, offset) {
  if (target instanceof BigInt64Array || target instanceof BigUint64Array) {
    const bigIntSource = [];
    for (s of source) {
      bigIntSource.push(BigInt(s));
    }
    source = bigIntSource;
  }
  if (offset == undefined) {
    return target.set(source);
  }
  return target.set(source, offset);
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

function ObjectDefinePropertyHelper(ta, index, value) {
  if (ta instanceof BigInt64Array || ta instanceof BigUint64Array) {
    Object.defineProperty(ta, index, {value: BigInt(value)});
  } else {
    Object.defineProperty(ta, index, {value: value});
  }
}

function ObjectDefinePropertiesHelper(ta, index, value) {
  const values = {};
  if (ta instanceof BigInt64Array || ta instanceof BigUint64Array) {
    values[index] = {value: BigInt(value)};
  } else {
    values[index] = {value: value};
  }
  Object.defineProperties(ta, values);
}

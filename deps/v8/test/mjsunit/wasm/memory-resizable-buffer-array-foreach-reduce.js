// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --js-staging
// Flags: --experimental-wasm-rab-integration

'use strict';

d8.file.execute('test/mjsunit/typedarray-helpers.js');

const kPageSize = 0x10000;

function Pad(a, v, start, ctor, pages) {
  for (let i = start; i < (pages * kPageSize) / ctor.BYTES_PER_ELEMENT; ++i) {
    a.push(v);
  }
}

function ZeroPad(a, start, ctor, pages) {
  Pad(a, 0, start, ctor, pages);
}

(function ArrayForEachReduceReduceRightDetachMidIteration() {
  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset
  function CreateRabForTest(ctor) {
    const rab = CreateResizableArrayBufferViaWasm(1, 2);
    // Write some data into the array.
    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
      // For reduceRight tests, also write [0, 2, 4, 6] to the end of the TA.
      WriteToTypedArray(taWrite, taWrite.length - 4 + i, 2 * i);
    }
    return rab;
  }

  let values;
  let rab;
  let detachAfter;
  function CollectValuesAndDetach(n) {
    if (typeof n == 'bigint') {
      values.push(Number(n));
    } else {
      values.push(n);
    }
    if (values.length == detachAfter) {
      %ArrayBufferDetachForceWasm(rab);
    }
    return true;
  }

  function ForEachHelper(array) {
    values = [];
    ArrayForEachHelper(array, CollectValuesAndDetach);
    return values;
  }

  function ReduceHelper(array) {
    values = [];
    ArrayReduceHelper(array, (acc, n) => { CollectValuesAndDetach(n); },
                      "initial value");
    return values;
  }

  function ReduceRightHelper(array) {
    values = [];
    ArrayReduceRightHelper(array, (acc, n) => { CollectValuesAndDetach(n); },
                           "initial value");
    return values;
  }

  // Test for forEach.

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    detachAfter = 2;
    assertEquals([0, 2], ForEachHelper(fixedLength));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    detachAfter = 1;
    assertEquals([4], ForEachHelper(fixedLengthWithOffset));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    detachAfter = 2;
    assertEquals([0, 2], ForEachHelper(lengthTracking));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    detachAfter = 1;
    assertEquals([4], ForEachHelper(lengthTrackingWithOffset));
  }

  // Tests for reduce.

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    detachAfter = 2;
    assertEquals([0, 2], ReduceHelper(fixedLength));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    detachAfter = 1;
    assertEquals([4], ReduceHelper(fixedLengthWithOffset));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    detachAfter = 2;
    assertEquals([0, 2], ReduceHelper(lengthTracking));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    detachAfter = 1;
    assertEquals([4], ReduceHelper(lengthTrackingWithOffset));
  }

  // Tests for reduceRight.

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLength = new ctor(rab, 0, 4);
    detachAfter = 2;
    assertEquals([6, 4], ReduceRightHelper(fixedLength));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    detachAfter = 1;
    assertEquals([6], ReduceRightHelper(fixedLengthWithOffset));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTracking = new ctor(rab, 0);
    detachAfter = 2;
    assertEquals([6, 4], ReduceRightHelper(lengthTracking));
  }

  for (let ctor of ctors) {
    rab = CreateRabForTest(ctor);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    detachAfter = 1;
    assertEquals([6], ReduceRightHelper(lengthTrackingWithOffset));
  }
})();

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

(function ArrayFlatFlatMapFrom() {
  const flatHelper = ArrayFlatHelper;
  const flatMapHelper = ArrayFlatMapHelper;

  for (let ctor of ctors) {
    const rab = CreateResizableArrayBufferViaWasm(1, 2);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, kPageSize - 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }
    WriteToTypedArray(taWrite, taWrite.length - 2, 2);
    WriteToTypedArray(taWrite, taWrite.length - 1, 3);

    function mapper(n) {
      if (typeof n == 'bigint') {
        return n + 1n;
      }
      return n + 1;
    }

    const fixedLengthFlat = flatHelper(fixedLength);
    assertEquals([0, 1, 2, 3], ToNumbers(fixedLengthFlat));
    assertTrue(fixedLengthFlat instanceof Array);

    const fixedLengthWithOffsetFlat = flatHelper(fixedLengthWithOffset);
    assertEquals([2, 3], ToNumbers(fixedLengthWithOffsetFlat));
    assertTrue(fixedLengthWithOffsetFlat instanceof Array);

    const lengthTrackingFlat = flatHelper(lengthTracking);
    let expectedLengthTrackingFlat = [0, 1, 2, 3];
    ZeroPad(expectedLengthTrackingFlat, 4, ctor, 1);
    expectedLengthTrackingFlat[expectedLengthTrackingFlat.length - 2] = 2;
    expectedLengthTrackingFlat[expectedLengthTrackingFlat.length - 1] = 3;
    assertEquals(expectedLengthTrackingFlat, ToNumbers(lengthTrackingFlat));
    assertTrue(lengthTrackingFlat instanceof Array);

    const lengthTrackingWithOffsetFlat = flatHelper(lengthTrackingWithOffset);
    assertEquals([2, 3], ToNumbers(lengthTrackingWithOffsetFlat));
    assertTrue(lengthTrackingWithOffsetFlat instanceof Array);

    assertEquals([1, 2, 3, 4],
                 ToNumbers(flatMapHelper(fixedLength, mapper)));
    assertEquals([3, 4],
                 ToNumbers(flatMapHelper(fixedLengthWithOffset, mapper)));
    let expectedLengthTrackingFlatMap = [1, 2, 3, 4];
    Pad(expectedLengthTrackingFlatMap, 1, 4, ctor, 1);
    expectedLengthTrackingFlatMap[expectedLengthTrackingFlatMap.length - 2] = 3;
    expectedLengthTrackingFlatMap[expectedLengthTrackingFlatMap.length - 1] = 4;
    assertEquals(expectedLengthTrackingFlatMap,
                 ToNumbers(flatMapHelper(lengthTracking, mapper)));
    assertEquals([3, 4],
                 ToNumbers(flatMapHelper(lengthTrackingWithOffset, mapper)));

    assertEquals([0, 1, 2, 3], ToNumbers(Array.from(fixedLength)));
    assertEquals([2, 3], ToNumbers(Array.from(fixedLengthWithOffset)));
    assertEquals(expectedLengthTrackingFlat, ToNumbers(Array.from(lengthTracking)));
    assertEquals([2, 3], ToNumbers(Array.from(lengthTrackingWithOffset)));

    // Wasm memories can't shrink.
    assertThrows(() => rab.resize(0), RangeError);

    %ArrayBufferDetachForceWasm(rab);

    assertEquals([], flatHelper(fixedLength));
    assertEquals([], flatHelper(fixedLengthWithOffset));
    assertEquals([], flatHelper(lengthTracking));
    assertEquals([], flatHelper(lengthTrackingWithOffset));

    assertEquals([],
                 ToNumbers(flatMapHelper(fixedLength, mapper)));
    assertEquals([],
                 ToNumbers(flatMapHelper(fixedLengthWithOffset, mapper)));
    assertEquals([],
                 ToNumbers(flatMapHelper(lengthTracking, mapper)));
    assertEquals([],
                 ToNumbers(flatMapHelper(lengthTrackingWithOffset, mapper)));

    assertThrows(() => { Array.from(fixedLength); }, TypeError);
    assertThrows(() => { Array.from(fixedLengthWithOffset); }, TypeError);
    assertThrows(() => { Array.from(lengthTracking); }, TypeError);
    assertThrows(() => { Array.from(lengthTrackingWithOffset) }, TypeError);
  }
})();

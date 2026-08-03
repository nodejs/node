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

(function ArraySlice() {
  const sliceHelper = ArraySliceHelper;

  for (let ctor of ctors) {
    const rab = CreateGrowableSharedArrayBufferViaWasm(1, 2);
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

    const fixedLengthSlice = sliceHelper(fixedLength);
    assertEquals([0, 1, 2, 3], ToNumbers(fixedLengthSlice));
    assertTrue(fixedLengthSlice instanceof Array);

    const fixedLengthWithOffsetSlice = sliceHelper(fixedLengthWithOffset);
    assertEquals([2, 3], ToNumbers(fixedLengthWithOffsetSlice));
    assertTrue(fixedLengthWithOffsetSlice instanceof Array);

    const lengthTrackingSlice = sliceHelper(lengthTracking);
    let expectedLengthTrackingSlice = [0, 1, 2, 3];
    ZeroPad(expectedLengthTrackingSlice, 4, ctor, 1);
    expectedLengthTrackingSlice[expectedLengthTrackingSlice.length - 2] = 2;
    expectedLengthTrackingSlice[expectedLengthTrackingSlice.length - 1] = 3;
    assertEquals(expectedLengthTrackingSlice, ToNumbers(lengthTrackingSlice));
    assertTrue(lengthTrackingSlice instanceof Array);

    const lengthTrackingWithOffsetSlice = sliceHelper(lengthTrackingWithOffset);
    assertEquals([2, 3], ToNumbers(lengthTrackingWithOffsetSlice));
    assertTrue(lengthTrackingWithOffsetSlice instanceof Array);
  }
})();

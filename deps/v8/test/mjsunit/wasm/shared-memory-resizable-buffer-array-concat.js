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

(function ArrayConcatDefault() {
  for (let ctor of ctors) {
    const rab = CreateGrowableSharedArrayBufferViaWasm(1, 2);
    const fixedLength = new ctor(rab, 0, 4);

    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, i + 1);
    }

    // Orig. array: [1, 2, 3, 4]
    //              [1, 2, 3, 4] << fixedLength

    function helper(receiver, ...params) {
      return ToNumbers(Array.prototype.concat.call(receiver, ...params));
    }

    // TypedArrays aren't concat spreadable by default.
    assertEquals([fixedLength, 5, 6, 7], helper(fixedLength, [5, 6], [7]));
  }
})();

(function ArrayConcatConcatSpreadable() {
  for (let ctor of ctors) {
    const rab = CreateGrowableSharedArrayBufferViaWasm(1, 2);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, kPageSize - 2 * ctor.BYTES_PER_ELEMENT);
    fixedLength[Symbol.isConcatSpreadable] = true;
    fixedLengthWithOffset[Symbol.isConcatSpreadable] = true;
    lengthTracking[Symbol.isConcatSpreadable] = true;
    lengthTrackingWithOffset[Symbol.isConcatSpreadable] = true;

    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, i + 1);
    }
    WriteToTypedArray(taWrite, taWrite.length - 2, 3);
    WriteToTypedArray(taWrite, taWrite.length - 1, 4);

    // Orig. array: [1, 2, 3, 4]
    //              [1, 2, 3, 4] << fixedLength
    //                    [3, 4] << fixedLengthWithOffset
    //              [1, 2, 3, 4, ...] << lengthTracking
    //                    [..., 3, 4] << lengthTrackingWithOffset

    function helper(receiver, ...params) {
      return ToNumbers(Array.prototype.concat.call(receiver, ...params));
    }

    assertEquals([0, 1, 2, 3, 4, 5, 6], helper([0], fixedLength, [5, 6]));
    assertEquals([0, 3, 4, 5, 6], helper([0], fixedLengthWithOffset, [5, 6]));
    let expectedLengthTracking = [0];
    for (let i = 0; i < 4; ++i) {
      expectedLengthTracking.push(i + 1);
    }
    ZeroPad(expectedLengthTracking, 4, ctor, 1);
    expectedLengthTracking[expectedLengthTracking.length - 2] = 3;
    expectedLengthTracking[expectedLengthTracking.length - 1] = 4;
    expectedLengthTracking.push(5);
    expectedLengthTracking.push(6);
    assertEquals(expectedLengthTracking, helper([0], lengthTracking, [5, 6]));
    assertEquals([0, 3, 4, 5, 6],
                 helper([0], lengthTrackingWithOffset, [5, 6]));
  }
})();

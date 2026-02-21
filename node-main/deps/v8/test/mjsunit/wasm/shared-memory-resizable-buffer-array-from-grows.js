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

(function ArrayFromMapperGrows() {
  for (let ctor of ctors) {
    const rab = CreateGrowableSharedArrayBufferViaWasm(1, 2);
    const lengthTracking = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(lengthTracking, i, i + 1);
    }
    function mapper(n) {
      rab.grow(2 * kPageSize);
      return n;
    }
    // We keep iterating after the TA has grown.
    let expectedLengthTrackingFrom = [1, 2, 3, 4];
    ZeroPad(expectedLengthTrackingFrom, 4, ctor, 2);
    assertEquals(expectedLengthTrackingFrom,
                 ToNumbers(Array.from(lengthTracking, mapper)));
    assertEquals(2 * kPageSize, rab.byteLength);
  }
})();

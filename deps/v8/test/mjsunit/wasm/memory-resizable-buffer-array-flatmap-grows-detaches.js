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

(function ArrayFlatMapMapperGrows() {
  const flatMapHelper = ArrayFlatMapHelper;

  for (let ctor of ctors) {
    const rab = CreateResizableArrayBufferViaWasm(1, 2);
    const lengthTracking = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(lengthTracking, i, i + 1);
    }
    function mapper(n) {
      rab.resize(2 * kPageSize);
      return n;
    }
    let expectedLengthTrackingFlat = [1, 2, 3, 4];
    ZeroPad(expectedLengthTrackingFlat, 4, ctor, 1);
    assertEquals(expectedLengthTrackingFlat, ToNumbers(flatMapHelper(lengthTracking, mapper)));
    assertEquals(2 * kPageSize, rab.byteLength);
  }
})();

(function ArrayFlatMapMapperDetaches() {
  const flatMapHelper = ArrayFlatMapHelper;
  let rab;
  function mapper(n) {
    %ArrayBufferDetachForceWasm(rab);
    return n;
  }

  for (let ctor of ctors) {
    rab = CreateResizableArrayBufferViaWasm(1, 2);
    const fixedLength = new ctor(rab, 0, 4);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(fixedLength, i, i + 1);
    }
    assertEquals([1], ToNumbers(flatMapHelper(fixedLength, mapper)));
    assertEquals(0, rab.byteLength);
  }
  for (let ctor of ctors) {
    rab = CreateResizableArrayBufferViaWasm(1, 2);
    const lengthTracking = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(lengthTracking, i, i + 1);
    }
    assertEquals([1], ToNumbers(flatMapHelper(lengthTracking, mapper)));
    assertEquals(0, rab.byteLength);
  }
})();

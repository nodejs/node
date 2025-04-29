// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --js-staging
// Flags: --experimental-wasm-rab-integration

'use strict';

d8.file.execute('test/mjsunit/typedarray-helpers.js');

const kPageSize = 0x10000;

(function ArrayPopShift() {
  // These functions always fail since setting the length fails.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBufferViaWasm(1, 2);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, kPageSize - 2 * ctor.BYTES_PER_ELEMENT);

    function testAllFuncsThrow() {
      for (let func of [Array.prototype.pop, Array.prototype.shift]) {
        assertThrows(() => {
            func.call(fixedLength); }, TypeError);
        assertThrows(() => {
            func.call(fixedLengthWithOffset); }, TypeError);
        assertThrows(() => {
            func.call(lengthTracking); }, TypeError);
        assertThrows(() => {
          func.call(lengthTrackingWithOffset); }, TypeError);
      }
    }

    testAllFuncsThrow();

    %ArrayBufferDetachForceWasm(rab);

    testAllFuncsThrow();
  }
})();

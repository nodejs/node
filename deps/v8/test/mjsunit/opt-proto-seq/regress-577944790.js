// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --mock-arraybuffer-allocator

// Test exception propagation when a TypedArray prototype rejects a BigInt store.
(function testLargeTypedArrayStoreException() {
  let ta;
  try {
    ta = new Int8Array(4294967296);
  } catch (e) {
    assertInstanceof(e, RangeError);
    return;
  }

  assertThrows(() => {
    function F() {}
    F.prototype = ta;
    F.prototype['4294967295'] = 1n;
    F.prototype.named = 1;
  }, TypeError);

  assertEquals(undefined, ta.named);
})();

// Test exception propagation when valueOf throws during a TypedArray store.
(function testLargeTypedArrayValueOfException() {
  let ta;
  try {
    ta = new Uint8Array(4294967296);
  } catch (e) {
    assertInstanceof(e, RangeError);
    return;
  }

  const badValue = {
    valueOf() {
      throw new URIError();
    },
  };

  assertThrows(() => {
    function F() {}
    F.prototype = ta;
    F.prototype['4294967295'] = badValue;
    F.prototype.named = 2;
  }, URIError);

  assertEquals(undefined, ta.named);
})();

// Test exception propagation on a small BigInt64Array prototype.
(function testBigIntTypedArrayStoreException() {
  const ta = new BigInt64Array(4);

  assertThrows(() => {
    function F() {}
    F.prototype = ta;
    F.prototype['4294967295'] = 1;
    F.prototype.named = 3;
  }, TypeError);

  assertEquals(undefined, ta.named);
})();

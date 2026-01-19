// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testConcatTypedArray(type, elems, modulo) {
  "use strict";
  var items = new Array(elems);
  var ta_by_len = new type(elems);
  for (var i = 0; i < elems; ++i) {
    ta_by_len[i] = items[i] = modulo === false ? i : elems % modulo;
  }
  var ta = new type(items);
  assertEquals([ta, ta], [].concat(ta, ta));
  ta[Symbol.isConcatSpreadable] = true;
  assertEquals(items, [].concat(ta));

  assertEquals([ta_by_len, ta_by_len], [].concat(ta_by_len, ta_by_len));
  ta_by_len[Symbol.isConcatSpreadable] = true;
  assertEquals(items, [].concat(ta_by_len));

  // TypedArray with fake `length`.
  ta = new type(1);
  var defValue = ta[0];
  var expected = new Array(4000);
  expected[0] = defValue;

  Object.defineProperty(ta, "length", { value: 4000 });
  ta[Symbol.isConcatSpreadable] = true;
  assertEquals(expected, [].concat(ta));
}

(function testConcatSmallTypedArray() {
  var length = 1;
  testConcatTypedArray(Uint8Array, length, Math.pow(2, 8));
  testConcatTypedArray(Uint16Array, length, Math.pow(2, 16));
  testConcatTypedArray(Uint32Array, length,  Math.pow(2, 32));
  testConcatTypedArray(Float32Array, length, false);
  testConcatTypedArray(Float64Array, length, false);
})();


(function testConcatLargeTypedArray() {
  var length = 4000;
  testConcatTypedArray(Uint8Array, length, Math.pow(2, 8));
  testConcatTypedArray(Uint16Array, length, Math.pow(2, 16));
  testConcatTypedArray(Uint32Array, length,  Math.pow(2, 32));
  testConcatTypedArray(Float32Array, length, false);
  testConcatTypedArray(Float64Array, length, false);
})();

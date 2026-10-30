// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Create a dictionary-mode TypedArray that mimics a default function prototype.
function makeDictionaryTypedArray() {
  const ta = new Uint8Array(4);
  ta.temp = 1;
  delete ta.temp;
  ta.constructor = Object;
  Object.setPrototypeOf(ta, Object.prototype);
  return ta;
}

// Test integer indices above kMaxElementIndex on a TypedArray prototype.
(function testTypedArrayLargeIntegerIndices() {
  const ta = makeDictionaryTypedArray();
  function F() {}
  F.prototype = ta;
  F.prototype['4294967295'] = 1;
  F.prototype['4294967296'] = 2;
  F.prototype.named = 3;

  assertEquals(undefined, Object.getOwnPropertyDescriptor(ta, '4294967295'));
  assertEquals(undefined, Object.getOwnPropertyDescriptor(ta, '4294967296'));
  assertEquals(undefined, ta['4294967295']);
  assertEquals(undefined, ta['4294967296']);
  assertEquals(3, ta.named);
})();

// Test canonical numeric index strings on a TypedArray prototype.
(function testTypedArraySpecialNumericIndices() {
  const ta = makeDictionaryTypedArray();
  function F() {}
  F.prototype = ta;
  F.prototype['1.5'] = 1;
  F.prototype['-0'] = 2;
  F.prototype.named = 3;

  assertEquals(undefined, Object.getOwnPropertyDescriptor(ta, '1.5'));
  assertEquals(undefined, Object.getOwnPropertyDescriptor(ta, '-0'));
  assertEquals(undefined, ta['1.5']);
  assertEquals(undefined, ta['-0']);
  assertEquals(3, ta.named);
})();

// Test a dictionary-mode String wrapper as a function prototype.
(function testStringWrapperPrototype() {
  const wrapper = new String('abc');
  wrapper.temp = 1;
  delete wrapper.temp;
  wrapper.constructor = Object;
  Object.setPrototypeOf(wrapper, Object.prototype);

  function F() {}
  F.prototype = wrapper;
  F.prototype.first = 10;
  F.prototype.second = 20;

  assertEquals(10, wrapper.first);
  assertEquals(20, wrapper.second);
  assertEquals('a', wrapper[0]);
})();

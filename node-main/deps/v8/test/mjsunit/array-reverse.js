// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertArrayEquals([], [].reverse());
assertArrayEquals([8, 6, 4, 2], [2, 4, 6, 8].reverse());
assertArrayEquals([0.8, 0.6, 0.4], [0.4, 0.6, 0.8].reverse());
assertArrayEquals(["str4", "str3", "str2"], ["str2", "str3", "str4"].reverse());

assertArrayEquals([4,3,,1], [1,,3,4].reverse());
assertArrayEquals([4,,2,1], [1,2,,4].reverse());
assertArrayEquals([5,,3,,1], [1,,3,,5].reverse());
assertArrayEquals([0.5,,0.3,,0.1], [0.1,,0.3,,0.5].reverse());
assertArrayEquals(["5",,"3",,"1"], ["1",,"3",,"5"].reverse());

function TestReverseWithObject() {
  let obj = { length: 5 };
  obj[0] = "foo";
  obj[3] = "bar";
  Array.prototype.reverse.call(obj);

  assertArrayEquals([,"bar",,,"foo"], obj);
}
TestReverseWithObject();

function TestReverseWithPrototypeChain() {
  let proto = { 0: "foo", 19: "bar" };
  let obj = { length: 20, 5: "foobar", __proto__: proto };
  Array.prototype.reverse.call(obj);

  assertEquals("bar", obj[0]);
  assertEquals("foobar", obj[14]);
  assertEquals("foo", obj[19]);
}
TestReverseWithPrototypeChain();

function TestReverseWithTypedArrays() {
  const constructors = [
    Uint8Array,
    Int8Array,
    Uint16Array,
    Int16Array,
    Uint32Array,
    Int32Array,
    Uint8ClampedArray,
    Float32Array,
    Float64Array
  ];

  for (const constructor of constructors) {
    const array_odd = new constructor([1, 2, 3]);
    Array.prototype.reverse.call(array_odd);
    assertArrayEquals([3, 2, 1], array_odd, constructor);

    const array_even = new constructor([1, 2, 3, 4]);
    Array.prototype.reverse.call(array_even);
    assertArrayEquals([4, 3, 2, 1], array_even, constructor);

    // Array.prototype.reverse respects shadowing length on TypedArrays.
    const array = new constructor([1, 2, 3, 4]);
    Object.defineProperty(array, 'length', {value: 2});
    Array.prototype.reverse.call(array);

    assertArrayEquals([2, 1], array, constructor);

    const array_shadowed_length = new constructor([1, 2]);
    Object.defineProperty(array_shadowed_length, 'length', {value: 5});

    assertThrows(() => Array.prototype.reverse.call(array_shadowed_length));
  }
}
TestReverseWithTypedArrays();

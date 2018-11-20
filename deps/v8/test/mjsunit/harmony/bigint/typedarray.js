// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-bigint --allow-natives-syntax

var intarray = new BigInt64Array(8);
var uintarray = new BigUint64Array(8);

function test(f) {
  f();
  f();  // Make sure we test ICs.
  f();
  %OptimizeFunctionOnNextCall(f);
  f();
}

function test_both(f) {
  test(() => f(BigInt64Array));
  test(() => f(BigUint64Array));
}

test(function basic_assignment() {
  const x = 0x1234567890abcdefn;
  intarray[0] = x;
  assertEquals(x, intarray[0]);
  uintarray[0] = x;
  assertEquals(x, uintarray[0]);
  const y = -0x76543210fedcba98n;
  intarray[0] = y;
  assertEquals(y, intarray[0]);
});

test(function construct() {
  var a = new BigInt64Array([1n, -2n, {valueOf: () => 3n}]);
  assertArrayEquals([1n, -2n, 3n], a);
  assertThrows(() => new BigInt64Array([4, 5]), TypeError);
  var b = new BigUint64Array([6n, -7n]);
  assertArrayEquals([6n, 0xfffffffffffffff9n], b);
  var c = new BigUint64Array(new BigInt64Array([8n, -9n]));
  assertArrayEquals([8n, 0xfffffffffffffff7n], c);
  var d = new BigInt64Array(new BigUint64Array([10n, 0xfffffffffffffff5n]));
  assertArrayEquals([10n, -11n], d);
  assertThrows(() => new BigInt64Array(new Int32Array([12, 13])), TypeError);
  assertThrows(() => new Int32Array(new BigInt64Array([14n, -15n])), TypeError);
});

test_both(function copyWithin(BigArray) {
  var a = BigArray.of(1n, 2n, 3n);
  a.copyWithin(0, 1, 3);
  assertArrayEquals([2n, 3n, 3n], a);
});

test_both(function entries(BigArray) {
  var a = BigArray.of(1n, 2n, 3n);
  var it = a.entries();
  assertEquals([0, 1n], it.next().value);
  assertEquals([1, 2n], it.next().value);
  assertEquals([2, 3n], it.next().value);
  assertTrue(it.next().done);
});

test_both(function every(BigArray) {
  var a = BigArray.of(2n, 3n, 4n);
  var seen = [];
  assertTrue(a.every((x) => {seen.push(x); return x > 1n}));
  assertEquals([2n, 3n, 4n], seen);
});

test_both(function fill(BigArray) {
  var a = BigArray.of(1n, 2n, 3n, 4n);
  a.fill(7n, 1, 3);
  assertArrayEquals([1n, 7n, 7n, 4n], a);
  assertThrows(() => (new BigArray(3).fill(1)), TypeError);
});

test_both(function filter(BigArray) {
  var a = BigArray.of(1n, 3n, 4n, 2n);
  var b = a.filter((x) => x > 2n);
  assertArrayEquals([3n, 4n], b);
});

test_both(function find(BigArray) {
  var a = BigArray.of(1n, 2n, 3n);
  assertEquals(2n, a.find((x) => x === 2n));
  assertEquals(undefined, a.find((x) => x === 2));
});

test_both(function findIndex(BigArray) {
  var a = BigArray.of(1n, 2n, 3n);
  assertEquals(1, a.findIndex((x) => x === 2n));
  assertEquals(-1, a.findIndex((x) => x === 2));
});

test_both(function forEach(BigArray) {
  var a = BigArray.of(1n, 2n, 3n);
  var seen = [];
  a.forEach((x) => seen.push(x));
  assertEquals([1n, 2n, 3n], seen);
});

test_both(function from(BigArray) {
  var a = BigArray.of(1n, 2n, 3n);
  var b = BigArray.from(a);
  assertArrayEquals([1n, 2n, 3n], b);
  assertThrows(() => BigArray.from([4, 5]), TypeError);
  var c = BigArray.from([6, 7], BigInt);
  assertArrayEquals([6n, 7n], c);
  assertThrows(() => Int32Array.from([4n, 5n]), TypeError);
  assertThrows(() => Int32Array.from([4, 5], BigInt), TypeError);
});

test(function from_mixed() {
  var contents = [1n, 2n, 3n];
  var a = new BigInt64Array(contents);
  var b = BigUint64Array.from(a);
  assertArrayEquals(contents, b);
  var c = BigInt64Array.from(b);
  assertArrayEquals(contents, c);
});

test_both(function includes(BigArray) {
  var a = BigArray.of(0n, 1n, 2n);
  assertTrue(a.includes(1n));
  assertFalse(a.includes(undefined));
  assertFalse(a.includes(1));
  assertFalse(a.includes(0x1234567890abcdef123n));  // More than 64 bits.
});

test_both(function indexOf(BigArray) {
  var a = BigArray.of(0n, 1n, 2n);
  assertEquals(1, a.indexOf(1n));
  assertEquals(-1, a.indexOf(undefined));
  assertEquals(-1, a.indexOf(1));
  assertEquals(-1, a.indexOf(0x1234567890abcdef123n));  // More than 64 bits.
});

test_both(function join(BigArray) {
  var a = BigArray.of(1n, 2n, 3n);
  assertEquals("1-2-3", a.join("-"));
});

test_both(function keys(BigArray) {
  var a = BigArray.of(1n, 2n, 3n);
  var it = a.keys();
  assertEquals(0, it.next().value);
  assertEquals(1, it.next().value);
  assertEquals(2, it.next().value);
  assertTrue(it.next().done);
});

test_both(function lastIndexOf(BigArray) {
  var a = BigArray.of(0n, 1n, 2n);
  assertEquals(1, a.lastIndexOf(1n));
  assertEquals(-1, a.lastIndexOf(undefined));
  assertEquals(-1, a.lastIndexOf(1));
  assertEquals(-1, a.lastIndexOf(0x1234567890abcdef123n));  // > 64 bits.
});

test_both(function map(BigArray) {
  var a = BigArray.of(1n, 2n, 3n);
  var b = a.map((x) => 2n * x);
  assertEquals(BigArray, b.constructor);
  assertArrayEquals([2n, 4n, 6n], b);
});

test_both(function of(BigArray) {
  var a = BigArray.of(true, 2n, {valueOf: () => 3n}, "4");
  assertArrayEquals([1n, 2n, 3n, 4n], a);
  assertThrows(() => BigArray.of(1), TypeError)
  assertThrows(() => BigArray.of(undefined), TypeError)
});

test_both(function reduce(BigArray) {
  var a = BigArray.of(1n, 2n, 3n);
  assertEquals(6n, a.reduce((sum, x) => sum + x, 0n));
});

test_both(function reduceRight(BigArray) {
  var a = BigArray.of(1n, 2n, 3n);
  assertEquals(6n, a.reduce((sum, x) => sum + x, 0n));
});

test_both(function reverse(BigArray) {
  var a = BigArray.of(1n, 2n, 3n);
  a.reverse();
  assertArrayEquals([3n, 2n, 1n], a);
});

test_both(function set(BigArray) {
  var a = new BigArray(7);
  a.set(BigArray.of(1n, 2n, 3n), 2);
  assertArrayEquals([0n, 0n, 1n, 2n, 3n, 0n, 0n], a);
  a.set([4n, 5n, 6n], 1);
  assertArrayEquals([0n, 4n, 5n, 6n, 3n, 0n, 0n], a);
  assertThrows(() => a.set([7, 8, 9], 3), TypeError);
  assertThrows(() => a.set(Int32Array.of(10, 11), 2), TypeError);

  var Other = BigArray == BigInt64Array ? BigUint64Array : BigInt64Array;
  a.set(Other.of(12n, 13n), 4);
  assertArrayEquals([0n, 4n, 5n, 6n, 12n, 13n, 0n], a);
});

test_both(function slice(BigArray) {
  var a = BigArray.of(1n, 2n, 3n, 4n);
  var b = a.slice(1, 3);
  assertArrayEquals([2n, 3n], b);
});

test_both(function some(BigArray) {
  var a = BigArray.of(1n, 2n, 3n);
  assertTrue(a.some((x) => x === 2n));
});

test_both(function sort(BigArray) {
  var a = BigArray.of(7n, 2n, 5n, 3n);
  a.sort();
  assertArrayEquals([2n, 3n, 5n, 7n], a);
});

test_both(function subarray(BigArray) {
  var a = BigArray.of(1n, 2n, 3n, 4n);
  var b = a.subarray(1, 3);
  assertEquals(BigArray, b.constructor);
  assertArrayEquals([2n, 3n], b);
});

test_both(function toString(BigArray) {
  var a = BigArray.of(1n, 2n, 3n);
  assertEquals("1,2,3", a.toString());
});

test_both(function values(BigArray) {
  var a = BigArray.of(1n, 2n, 3n);
  var it = a.values();
  assertEquals(1n, it.next().value);
  assertEquals(2n, it.next().value);
  assertEquals(3n, it.next().value);
  assertTrue(it.next().done);
});

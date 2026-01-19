// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testAssertNotEquals(a, b) {
  let impl = (a, b) => {
    assertNotEquals(a, b);
    try {
      assertEquals(a, b);
    } catch (e) {
      return;
    }
    throw new Error('assertEquals did not throw');
  };
  // Test in both directions.
  impl(a, b);
  impl(b, a);
}

function testAssertEquals(a, b) {
  let impl = (a, b) => {
    assertEquals(a, b);
    try {
      assertNotEquals(a, b);
    } catch (e) {
      return;
    }
    throw new Error('assertNotEquals did not throw');
  };
  // Test in both directions.
  impl(a, b);
  impl(b, a);
}

(function TestAssertEqualsNonEnumerableProperty() {
  let a = {};
  assertTrue(Reflect.defineProperty(a, 'prop', {value: 1}));
  testAssertNotEquals({}, a);
  // Both objects are treated as equal if they have the same properties
  // with the same values independent of the property's enumerability.
  testAssertEquals({prop: 1}, a);
  testAssertNotEquals({prop: 2}, a);
})();

(function TestAssertEqualsPropertyOrder() {
  // Test that property order does not matter.
  let a = {};
  a.x = 1;
  a.y = 2;
  let b = {};
  b.y = 2;
  b.x = 1;
  testAssertEquals(a, b);
})();

(function TestAssertEqualsPropertyDifferentName() {
  testAssertNotEquals({a: 1, b: 2}, {a: 1, c: 2});
  testAssertNotEquals({a: 1, b: undefined}, {a: 1, c: undefined});
})();

(function TestAssertEqualsArrays() {
  let arr = new Array();
  arr.push(...[1, 2, 3]);
  assertNotSame([1, 2, 3], arr);
  testAssertEquals([1, 2, 3], arr);
  testAssertNotEquals([1, 2, 3, 4], arr);
  testAssertNotEquals([1, 2, -3], arr);
  testAssertNotEquals([1, 3, 2], arr);
  // Array length matters, even with empty elements
  testAssertEquals(new Array(1), new Array(1));
  testAssertNotEquals(new Array(1), new Array(2));
  testAssertEquals([,,], new Array(2));
  // The difference between empty and undefined is not ignored.
  testAssertNotEquals([undefined], new Array(1));
})();

(function TestAssertEqualsArraysNested() {
  let arr = new Array();
  arr.push(...[1, 2, new Array(3, 4, 5)]);
  assertNotSame([1, 2, [3, 4, 5]], arr);
  testAssertEquals([1, 2, [3, 4, 5]], arr);
})();

(function TestAssertEqualsArrayProperties() {
  // Array properties are ignored.
  let arrWithProp = new Array();
  arrWithProp.myProperty = 'Additional property';
  testAssertEquals([], arrWithProp);
})();

(function TestAssertEqualsArrayObject() {
  // An array isn't treated as equal to an equivalent object.
  let obj = {0: 1, 1: 2};
  Object.defineProperty(obj, 'length', {value: 2});
  Object.setPrototypeOf(obj, Array.prototype);
  testAssertNotEquals(obj , [1, 2]);
  testAssertNotEquals([1, 2], obj);
})();

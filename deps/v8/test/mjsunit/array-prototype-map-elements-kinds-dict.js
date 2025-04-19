// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-always-turbofan

load('test/mjsunit/elements-kinds-helpers.js');

// Tests calling Array.prototype.map on a dictionary mode array invalidate the
// "array constructor" protector, so they must be in a separate file.

function plusOne(x) {
  return x + 1;
}

function plusOneInObject(x) {
  return {a: x.a + 1};
}

(function testDictionaryElements1() {
  function foo(a) {
    return a.map(plusOne);
  }

  const array = [1, 2, 3];
  MakeArrayDictionaryMode(array, () => { return 0; });
  assertTrue(%HasDictionaryElements(array));

  const result = foo(array);
  assertTrue(HasHoleySmiElements(result));
})();

(function testDictionaryElements2() {
  function foo(a) {
    return a.map(plusOne);
  }

  const array = [1, 2, 3];
  MakeArrayDictionaryMode(array, () => { return 0.1; });
  assertTrue(%HasDictionaryElements(array));

  const result = foo(array);
  assertTrue(HasHoleyDoubleElements(result));
})();

(function testDictionaryElements3() {
  function foo(a) {
    return a.map(plusOneInObject);
  }

  const array = [{a: 1}, {a: 2}, {a: 3}];
  MakeArrayDictionaryMode(array, () => { return {a: 0}; });
  assertTrue(%HasDictionaryElements(array));

  const result = foo(array);
  assertTrue(HasHoleyObjectElements(result));
})();

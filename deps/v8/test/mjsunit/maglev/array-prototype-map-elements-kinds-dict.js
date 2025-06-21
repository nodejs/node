// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-always-turbofan

load('test/mjsunit/elements-kinds-helpers.js');

// Tests calling Array.prototype.map on a dictionary mode array invalidate the
// "array constructor" protector, so they must be in a separate file.

function plusOne(x) {
  return x + 1;
}
%PrepareFunctionForOptimization(plusOne);

function plusOneInObject(x) {
  return {a: x.a + 1};
}
%PrepareFunctionForOptimization(plusOneInObject);

(function testDictionaryElements1() {
  function foo(a) {
    return a.map(plusOne);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [1, 2, 3];
  const array2 = [1, 2, 3];
  MakeArrayDictionaryMode(array, () => { return 0; });
  MakeArrayDictionaryMode(array2, () => { return 0; });
  assertTrue(%HasDictionaryElements(array));
  assertTrue(%HasDictionaryElements(array2));

  const result = foo(array);
  assertTrue(HasHoleySmiElements(result));

  %OptimizeMaglevOnNextCall(foo);

  const result2 = foo(array2);
  assertTrue(HasHoleySmiElements(result2));
  assertTrue(isMaglevved(foo));
})();

(function testDictionaryElements2() {
  function foo(a) {
    return a.map(plusOne);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [1, 2, 3];
  const array2 = [1, 2, 3];
  MakeArrayDictionaryMode(array, () => { return 0.1; });
  MakeArrayDictionaryMode(array2, () => { return 0.1; });
  assertTrue(%HasDictionaryElements(array));
  assertTrue(%HasDictionaryElements(array2));

  const result = foo(array);
  assertTrue(HasHoleyDoubleElements(result));

  %OptimizeMaglevOnNextCall(foo);

  const result2 = foo(array2);
  assertTrue(HasHoleyDoubleElements(result2));
  assertTrue(isMaglevved(foo));
})();

(function testDictionaryElements3() {
  function foo(a) {
    return a.map(plusOneInObject);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [{a: 1}, {a: 2}, {a: 3}];
  const array2 = [{a: 1}, {a: 2}, {a: 3}];
  MakeArrayDictionaryMode(array, () => { return {a: 0}; });
  MakeArrayDictionaryMode(array2, () => { return {a: 0}; });
  assertTrue(%HasDictionaryElements(array));

  const result = foo(array);
  assertTrue(HasHoleyObjectElements(result));

  %OptimizeMaglevOnNextCall(foo);

  const result2 = foo(array2);
  assertTrue(HasHoleyObjectElements(result2));
  assertTrue(isMaglevved(foo));
})();

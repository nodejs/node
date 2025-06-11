// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

const kUndefinedPattern = 0xFFF6FFFF;

function CreateBuffer(i32pattern) {
  let buffer = new ArrayBuffer(16);
  let i32 = new Int32Array(buffer);
  for(let i = 0; i < 4; ++i) {
    i32[i] = i32pattern;
  }
  return buffer;
}

(function LoadUndefinedPatternFromTypedArray() {
  function foo(f64) {
    return f64[0];
  }

  let f64 = new Float64Array(CreateBuffer(kUndefinedPattern));

  let x = foo(f64);

  assertNotEquals(undefined, x);
  assertTrue(Number.isNaN(x));
})();

(function HasOwnPropertyWithUndefined() {
  function foo(a, i) {
    return a.hasOwnProperty(i);
  }

  let arr = new Array();
  arr[3] = 3.14;
  arr[1] = undefined;

  if(%IsExperimentalUndefinedDoubleEnabled()) {
    assertTrue(%HasDoubleElements(arr));
  }
  assertFalse(foo(arr, 0));
  assertTrue(foo(arr, 1));
  assertFalse(foo(arr, 2));
  assertTrue(foo(arr, 3));
})();

(function LoadUndefinedFromArray() {
  function foo(a, i) {
    return a[i];
  }

  let arr = new Array();
  arr[3] = 3.14;
  arr[1] = undefined;

  if(%IsExperimentalUndefinedDoubleEnabled()) {
    assertTrue(%HasDoubleElements(arr));
  }
  assertEquals(undefined, foo(arr, 0));
  assertEquals(undefined, foo(arr, 1));
  assertEquals(undefined, foo(arr, 2));
  assertEquals(3.14, foo(arr, 3));
})();


(function StoreMaybeSilencedValue() {
  function foo(cond, a, f64) {
    let result = new Array(...[3.14, , 9.88]);
    // This is an interesting case because the store must silence the undefined
    // pattern loaded from f64[0], but preserve the one from a[0].
    result[0] = cond ? a[0] : f64[0];
    return result;
  }

  // Interpreted
  {
    %PrepareFunctionForOptimization(foo);
    const arr = new Array();
    arr[0] = undefined;

    const f64 = new Float64Array(CreateBuffer(kUndefinedPattern));
    assertEquals(undefined, foo(true, arr, f64)[0]);
    const r = foo(false, arr, f64);
    if(%IsExperimentalUndefinedDoubleEnabled()) {
      assertTrue(%HasDoubleElements(r));
    }
    assertNotEquals(undefined, r[0]);
    assertTrue(Number.isNaN(r[0]));
  }

  // TF-compiled
  {
    %OptimizeFunctionOnNextCall(foo);

    const arr = new Array();
    arr[0] = undefined;

    const f64 = new Float64Array(CreateBuffer(kUndefinedPattern));

    let r = foo(false, arr, f64);
    if(%IsExperimentalUndefinedDoubleEnabled()) {
      assertTrue(%HasDoubleElements(r));
    }
    assertNotEquals(undefined, r[0]);
    assertTrue(Number.isNaN(r[0]));
    assertEquals(undefined, foo(true, arr, f64)[0]);
    r = foo(false, arr, f64);
    assertNotEquals(undefined, r[0]);
    assertTrue(Number.isNaN(r[0]));
  }
})();

(function StoreToArrayWithProto() {
  function foo(a) {
    a[3] = undefined;
  }

  let arr_base = new Array(0, 1, 2, 3, 4, 5);
  let arr = new Array();
  arr.__proto__ = arr_base;
  arr[7] = 7.7;
  foo(arr);
  if(%IsExperimentalUndefinedDoubleEnabled()) {
    %HasDoubleElements(arr);
  }
  assertEquals(3, arr_base[3]);
  assertEquals(undefined, arr[3]);
  assertEquals(undefined, arr[6]);
  assertEquals(7.7, arr[7]);
})();

const largeHoleyArray = new Array(1e4);

for (var i = 0; i < 10; i++) {
  largeHoleyArray[i] = i;
}

for (var i = 50; i < 55; i++) {
  largeHoleyArray[i] = i;
}

(function LoadFromHoleySmiArray() {
  function foo() {
    var newArr = [];
    for (let i = 0; i < largeHoleyArray.length; ++i) {
      newArr[i] = largeHoleyArray[i];
    }
    return newArr;
  }

  %PrepareFunctionForOptimization(foo);
  let r = foo();
  if(%IsExperimentalUndefinedDoubleEnabled()) {
    assertTrue(%HasDoubleElements(r));
  }
  assertEquals(52, r[52]);
  assertEquals(undefined, r[42]);
  %OptimizeFunctionOnNextCall(foo);
  r = foo();
  if(%IsExperimentalUndefinedDoubleEnabled()) {
    assertTrue(%HasDoubleElements(r));
  }
  assertEquals(52, r[52]);
  assertEquals(undefined, r[42]);
})();

(function ArrayConcatSlowPath() {
  function foo() {
    var a = [1, 2, 3];
    a.foo = "hello world";

    var b = Array(4096).fill(1.1);
    b[4098] = 1.2;
    b[4096] = undefined;
    b[4097] = undefined;

    return a.concat(b);
  }

  const r = foo();
  assertEquals(1, r[0]);
  assertEquals(2, r[1]);
  assertEquals(3, r[2]);
  assertEquals(1.1, r[3]);
  assertEquals(1.1, r[4098]);
  assertEquals(undefined, r[4099]);
  assertEquals(undefined, r[4100]);
  assertEquals(1.2, r[4101]);
  if(%IsExperimentalUndefinedDoubleEnabled()) {
    assertTrue(%HasDoubleElements(r));
  }
})();

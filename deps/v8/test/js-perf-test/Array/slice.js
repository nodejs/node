// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kArraySize = 1000;

(() => {
  const A = new Array(kArraySize);

  for (let i = 0; i < A.length; i++) {
    A[i] = i;
  }

  assert(%HasSmiElements(A), "A should have SMI elements for this test");

  // Commonly used to copy.
  function testArraySlice0() {
    return A.slice(0);
  }

  function testArraySlice500() {
    return A.slice(500);
  }

  function testArraySlice500_999() {
    return A.slice(500, 999);
  }

  function testArraySliceN500() {
    return A.slice(-500);
  }

  function testArraySlice200_700() {
    return A.slice(200, 700);
  }

  function testArraySlice200_N300() {
    return A.slice(200, -300);
  }

  function testArraySlice4_1() {
    return A.slice(200, -300);
  }

  createSuiteWithWarmup("Array.slice(0)", 1, testArraySlice0);
  createSuiteWithWarmup("Array.slice(500)", 1, testArraySlice500);
  createSuiteWithWarmup("Array.slice(500,999)", 1, testArraySlice500_999);
  createSuiteWithWarmup("Array.slice(-500)", 1, testArraySliceN500);
  createSuiteWithWarmup("Array.slice(200,700)", 1, testArraySlice200_700);
  createSuiteWithWarmup("Array.slice(200,-300)", 1, testArraySlice200_N300);
  createSuiteWithWarmup("Array.slice(4,1)", 1, testArraySlice4_1);

})();

(() => {
  const A = new Array(kArraySize);

  for (let i = 0; i < A.length; i++) {
    A[i] = i;
  }

  A[100000] = 255;
  assert(%HasDictionaryElements(A), "A should be in dictionary mode for this test");

  function testArraySlice0() {
    return A.slice(0);
  }

  function testArraySlice500_999() {
    return A.slice(500, 999);
  }

  function testArraySlice200_700() {
    return A.slice(200, 700);
  }

  function testArraySlice200_N300() {
    return A.slice(200, -300);
  }

  function testArraySlice4_1() {
    return A.slice(200, -300);
  }

  createSuiteWithWarmup("Array.slice(0)-dict", 1, testArraySlice0);
  createSuiteWithWarmup("Array.slice(500,999)-dict", 1, testArraySlice500_999);
  createSuiteWithWarmup("Array.slice(200,700)-dict", 1, testArraySlice200_700);
  createSuiteWithWarmup("Array.slice(200,-300)-dict", 1, testArraySlice200_N300);
  createSuiteWithWarmup("Array.slice(4,1)-dict", 1, testArraySlice4_1);

})();

(() => {
  const A = new Array(kArraySize);

  for (let i = 0; i < A.length; i++) {
    A[i] = i + 0.5;
  }

  assert(%HasDoubleElements(A), "A should have double elements for this test");

  function testArraySlice0() {
    return A.slice(0);
  }

  function testArraySlice500_999() {
    return A.slice(500, 999);
  }

  function testArraySlice200_700() {
    return A.slice(200, 700);
  }

  function testArraySlice200_N300() {
    return A.slice(200, -300);
  }

  function testArraySlice4_1() {
    return A.slice(200, -300);
  }

  createSuiteWithWarmup("Array.slice(0)-double", 1, testArraySlice0);
  createSuiteWithWarmup("Array.slice(500,999)-double", 1, testArraySlice500_999);
  createSuiteWithWarmup("Array.slice(200,700)-double", 1, testArraySlice200_700);
  createSuiteWithWarmup("Array.slice(200,-300)-double", 1, testArraySlice200_N300);
  createSuiteWithWarmup("Array.slice(4,1)-double", 1, testArraySlice4_1);

})();

(() => {
  const A = new Array(kArraySize);

  for (let i = 0; i < A.length; i++) {
    A[i] = new Object();
  }

  assert(%HasObjectElements(A), "A should have object elements for this test");

  function testArraySlice0() {
    return A.slice(0);
  }

  function testArraySlice500_999() {
    return A.slice(500, 999);
  }

  function testArraySlice200_700() {
    return A.slice(200, 700);
  }

  function testArraySlice200_N300() {
    return A.slice(200, -300);
  }

  function testArraySlice4_1() {
    return A.slice(200, -300);
  }

  createSuiteWithWarmup("Array.slice(0)-object", 1, testArraySlice0);
  createSuiteWithWarmup("Array.slice(500,999)-object", 1, testArraySlice500_999);
  createSuiteWithWarmup("Array.slice(200,700)-object", 1, testArraySlice200_700);
  createSuiteWithWarmup("Array.slice(200,-300)-object", 1, testArraySlice200_N300);
  createSuiteWithWarmup("Array.slice(4,1)-object", 1, testArraySlice4_1);

})();

(() => {
  const A = new Array(kArraySize);

  for (let i = 0; i < A.length; i++) {
    A[i] = i;
  }

  assert(%HasSmiElements(A), "A should have SMI elements for this test");

  let arguments_array;
  function sloppy_aliased(a) {
    arguments_array = arguments;
  }
  sloppy_aliased.apply(null, A);

  assert(%HasSloppyArgumentsElements(arguments_array),
      "arguments_array should have sloppy arguments elements for this test");

  function testArraySlice0() {
    return Array.prototype.slice.call(arguments_array, 0);
  }

  function testArraySlice500_999() {
    return Array.prototype.slice.call(arguments_array, 500, 999);
  }

  function testArraySlice200_700() {
    return Array.prototype.slice.call(arguments_array, 200, 700);
  }

  function testArraySlice200_N300() {
    return Array.prototype.slice.call(arguments_array, 200, -300);
  }

  function testArraySlice4_1() {
    return Array.prototype.slice.call(arguments_array, 200, -300);
  }

  createSuiteWithWarmup("Array.slice(0)-sloppy-args", 1, testArraySlice0);
  createSuiteWithWarmup("Array.slice(500,999)-sloppy-args", 1, testArraySlice500_999);
  createSuiteWithWarmup("Array.slice(200,700)-sloppy-args", 1, testArraySlice200_700);
  createSuiteWithWarmup("Array.slice(200,-300)-sloppy-args", 1, testArraySlice200_N300);
  createSuiteWithWarmup("Array.slice(4,1)-sloppy-args", 1, testArraySlice4_1);

})();

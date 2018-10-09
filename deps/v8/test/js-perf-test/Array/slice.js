// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(() => {

  const A = new Array(1000);

  for (let i = 0; i < A.length; i++) {
    A[i] = i;
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

  createSuiteWithWarmup("Array.slice(500)", 1, testArraySlice500);
  createSuiteWithWarmup("Array.slice(500,999)", 1, testArraySlice500_999);
  createSuiteWithWarmup("Array.slice(-500)", 1, testArraySliceN500);
  createSuiteWithWarmup("Array.slice(200,700)", 1, testArraySlice200_700);
  createSuiteWithWarmup("Array.slice(200,-300)", 1, testArraySlice200_N300);
  createSuiteWithWarmup("Array.slice(4,1)", 1, testArraySlice4_1);

})();

(() => {
  const A = new Array(1000);

  for (let i = 0; i < A.length; i++) {
    A[i] = i;
  }

  A[100000] = 255;
  assert(%HasDictionaryElements(A), "A should be in dictionary mode for this test");

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

  createSuiteWithWarmup("Array.slice(500,999)-dict", 1, testArraySlice500_999);
  createSuiteWithWarmup("Array.slice(200,700)-dict", 1, testArraySlice200_700);
  createSuiteWithWarmup("Array.slice(200,-300)-dict", 1, testArraySlice200_N300);
  createSuiteWithWarmup("Array.slice(4,1)-dict", 1, testArraySlice4_1);

})();

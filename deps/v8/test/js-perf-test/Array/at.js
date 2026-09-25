// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const array_at_size = 100;

// SMI array
(() => {
  const A = new Array(array_at_size);

  for (let i = 0; i < A.length; i++) {
    A[i] = i;
  }

  assert(%HasSmiElements(A), "A should have SMI elements for this test");

  // Commonly used to get the last item.
  function testArrayAtNeg1() {
    return A.at(-1);
  }

  function testArrayAt0() {
    return A.at(0);
  }

  function testArrayAt20() {
    return A.at(20);
  }

  function testArrayAt80() {
    return A.at(80);
  }

  createSuiteWithWarmup("Array.at(-1)-smi", 1, testArrayAtNeg1);
  createSuiteWithWarmup("Array.at(0)-smi", 1, testArrayAt0);
  createSuiteWithWarmup("Array.at(20)-smi", 1, testArrayAt20);
  createSuiteWithWarmup("Array.at(80)-smi", 1, testArrayAt80);
})();

// Double array
(() => {
  const A = new Array(array_at_size);

  for (let i = 0; i < A.length; i++) {
    A[i] = i + 0.5;
  }

  assert(%HasDoubleElements(A), "A should have Double elements for this test");

  // Commonly used to get the last item.
  function testArrayAtNeg1() {
    return A.at(-1);
  }

  function testArrayAt0() {
    return A.at(0);
  }

  function testArrayAt20() {
    return A.at(20);
  }

  function testArrayAt80() {
    return A.at(80);
  }

  createSuiteWithWarmup("Array.at(-1)-double", 1, testArrayAtNeg1);
  createSuiteWithWarmup("Array.at(0)-double", 1, testArrayAt0);
  createSuiteWithWarmup("Array.at(20)-double", 1, testArrayAt20);
  createSuiteWithWarmup("Array.at(80)-double", 1, testArrayAt80);
})();

// Object array
(() => {
  const A = new Array(array_at_size);

  for (let i = 0; i < A.length; i++) {
    A[i] = { ["p" + i] : i};
  }

  assert(%HasObjectElements(A), "A should have Object elements for this test");

  // Commonly used to get the last item.
  function testArrayAtNeg1() {
    return A.at(-1);
  }

  function testArrayAt0() {
    return A.at(0);
  }

  function testArrayAt20() {
    return A.at(20);
  }

  function testArrayAt80() {
    return A.at(80);
  }

  createSuiteWithWarmup("Array.at(-1)-object", 1, testArrayAtNeg1);
  createSuiteWithWarmup("Array.at(0)-object", 1, testArrayAt0);
  createSuiteWithWarmup("Array.at(20)-object", 1, testArrayAt20);
  createSuiteWithWarmup("Array.at(80)-object", 1, testArrayAt80);
})();

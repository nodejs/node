// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Flags: --no-always-sparkplug --no-always-turbofan --no-stress-maglev


function allocateArray() {
  return [1, 2, 3];
}
%PrepareFunctionForOptimization(allocateArray);

let a1 = allocateArray();
a1[0] = 0.5;

let a2 = allocateArray();

// Check whether the configuration supports mementos.
let hasMementos = %HasDoubleElements(a2);

function accessArray(a) {
  return a[0];
}
%PrepareFunctionForOptimization(accessArray);

let doubleArray = [1.5, 2.5, 3.5];
let smiArray = [1, 2, 3];

// Make the feedback polymorphic (PACKED_DOUBLE_ELEMENTS, PACKED_SMI_ELEMENTS).
accessArray(doubleArray);
accessArray(smiArray);

let smiArray2 = [4, 5, 6];
assertTrue(%HasSmiElements(smiArray2));
accessArray(smiArray2);

if (hasMementos) {
  // Loading the element updated the elements kind.
  assertTrue(%HasDoubleElements(smiArray2));
}

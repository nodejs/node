// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Flags: --no-always-sparkplug --no-stress-maglev


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

// Simple map-change transitions (sharing the same backing store representation)
// must not transition elements kinds or pollute AllocationSites on loads.
(function TestNoSimpleMapChangeTransitionOnLoad() {
  // 1. PACKED_ELEMENTS -> HOLEY_ELEMENTS
  function makePackedObjectArray() {
    return ['a', 'b'];
  }
  %PrepareFunctionForOptimization(makePackedObjectArray);
  function loadPackedOrHoleyObject(a) {
    return a[0];
  }
  %PrepareFunctionForOptimization(loadPackedOrHoleyObject);

  let holeyObjectArray = ['x', , 'z'];
  assertTrue(%HasObjectElements(holeyObjectArray));
  assertTrue(%HasHoleyElements(holeyObjectArray));

  let packedObj1 = makePackedObjectArray();
  assertTrue(%HasObjectElements(packedObj1));
  assertTrue(%HasPackedElements(packedObj1));

  loadPackedOrHoleyObject(holeyObjectArray);
  loadPackedOrHoleyObject(packedObj1);

  let packedObj2 = makePackedObjectArray();
  assertTrue(%HasPackedElements(packedObj2));
  loadPackedOrHoleyObject(packedObj2);
  assertTrue(%HasPackedElements(packedObj2));
  assertTrue(%HasPackedElements(makePackedObjectArray()));

  // 2. PACKED_SMI_ELEMENTS -> HOLEY_SMI_ELEMENTS
  function makePackedSmiArray() {
    return [1, 2];
  }
  %PrepareFunctionForOptimization(makePackedSmiArray);
  function loadPackedOrHoleySmi(a) {
    return a[0];
  }
  %PrepareFunctionForOptimization(loadPackedOrHoleySmi);

  let holeySmiArray = [1, , 3];
  assertTrue(%HasSmiElements(holeySmiArray));
  assertTrue(%HasHoleyElements(holeySmiArray));

  loadPackedOrHoleySmi(holeySmiArray);
  loadPackedOrHoleySmi(makePackedSmiArray());

  let packedSmi2 = makePackedSmiArray();
  assertTrue(%HasSmiElements(packedSmi2));
  assertFalse(%HasHoleyElements(packedSmi2));
  loadPackedOrHoleySmi(packedSmi2);
  assertTrue(%HasSmiElements(packedSmi2));
  assertFalse(%HasHoleyElements(packedSmi2));
  assertFalse(%HasHoleyElements(makePackedSmiArray()));

  // 3. PACKED_DOUBLE_ELEMENTS -> HOLEY_DOUBLE_ELEMENTS
  function makePackedDoubleArray() {
    return [1.5, 2.5];
  }
  %PrepareFunctionForOptimization(makePackedDoubleArray);
  function loadPackedOrHoleyDouble(a) {
    return a[0];
  }
  %PrepareFunctionForOptimization(loadPackedOrHoleyDouble);

  let holeyDoubleArray = [1.5, , 3.5];
  assertTrue(%HasDoubleElements(holeyDoubleArray));
  assertTrue(%HasHoleyElements(holeyDoubleArray));

  loadPackedOrHoleyDouble(holeyDoubleArray);
  loadPackedOrHoleyDouble(makePackedDoubleArray());

  let packedDouble2 = makePackedDoubleArray();
  assertTrue(%HasDoubleElements(packedDouble2));
  assertFalse(%HasHoleyElements(packedDouble2));
  loadPackedOrHoleyDouble(packedDouble2);
  assertTrue(%HasDoubleElements(packedDouble2));
  assertFalse(%HasHoleyElements(packedDouble2));
  assertFalse(%HasHoleyElements(makePackedDoubleArray()));

  // 4. PACKED_SMI_ELEMENTS -> PACKED_ELEMENTS
  function makeSmiForObjLoad() {
    return [10, 20];
  }
  %PrepareFunctionForOptimization(makeSmiForObjLoad);
  function loadSmiOrObject(a) {
    return a[0];
  }
  %PrepareFunctionForOptimization(loadSmiOrObject);

  let objArray = ['hello', 'world'];
  assertTrue(%HasObjectElements(objArray));
  assertTrue(%HasPackedElements(objArray));

  loadSmiOrObject(objArray);
  loadSmiOrObject(makeSmiForObjLoad());

  let smiArr = makeSmiForObjLoad();
  assertTrue(%HasSmiElements(smiArr));
  loadSmiOrObject(smiArr);
  assertTrue(%HasSmiElements(smiArr));
  assertTrue(%HasSmiElements(makeSmiForObjLoad()));
})();

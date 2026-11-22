// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-allocation-site-pretenuring

(function TestPackedDoubleToPackedObject() {
  function make() {
    return [1.1, 2.2];
  }
  %PrepareFunctionForOptimization(make);

  // First call goes through Runtime_CreateArrayLiteral -> DeepCopy.
  let a = make();
  assertTrue(%HasDoubleElements(a));
  assertTrue(%HasFastPackedElements(a));

  a[0] = 'obj';
  assertTrue(%HasObjectElements(a));
  assertTrue(%HasFastPackedElements(a));

  let b = make();
  assertTrue(%HasObjectElements(b));
  assertTrue(%HasFastPackedElements(b));

  %OptimizeFunctionOnNextCall(make);
  let c = make();
  assertTrue(%HasObjectElements(c));
  assertTrue(%HasFastPackedElements(c));
})();

(function TestPackedDoubleToHoleyDouble() {
  function make() {
    return [1.1, 2.2];
  }
  %PrepareFunctionForOptimization(make);

  let a = make();
  assertTrue(%HasDoubleElements(a));
  assertTrue(%HasFastPackedElements(a));

  a[5] = 3.3;
  assertTrue(%HasDoubleElements(a));
  assertTrue(%HasHoleyElements(a));

  let b = make();
  assertTrue(%HasDoubleElements(b));
  assertTrue(%HasHoleyElements(b));

  %OptimizeFunctionOnNextCall(make);
  let c = make();
  assertTrue(%HasDoubleElements(c));
  assertTrue(%HasHoleyElements(c));
})();

(function TestHoleyDoubleToHoleyObject() {
  function make() {
    return [1.1, , 2.2];
  }
  %PrepareFunctionForOptimization(make);

  let a = make();
  assertTrue(%HasDoubleElements(a));
  assertTrue(%HasHoleyElements(a));

  a[0] = 'obj';
  assertTrue(%HasObjectElements(a));
  assertTrue(%HasHoleyElements(a));

  let b = make();
  assertTrue(%HasObjectElements(b));
  assertTrue(%HasHoleyElements(b));

  %OptimizeFunctionOnNextCall(make);
  let c = make();
  assertTrue(%HasObjectElements(c));
  assertTrue(%HasHoleyElements(c));
})();

(function TestPackedObjectToHoleyObject() {
  function make() {
    return ['a', 'b'];
  }
  %PrepareFunctionForOptimization(make);

  let a = make();
  assertTrue(%HasObjectElements(a));
  assertTrue(%HasFastPackedElements(a));

  a[5] = 'c';
  assertTrue(%HasObjectElements(a));
  assertTrue(%HasHoleyElements(a));

  let b = make();
  assertTrue(%HasObjectElements(b));
  assertTrue(%HasHoleyElements(b));

  %OptimizeFunctionOnNextCall(make);
  let c = make();
  assertTrue(%HasObjectElements(c));
  assertTrue(%HasHoleyElements(c));
})();

(function TestPackedSmiToDoubleToObject() {
  function make() {
    return [1, 2];
  }
  %PrepareFunctionForOptimization(make);

  let a = make();
  assertTrue(%HasSmiElements(a));
  assertTrue(%HasFastPackedElements(a));

  a[0] = 1.5;
  let b = make();
  assertTrue(%HasDoubleElements(b));
  assertTrue(%HasFastPackedElements(b));

  b[0] = 'obj';
  let c = make();
  assertTrue(%HasObjectElements(c));
  assertTrue(%HasFastPackedElements(c));

  %OptimizeFunctionOnNextCall(make);
  let d = make();
  assertTrue(%HasObjectElements(d));
  assertTrue(%HasFastPackedElements(d));
})();

(function TestNestedArrayAndObjectLiterals() {
  function makeNested() {
    return {
      double_arr: [1.1, 2.2],
      obj_arr: ['a', 'b'],
      deep_arr: [[[[1.1, 2.2]]]],
    };
  }
  %PrepareFunctionForOptimization(makeNested);

  let a = makeNested();
  assertTrue(%HasDoubleElements(a.double_arr));
  assertTrue(%HasFastPackedElements(a.double_arr));
  assertTrue(%HasObjectElements(a.obj_arr));
  assertTrue(%HasFastPackedElements(a.obj_arr));
  assertTrue(%HasDoubleElements(a.deep_arr[0][0][0]));
  assertTrue(%HasFastPackedElements(a.deep_arr[0][0][0]));

  a.double_arr[0] = 'obj';
  a.obj_arr[5] = 'c';
  a.deep_arr[0][0][0][0] = 'obj';

  let b = makeNested();
  assertTrue(%HasObjectElements(b.double_arr));
  assertTrue(%HasFastPackedElements(b.double_arr));
  assertTrue(%HasObjectElements(b.obj_arr));
  assertTrue(%HasHoleyElements(b.obj_arr));
  assertTrue(%HasObjectElements(b.deep_arr[0][0][0]));
  assertTrue(%HasFastPackedElements(b.deep_arr[0][0][0]));

  // Also test transitioning a nested array on a subsequent call that goes
  // through runtime deep copy.
  b.deep_arr[0][0][0][5] = 'holey';
  let c = makeNested();
  assertTrue(%HasObjectElements(c.deep_arr[0][0][0]));
  assertTrue(%HasHoleyElements(c.deep_arr[0][0][0]));
})();

(function TestDeoptOnPackedDoubleBoilerplateTransition() {
  function make() {
    return [1.1, 2.2];
  }
  %PrepareFunctionForOptimization(make);

  // Allocate before optimization so `a` has an AllocationMemento.
  let a = make();
  make();
  %OptimizeFunctionOnNextCall(make);
  let b = make();
  assertTrue(%HasDoubleElements(b));
  assertTrue(%HasFastPackedElements(b));

  // Transitioning `a` from PACKED_DOUBLE_ELEMENTS to PACKED_ELEMENTS updates
  // the AllocationSite boilerplate and must deoptimize `make`.
  a[0] = 'obj';
  assertUnoptimized(make);

  let c = make();
  assertTrue(%HasObjectElements(c));
  assertTrue(%HasFastPackedElements(c));
})();

(function TestDeoptOnPackedObjectBoilerplateTransition() {
  function make() {
    return ['a', 'b'];
  }
  %PrepareFunctionForOptimization(make);

  // Allocate before optimization so `a` has an AllocationMemento.
  let a = make();
  make();
  %OptimizeFunctionOnNextCall(make);
  let b = make();
  assertTrue(%HasObjectElements(b));
  assertTrue(%HasFastPackedElements(b));

  // Transitioning `a` from PACKED_ELEMENTS to HOLEY_ELEMENTS updates the
  // AllocationSite boilerplate and must deoptimize `make`.
  a[5] = 'c';
  assertUnoptimized(make);

  let c = make();
  assertTrue(%HasObjectElements(c));
  assertTrue(%HasHoleyElements(c));
})();

(function TestArrayConstructorNonSmiTransitionsAndDeopt() {
  function make() {
    return new Array();
  }
  %PrepareFunctionForOptimization(make);

  let a = make();
  assertTrue(%HasSmiElements(a));
  assertTrue(%HasFastPackedElements(a));

  // Transition PACKED_SMI_ELEMENTS -> PACKED_DOUBLE_ELEMENTS.
  a[0] = 1.5;
  let b = make();
  assertTrue(%HasDoubleElements(b));
  assertTrue(%HasFastPackedElements(b));

  %OptimizeFunctionOnNextCall(make);
  let c = make();
  assertTrue(%HasDoubleElements(c));
  assertTrue(%HasFastPackedElements(c));

  // `b` was allocated before optimization when the site had
  // PACKED_DOUBLE_ELEMENTS; transitioning `b` to PACKED_ELEMENTS must update
  // the site and deoptimize `make`.
  b[0] = 'obj';
  assertUnoptimized(make);

  let d = make();
  assertTrue(%HasObjectElements(d));
  assertTrue(%HasFastPackedElements(d));
})();

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc
// Flags: --no-incremental-marking --no-stress-incremental-marking
// Flags: --no-stress-concurrent-allocation

function prepareUnoptimized(fn) {
  %EnsureFeedbackVectorForFunction(fn);
  %NeverOptimizeFunction(fn);
}

// Test empty array literal ElementsKind transitions via AllocationSiteCache.
(function TestEmptyArrayLiteralTransitions() {
  gc();
  function makeEmpty() {
    return [];
  }
  prepareUnoptimized(makeEmpty);

  let a0 = makeEmpty();
  assertTrue(%HasSmiElements(a0));

  let a1 = makeEmpty();
  a1[0] = 1.5;
  assertTrue(%HasDoubleElements(a1));

  let a2 = makeEmpty();
  assertTrue(%HasDoubleElements(a2));
  a2[0] = {};
  assertTrue(%HasObjectElements(a2));

  let a3 = makeEmpty();
  assertTrue(%HasObjectElements(a3));
  a3[10] = 1;
  assertTrue(%HasHoleyElements(a3));

  let a4 = makeEmpty();
  assertTrue(%HasHoleyElements(a4));
})();

// Test non-empty shallow array literal ElementsKind transitions.
(function TestShallowArrayLiteralTransitions() {
  gc();
  function makeLiteral() {
    return [1, 2, 3];
  }
  prepareUnoptimized(makeLiteral);

  let a0 = makeLiteral();
  assertTrue(%HasSmiElements(a0));

  let a1 = makeLiteral();
  a1[0] = 2.5;
  assertTrue(%HasDoubleElements(a1));

  let a2 = makeLiteral();
  assertTrue(%HasDoubleElements(a2));
  a2[1] = 'str';
  assertTrue(%HasObjectElements(a2));

  let a3 = makeLiteral();
  assertTrue(%HasObjectElements(a3));
})();

// Test non-Smi shallow array literal transitions (DOUBLE -> OBJECT,
// DOUBLE -> HOLEY_DOUBLE, OBJECT -> HOLEY_ELEMENTS).
(function TestNonSmiShallowArrayLiteralTransitions() {
  gc();
  function makeDoubleToObj() {
    return [1.1, 2.2];
  }
  prepareUnoptimized(makeDoubleToObj);

  let d0 = makeDoubleToObj();
  assertTrue(%HasDoubleElements(d0));
  assertFalse(%HasHoleyElements(d0));
  d0[0] = 'obj';
  assertTrue(%HasObjectElements(d0));
  let d1 = makeDoubleToObj();
  assertTrue(%HasObjectElements(d1));
  assertFalse(%HasHoleyElements(d1));

  function makeDoubleToHoley() {
    return [1.1, 2.2];
  }
  prepareUnoptimized(makeDoubleToHoley);

  let dh0 = makeDoubleToHoley();
  assertTrue(%HasDoubleElements(dh0));
  assertFalse(%HasHoleyElements(dh0));
  dh0[5] = 3.3;
  assertTrue(%HasDoubleElements(dh0));
  assertTrue(%HasHoleyElements(dh0));
  let dh1 = makeDoubleToHoley();
  assertTrue(%HasDoubleElements(dh1));
  assertTrue(%HasHoleyElements(dh1));

  function makeObjToHoley() {
    return ['a', 'b'];
  }
  prepareUnoptimized(makeObjToHoley);

  let oh0 = makeObjToHoley();
  assertTrue(%HasObjectElements(oh0));
  assertFalse(%HasHoleyElements(oh0));
  oh0[5] = 'c';
  assertTrue(%HasObjectElements(oh0));
  assertTrue(%HasHoleyElements(oh0));
  let oh1 = makeObjToHoley();
  assertTrue(%HasObjectElements(oh1));
  assertTrue(%HasHoleyElements(oh1));
})();

// Test chained transitions on the same array instance without intermediate
// allocations (PACKED_SMI -> PACKED_DOUBLE -> PACKED_ELEMENTS ->
// HOLEY_ELEMENTS).
(function TestChainedTransitionsOnSameArray() {
  gc();
  function make() {
    return [1, 2];
  }
  prepareUnoptimized(make);

  let a = make();
  assertTrue(%HasSmiElements(a));
  assertFalse(%HasHoleyElements(a));

  a[0] = 1.5;
  assertTrue(%HasDoubleElements(a));
  assertFalse(%HasHoleyElements(a));

  a[0] = 'obj';
  assertTrue(%HasObjectElements(a));
  assertFalse(%HasHoleyElements(a));

  a[10] = 'holey';
  assertTrue(%HasObjectElements(a));
  assertTrue(%HasHoleyElements(a));

  let next = make();
  assertTrue(%HasObjectElements(next));
  assertTrue(%HasHoleyElements(next));
})();

// Test object literal with nested array literal (Runtime_CreateObjectLiteral /
// DeepCopy) recording the inner array in AllocationSiteCache.
(function TestObjectLiteralWithNestedArray() {
  gc();
  function makeObj() {
    return { x: 1, arr: [1, 2] };
  }
  prepareUnoptimized(makeObj);

  let o0 = makeObj();
  assertTrue(%HasSmiElements(o0.arr));
  o0.arr[0] = 1.5;
  assertTrue(%HasDoubleElements(o0.arr));

  let o1 = makeObj();
  assertTrue(%HasDoubleElements(o1.arr));
  o1.arr[0] = 'obj';
  assertTrue(%HasObjectElements(o1.arr));

  let o2 = makeObj();
  assertTrue(%HasObjectElements(o2.arr));
})();

// Test Array.prototype.push transitions updating AllocationSite via cache.
(function TestArrayPushTransitions() {
  gc();
  function makePushLit() {
    return [];
  }
  prepareUnoptimized(makePushLit);

  let a0 = makePushLit();
  assertTrue(%HasSmiElements(a0));
  a0.push(1.5);
  assertTrue(%HasDoubleElements(a0));

  let a1 = makePushLit();
  assertTrue(%HasDoubleElements(a1));
  a1.push('obj');
  assertTrue(%HasObjectElements(a1));

  let a2 = makePushLit();
  assertTrue(%HasObjectElements(a2));

  function makePushCtor() {
    return new Array();
  }
  prepareUnoptimized(makePushCtor);

  let b0 = makePushCtor();
  assertTrue(%HasSmiElements(b0));
  b0.push(1.5);
  assertTrue(%HasDoubleElements(b0));

  let b1 = makePushCtor();
  assertTrue(%HasDoubleElements(b1));
  b1.push('obj');
  assertTrue(%HasObjectElements(b1));

  let b2 = makePushCtor();
  assertTrue(%HasObjectElements(b2));
})();

// Test batch allocation before transition and cache clearing across GC.
(function TestBatchAndGC() {
  gc();
  function makeBatchItem() {
    return [10, 20];
  }
  prepareUnoptimized(makeBatchItem);

  let batch = [];
  for (let i = 0; i < 16; i++) {
    batch.push(makeBatchItem());
  }
  assertTrue(%HasSmiElements(batch[0]));
  batch[0][0] = 3.14;
  assertTrue(%HasDoubleElements(batch[0]));

  let after = makeBatchItem();
  assertTrue(%HasDoubleElements(after));

  // Verify GC prologue clears the cache safely without dangling pointers.
  gc();
  batch[1][0] = {};
  let afterGC = makeBatchItem();
  assertTrue(%HasDoubleElements(afterGC));
})();

// Test Array constructor ElementsKind transitions.
(function TestArrayConstructorTransitions() {
  gc();
  function makeConstructed() {
    return new Array(1, 2, 3);
  }
  prepareUnoptimized(makeConstructed);

  let a0 = makeConstructed();
  assertTrue(%HasSmiElements(a0));

  let a1 = makeConstructed();
  a1[0] = 1.25;
  assertTrue(%HasDoubleElements(a1));

  let a2 = makeConstructed();
  assertTrue(%HasDoubleElements(a2));
})();

// Test nested array literal inner AllocationSite transitions.
(function TestNestedArrayLiteralTransitions() {
  gc();
  function makeNested() {
    return [[1, 2], [3, 4]];
  }
  prepareUnoptimized(makeNested);

  let n0 = makeNested();
  assertTrue(%HasSmiElements(n0[0]));
  assertTrue(%HasSmiElements(n0[1]));

  let n1 = makeNested();
  n1[1][0] = 1.5;
  assertTrue(%HasDoubleElements(n1[1]));

  let n2 = makeNested();
  assertTrue(%HasSmiElements(n2[0]));
  assertTrue(%HasDoubleElements(n2[1]));
})();

// Test 0-arg and 1-arg Array constructor transitions.
(function TestZeroAndOneArgArrayConstructor() {
  gc();
  function makeZeroArg() {
    return new Array();
  }
  function makeOneArg() {
    return new Array(4);
  }
  prepareUnoptimized(makeZeroArg);
  prepareUnoptimized(makeOneArg);

  let z0 = makeZeroArg();
  assertTrue(%HasSmiElements(z0));
  z0[0] = 2.5;
  assertTrue(%HasDoubleElements(z0));
  let z1 = makeZeroArg();
  assertTrue(%HasDoubleElements(z1));
  z1[0] = 'obj';
  assertTrue(%HasObjectElements(z1));
  let z2 = makeZeroArg();
  assertTrue(%HasObjectElements(z2));

  let o0 = makeOneArg();
  assertTrue(%HasSmiElements(o0));
  assertTrue(%HasHoleyElements(o0));
  o0[0] = 3.5;
  assertTrue(%HasDoubleElements(o0));
  let o1 = makeOneArg();
  assertTrue(%HasDoubleElements(o1));
  assertTrue(%HasHoleyElements(o1));
  o1[0] = 'obj';
  assertTrue(%HasObjectElements(o1));
  assertTrue(%HasHoleyElements(o1));
  let o2 = makeOneArg();
  assertTrue(%HasObjectElements(o2));
  assertTrue(%HasHoleyElements(o2));
})();

// Test warmed-up ElementsTransitionAndStore IC stub hitting
// TrapAllocationSiteInCache.
(function TestWarmedElementsTransitionStub() {
  gc();
  function transitionToDouble(arr, val) {
    arr[0] = val;
  }
  prepareUnoptimized(transitionToDouble);
  function makeDummy1() {
    return [1, 2];
  }
  function makeDummy2() {
    return [1, 2];
  }
  prepareUnoptimized(makeDummy1);
  prepareUnoptimized(makeDummy2);

  // Call 1 makes the IC monomorphic on PACKED_DOUBLE_ELEMENTS; call 2 passes
  // a fresh PACKED_SMI_ELEMENTS array so the IC becomes polymorphic with
  // ElementsTransitionAndStore.
  transitionToDouble(makeDummy1(), 1.5);
  transitionToDouble(makeDummy2(), 1.5);

  function makeFreshSmi() {
    return [10, 20];
  }
  prepareUnoptimized(makeFreshSmi);

  let a0 = makeFreshSmi();
  assertTrue(%HasSmiElements(a0));
  transitionToDouble(a0, 2.5);
  assertTrue(%HasDoubleElements(a0));

  let a1 = makeFreshSmi();
  assertTrue(%HasDoubleElements(a1));
})();

// Test cache collision/eviction (>256 allocations) without crash or corruption.
(function TestCacheCollisionEviction() {
  gc();
  function makeEvicted() {
    return [1, 2];
  }
  function makeOther() {
    return [];
  }
  prepareUnoptimized(makeEvicted);
  prepareUnoptimized(makeOther);

  let first = makeEvicted();
  for (let i = 0; i < 4096; i++) {
    makeOther();
  }
  // `first` has been evicted from the 256-entry direct-mapped cache.
  first[0] = 1.5;
  assertTrue(%HasDoubleElements(first));

  // Clear young generation after the 4,096 allocations so `next` cannot trigger
  // a Scavenge before its elements kind transition.
  gc();

  // Because `first` was evicted, `makeEvicted`'s AllocationSite is still SMI.
  let next = makeEvicted();
  assertTrue(%HasSmiElements(next));
  next[0] = 1.5;
  assertTrue(%HasDoubleElements(next));
  let afterNext = makeEvicted();
  assertTrue(%HasDoubleElements(afterNext));
})();

// Test non-JSArray receiver (object with indexed elements) does not corrupt
// cache.
(function TestNonJSArrayIndexedTransition() {
  gc();
  function storeElement(obj, val) {
    obj[0] = val;
  }
  prepareUnoptimized(storeElement);

  for (let i = 0; i < 5; i++) {
    let o = {};
    o[0] = 1;
    storeElement(o, 1.5);
    assertEquals(1.5, o[0]);
  }
})();

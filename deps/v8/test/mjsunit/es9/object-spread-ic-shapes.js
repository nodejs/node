// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test that cloning the empty object yields the same shape in monomorphic
// state.
(function() {
  function clone(o) {
    return {...o};
  }

  const o = {};
  assertTrue(%HaveSameMap(o, clone(o)));
  %PrepareFunctionForOptimization(clone);
  assertTrue(%HaveSameMap(o, clone(o)));
  assertTrue(%HaveSameMap(o, clone(o)));
  %OptimizeFunctionOnNextCall(clone);
  assertTrue(%HaveSameMap(o, clone(o)));
})();

// Test that cloning an object with a single data field yields the same shape
// in monomorphic state.
(function() {
  function clone(o) {
    return {...o};
  }

  const o = {a: "a"};
  assertTrue(%HaveSameMap(o, clone(o)));
  %PrepareFunctionForOptimization(clone);
  assertTrue(%HaveSameMap(o, clone(o)));
  assertTrue(%HaveSameMap(o, clone(o)));
  %OptimizeFunctionOnNextCall(clone);
  assertTrue(%HaveSameMap(o, clone(o)));
})();

// Test that cloning fast mode objects yields the same shape in polymorphic state.
(function() {
  function clone(o) {
    return {...o};
  }

  const o0 = {};
  const o1 = {a: "a"};
  const o2 = {a: "a", b: "b"};
  assertTrue(%HaveSameMap(o0, clone(o0)));
  assertTrue(%HaveSameMap(o1, clone(o1)));
  assertTrue(%HaveSameMap(o2, clone(o2)));
  %PrepareFunctionForOptimization(clone);
  assertTrue(%HaveSameMap(o0, clone(o0)));
  assertTrue(%HaveSameMap(o1, clone(o1)));
  assertTrue(%HaveSameMap(o2, clone(o2)));
  assertTrue(%HaveSameMap(o0, clone(o0)));
  assertTrue(%HaveSameMap(o1, clone(o1)));
  assertTrue(%HaveSameMap(o2, clone(o2)));
  %OptimizeFunctionOnNextCall(clone);
  assertTrue(%HaveSameMap(o0, clone(o0)));
  assertTrue(%HaveSameMap(o1, clone(o1)));
  assertTrue(%HaveSameMap(o2, clone(o2)));
})();

// Test that cloning the empty object with `null` prototype (in fast mode)
// yields the same shape in monomorphic state.
(function() {
  function clone(o) {
    return {...o, __proto__: null};
  }

  const o = {};
  Object.setPrototypeOf(o, null);
  assertTrue(%HasFastProperties(o));
  assertNull(Object.getPrototypeOf(o));
  assertTrue(%HaveSameMap(o, clone(o)));
  %PrepareFunctionForOptimization(clone);
  assertTrue(%HaveSameMap(o, clone(o)));
  assertTrue(%HaveSameMap(o, clone(o)));
  %OptimizeFunctionOnNextCall(clone);
  assertTrue(%HaveSameMap(o, clone(o)));
})();

// Test that different CloneObjectICs produce the same shape for the same
// object literal inputs with non-default property attributes.
(function() {
  function clone1(o) {
    return {...o};
  }
  function clone2(o) {
    return {...o};
  }

  const o1 = {x: "x", y: "y", z: "z"};
  Object.defineProperty(o1, 'x', {configurable:false, writable:false});
  Object.defineProperty(o1, 'y', {configurable:false});
  Object.defineProperty(o1, 'z', {writable:false});
  assertTrue(%HasFastProperties(o1));

  const o2 = {a: 1, b: 2, c: 3, d: 4};
  Object.defineProperty(o2, 'c', {writable:false});
  Object.defineProperty(o2, 'd', {configurable:false});
  assertTrue(%HasFastProperties(o2));

  for (const o of [o1, o2]) {
    for (const clone of [clone1, clone2]) {
      const c = clone(o);
      assertFalse(%HaveSameMap(o, c));
      assertEquals(Object.keys(o), Object.keys(c));
      for (const key of Object.keys(c)) {
        const d = Object.getOwnPropertyDescriptor(c, key);
        assertTrue(d.configurable);
        assertTrue(d.enumerable);
        assertTrue(d.writable);
        assertEquals(d.value, o[key]);
      }
    }
  }
  assertTrue(%HaveSameMap(clone1(o1), {x: "x", y: "y", z: "z"}));
  assertTrue(%HaveSameMap(clone2(o2), {a: 1, b: 2, c: 3, d: 4}));
  %PrepareFunctionForOptimization(clone1);
  %PrepareFunctionForOptimization(clone2);
  assertTrue(%HaveSameMap(clone1(o1), clone2(o1)));
  assertTrue(%HaveSameMap(clone1(o1), clone2(o1)));
  assertTrue(%HaveSameMap(clone1(o2), clone2(o2)));
  assertTrue(%HaveSameMap(clone1(o2), clone2(o2)));
  assertTrue(%HaveSameMap(clone1(o1), {x: "x", y: "y", z: "z"}));
  assertTrue(%HaveSameMap(clone2(o2), {a: 1, b: 2, c: 3, d: 4}));
  %OptimizeFunctionOnNextCall(clone1);
  %OptimizeFunctionOnNextCall(clone2);
  assertTrue(%HaveSameMap(clone1(o1), clone2(o1)));
  assertTrue(%HaveSameMap(clone1(o2), clone2(o2)));
  assertTrue(%HaveSameMap(clone1(o1), {x: "x", y: "y", z: "z"}));
  assertTrue(%HaveSameMap(clone2(o2), {a: 1, b: 2, c: 3, d: 4}));
})();

// Test that different CloneObjectICs produce the same shape for trivial
// constructor instances in monomorphic state.
(function() {
  function clone1(o) {
    return {...o};
  }
  function clone2(o) {
    return {...o};
  }

  class A {
    constructor() {
      this.a = 1;
      this.b = 2;
      this.c = 3;
      this.d = 4;
    }
  };
  for (let i = 0; i < 10; ++i) new A();  // Finish slack tracking

  assertTrue(%HaveSameMap(clone1(new A()), clone1(new A())));
  assertTrue(%HaveSameMap(clone1(new A()), clone2(new A())));
  assertTrue(%HaveSameMap(clone2(new A()), clone1(new A())));
  assertTrue(%HaveSameMap(clone2(new A()), clone2(new A())));
  %PrepareFunctionForOptimization(clone1);
  %PrepareFunctionForOptimization(clone2);
  assertTrue(%HaveSameMap(clone1(new A()), clone1(new A())));
  assertTrue(%HaveSameMap(clone1(new A()), clone2(new A())));
  assertTrue(%HaveSameMap(clone2(new A()), clone1(new A())));
  assertTrue(%HaveSameMap(clone2(new A()), clone2(new A())));
  assertTrue(%HaveSameMap(clone1(new A()), clone1(new A())));
  assertTrue(%HaveSameMap(clone1(new A()), clone2(new A())));
  assertTrue(%HaveSameMap(clone2(new A()), clone1(new A())));
  assertTrue(%HaveSameMap(clone2(new A()), clone2(new A())));
  %OptimizeFunctionOnNextCall(clone1);
  %OptimizeFunctionOnNextCall(clone2);
  assertTrue(%HaveSameMap(clone1(new A()), clone1(new A())));
  assertTrue(%HaveSameMap(clone1(new A()), clone2(new A())));
  assertTrue(%HaveSameMap(clone2(new A()), clone1(new A())));
  assertTrue(%HaveSameMap(clone2(new A()), clone2(new A())));
})();

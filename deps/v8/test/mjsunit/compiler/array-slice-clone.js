// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-stress-flush-code
// Flags: --no-lazy-feedback-allocation


// Test CloneFastJSArray inserted by JSCallReducer for Array.prototype.slice.
// CloneFastJSArray produces COW arrays if the original array is COW.

// Trigger JSCallReducer on slice() and slice(0)
(function() {
  const arr = [1,2,3,4,5];

  function slice() {
    return arr.slice();
  }

  function slice0() {
    return arr.slice(0);
  }

  %PrepareFunctionForOptimization(slice0);
  %PrepareFunctionForOptimization(slice);

  assertEquals(arr, slice());
  assertFalse(arr === slice());
  assertEquals(slice(), slice0());
  assertEquals(slice0(), slice());

  %OptimizeFunctionOnNextCall(slice0);
  assertEquals(slice(), slice0());
  %OptimizeFunctionOnNextCall(slice);

  assertEquals(slice(), slice0());
  assertOptimized(slice); assertOptimized(slice0);
})();

// This will cause deopt of slice by a CheckMap installed by
// JSNativeContextSpecialization::ReduceNamedAccess
(function() {
  const arr = [1,2,3,4,5];

  function slice() {
    return arr.slice();
  }

  %PrepareFunctionForOptimization(slice);

  assertEquals(arr, slice());
  assertEquals(slice(), arr);

  %OptimizeFunctionOnNextCall(slice);
  slice();

  // Trigger deopt here
  arr.push(7.2);
  assertEquals(slice()[5], 7.2);
})();

// There should not be a deopt cycle.
(function() {
  const arr = [1,2,3,4,5];

  function slice() {
    return arr.slice();
  }

  %PrepareFunctionForOptimization(slice);

  assertEquals(arr, slice());
  assertEquals(slice(), arr);

  %OptimizeFunctionOnNextCall(slice);
  // Trigger opt
  assertEquals(slice(), arr);

  // Trigger deopt by CheckMap from JSNativeContextSpecialization
  arr.push(7.2);
  slice();

  %PrepareFunctionForOptimization(slice);
  %OptimizeFunctionOnNextCall(slice);
  // Trigger opt again
  slice();

  // Should not deopt again
  arr.push(8.2);
  slice();
  assertOptimized(slice);
})();

// JSCallReducer will not reduce because the species has been modified
(function() {
  const array = [3,4,5];

  function slice(){
    return array.slice();
  }

  class MyArray extends Array {};
  array.constructor = MyArray;

  %PrepareFunctionForOptimization(slice);

  slice(); slice();

  %OptimizeFunctionOnNextCall(slice);
  var narr = slice();
  assertInstanceof(narr, MyArray);
})();

(function() {
  const array = [3,4,5];

  function slice(){
    return array.slice();
  }

  %PrepareFunctionForOptimization(slice);

  slice(); slice();

  %OptimizeFunctionOnNextCall(slice);

  slice();

  class MyArray extends Array {};
  array.constructor = MyArray;
  // deopt
  var narr = slice();
  // if not deopt, narr will be instanceof Array
  assertTrue(narr instanceof MyArray);
})();

// JSCallReducer adds check for UnreliableReceiverMaps
(function() {
  const arr = [1,2,3,4,5];

  function slice() {
    return arr.slice();
  }

  %PrepareFunctionForOptimization(slice);

  slice(); slice();
  arr.foo = 6.2;

  %OptimizeFunctionOnNextCall(slice);
  // JSCallReducer will add check for UnreliableReceiverMaps
  slice();

  // Trigger deopt because of DependOnStableMaps
  // installed by JSNativeContextSpecialization,
  // but not the check installed by ReduceArrayPrototypeSlice itself
  arr.bar = 7.2;

  let narr = slice();
  assertEquals(arr, narr);
  assertEquals(narr.foo, undefined);
  assertEquals(narr.bar, undefined);
})();

// Multiple maps
(function() {
  const iarr = [1,2,3];
  const darr = [2.1, 3.3, 0.2];

  function slice(arr) {
    return arr.slice();
  }

  %PrepareFunctionForOptimization(slice);

  slice(iarr); slice(darr);
  slice(iarr); slice(darr);

  %OptimizeFunctionOnNextCall(slice);
  // The optimization works for both maps
  assertEquals(iarr, slice(iarr));
  assertEquals(darr, slice(darr));
  assertOptimized(slice);
})();

// Tests for the branch of CanInlineArrayIteratingBuiltin

// JSCallReducer will not reduce to CloneFastJSArray
// if array's prototype is not JS_ARRAY_TYPE
(function () {
  class MyArray extends Array {
    constructor() {
      super();
      this[6]= 6;
    }
  }
  let array = new MyArray(3, 5, 4);

  function slice() {
    return array.slice();
  }

  %PrepareFunctionForOptimization(slice);

  assertEquals(slice(),array);
  slice();

  %OptimizeFunctionOnNextCall(slice);
  let narr = slice();
  // here, slice supposes to call MyArray's constructor.
  // If we optimize with CloneFastJSArray, Array's constructor is called instead.
  assertEquals(narr[6], 6);
  assertTrue(narr instanceof MyArray);
})();

// JSCallReducer will not reduce to CloneFastJSArray
// if array's instance type is not JS_ARRAY_TYPE.
// CloneFastJSArray does not work with non JS_ARRAY_TYPE.
// Check : receiver_map->instance_type() == JS_ARRAY_TYPE
(function () {
  var x = {"0" : 0, "2": 2} ;
  x.__proto__ = Array.prototype;

  function slice() {
    return x.slice();
  }

  %PrepareFunctionForOptimization(slice);

  slice(); slice();

  %OptimizeFunctionOnNextCall(slice);
  assertEquals(slice(), []);
})();

// JSCallReducer will not reduce to CloneFastJSArray
// since array is not Fast Elements Kind
// Check : IsFastElementsKind(receiver_map->elements_kind())
(function () {
  var array = [3, 4, 5];

  function slice() {
    return array.slice();
  }

  %PrepareFunctionForOptimization(slice);

  assertEquals(slice(),array);
  slice();

  // a sparse array switches to Dictionary Elements
  array[9999] = 0;
  %OptimizeFunctionOnNextCall(slice);
  var narr = slice();
  assertEquals(narr, array);
})();

(function () {
  var array = [3, 4, 5];

  function slice() {
    return array.slice();
  }

  %PrepareFunctionForOptimization(slice);

  assertEquals(slice(),array);
  slice();

  %OptimizeFunctionOnNextCall(slice);
  slice();

  // a sparse array switches to Dictionary Elements
  array[9999] = 0;
  // trigger deopt because map changes
  assertEquals(slice(),array);
})();

// JSCallReducer will not reduce to CloneFastJSArray
// if array is used as a prototype and has unstable map
(function () {
  var array = [3, 5, 4];

  function slice(arr) {
    return arr.slice();
  }

  %PrepareFunctionForOptimization(slice);

  // make array's map is_prototype_map()
  var x = {__proto__ : array};

  assertEquals(slice(array),array);
  slice(array);

  // make array's map unstable
  array.push(6.3);
  slice(array);

  %OptimizeFunctionOnNextCall(slice);

  assertEquals(slice(array),array);
})();

// JSCallReducer will not reduce to CloneFastJSArray
// if the Array prototype got some elements.
// Check: isolate->IsNoElementsProtectorIntact()
(function () {
  var array = [, 6, 6];

  function slice() {
    return array.slice();
  }

  %PrepareFunctionForOptimization(slice);

  assertEquals(slice(),array);
  slice();

  array.__proto__.push(6);

  %OptimizeFunctionOnNextCall(slice);

  // if we optimized, we would get [ , 6, 6]
  // here, slice copies elements from both the object and the prototype
  let narr = slice();
  assertNotEquals(Object.getOwnPropertyDescriptor(narr,0), undefined);
  assertEquals(narr, [6, 6, 6]);
})();

(function () {
  var array = [, 6, 6];

  function slice() {
    return array.slice();
  }

  %PrepareFunctionForOptimization(slice);

  assertEquals(slice(),array);
  slice();

  %OptimizeFunctionOnNextCall(slice);
  slice();

  // Deopt
  array.__proto__.push(6);
  let narr = slice();
  assertNotEquals(Object.getOwnPropertyDescriptor(narr, 0), undefined);
  assertEquals(narr[0], 6);
})();

// JSCallReducer will not reduce to CloneFastJSArray
// if the Array prototype is not original
// Check: isolate->IsAnyInitialArrayPrototype(receiver_prototype)
(function () {
  var array = [6, , 6];

  function slice() {
    return array.slice();
  }

  %PrepareFunctionForOptimization(slice);

  assertEquals(slice(),array);
  slice();

  // change the prototype
  array.__proto__ = [ , 6, ];

  %OptimizeFunctionOnNextCall(slice);
  let narr = slice();
  // if optimized, we would get [6, , 6]
  assertNotEquals(Object.getOwnPropertyDescriptor(narr, 1), undefined);
  assertEquals(narr, [6,6,6]);
})();

(function () {
  var array = [6, ,6];

  function slice() {
    return array.slice();
  }

  %PrepareFunctionForOptimization(slice);

  assertEquals(slice(),array);
  slice();

  %OptimizeFunctionOnNextCall(slice);
  slice();

  // change the prototype
  array.__proto__ = [,6,];
  // deopt because of map changed
  let narr = slice();

  // if optimized, we would get [6, , 6]
  assertNotEquals(Object.getOwnPropertyDescriptor(narr, 1), undefined);
  assertEquals(narr, [6,6,6]);
})();

// Packed
// Trigger JSCallReducer on slice() and slice(0)
(function() {
  // Non-extensible:
  var arr = Object.preventExtensions([1,2,'a',4,5]);

  function slice() {
    return arr.slice();
  }

  function slice0() {
    return arr.slice(0);
  }

  function test() {
    %PrepareFunctionForOptimization(slice0);
    %PrepareFunctionForOptimization(slice);

    assertEquals(arr, slice());
    assertFalse(arr === slice());
    assertEquals(slice(), slice0());
    assertEquals(slice0(), slice());

    %OptimizeFunctionOnNextCall(slice0);
    assertEquals(slice(), slice0());
    %OptimizeFunctionOnNextCall(slice);

    assertEquals(slice(), slice0());
    assertOptimized(slice); assertOptimized(slice0);
  }
  test();

  // Sealed
  arr = Object.seal([1,2,'a',4,5]);
  test();

  // Frozen
  arr = Object.freeze([1,2,'a',4,5]);
  test();
})();

// Holey
// Trigger JSCallReducer on slice() and slice(0)
(function() {
  // Non-extensible:
  var arr = Object.preventExtensions([,1,2,'a',4,5]);

  function slice() {
    return arr.slice();
  }

  function slice0() {
    return arr.slice(0);
  }

  function test() {
    %PrepareFunctionForOptimization(slice0);
    %PrepareFunctionForOptimization(slice);
    assertEquals(arr, slice());
    assertFalse(arr === slice());
    assertEquals(slice(), slice0());
    assertEquals(slice0(), slice());

    %OptimizeFunctionOnNextCall(slice0);
    assertEquals(slice(), slice0());
    %OptimizeFunctionOnNextCall(slice);

    assertEquals(slice(), slice0());
    assertOptimized(slice0);
    assertOptimized(slice);
  }
  test();

  // Sealed
  arr = Object.seal([,1,2,'a',4,5]);
  test();

  // Frozen
  arr = Object.freeze([,1,2,'a',4,5]);
  test();
})();

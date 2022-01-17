// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Overwrite the value for --noverify-heap and
// --noenable-slow-asserts, which the test runner already set to true before.
//  Due to flag contradiction checking, this requires
// --allow-overwriting-for-next-flag to avoid an error.
// Flags: --allow-overwriting-for-next-flag --noverify-heap
// Flags: --allow-overwriting-for-next-flag --noenable-slow-asserts
// Flags: --allow-natives-syntax --opt --no-always-opt

// --noverify-heap and --noenable-slow-asserts are set because the test is too
// slow with it on.

// Ensure that keyed stores work, and optimized functions learn if the
// store required change to dictionary mode. Verify that stores that grow
// the array into large object space don't cause a deopt.
(function() {
  var a = [];

  function foo(a, i) {
    a[i] = 5.3;
  }

  %PrepareFunctionForOptimization(foo);
  foo(a, 1);
  foo(a, 2);
  foo(a, 3);
  %OptimizeFunctionOnNextCall(foo);
  a[3] = 0;
  foo(a, 3);
  assertEquals(a[3], 5.3);
  foo(a, 50000);
  // TODO(v8:11457) We don't currently support inlining element stores if there
  // is a dictionary mode prototypes on the prototype chain. Therefore, if
  // v8_dict_property_const_tracking is enabled, the optimized code only
  // contains a call to the IC handler and doesn't get deopted.
  assertEquals(%IsDictPropertyConstTrackingEnabled(), isOptimized(foo));
  assertTrue(%HasDictionaryElements(a));

  %PrepareFunctionForOptimization(foo);
  var b = [];
  foo(b, 1);
  foo(b, 2);
  // Put b in dictionary mode.
  b[10000] = 5;
  assertTrue(%HasDictionaryElements(b));
  foo(b, 3);
  %PrepareFunctionForOptimization(foo);
  %OptimizeFunctionOnNextCall(foo);
  foo(b, 50000);
  assertOptimized(foo);
  assertTrue(%HasDictionaryElements(b));

  // Clearing feedback for the StoreIC in foo is important for runs with
  // flag --stress-opt.
  %ClearFunctionFeedback(foo);
})();


(function() {
  var a = new Array(10);

  function foo2(a, i) {
    a[i] = 50;
  }

  // The KeyedStoreIC will learn GROW_MODE.
  %PrepareFunctionForOptimization(foo2);
  foo2(a, 10);
  foo2(a, 12);
  foo2(a, 31);
  %OptimizeFunctionOnNextCall(foo2);
  foo2(a, 40);

  assertOptimized(foo2);
  assertTrue(%HasSmiElements(a));

  // Grow a large array into large object space through the keyed store
  // without deoptimizing. Grow by 9s. If we set elements too sparsely, the
  // array will convert to dictionary mode.
  a = new Array(99999);
  assertTrue(%HasSmiElements(a));
  for (var i = 0; i < 263000; i += 9) {
    foo2(a, i);
  }

  // Verify that we are over 1 page in size, and foo2 remains optimized.
  // This means we've smoothly transitioned to allocating in large object
  // space.
  assertTrue(%HasSmiElements(a));
  assertTrue(a.length * 4 > (1024 * 1024));
  assertOptimized(foo2);

  %ClearFunctionFeedback(foo2);
})();

// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --noalways-turbofan

// Test that JSResolvePromise takes a proper stability dependency
// on the resolutions map if the infer receiver maps are unreliable
// (as is the case for HeapConstants).
(function() {
  // We need an object literal which gets a stable map initially.
  function makeObjectWithStableMap() {
    return {a:1, b:1, c:1};
  }
  const a = makeObjectWithStableMap();

  function foo() {
    return Promise.resolve(a);
  }

  %PrepareFunctionForOptimization(foo);
  assertInstanceof(foo(), Promise);
  assertInstanceof(foo(), Promise);
  %OptimizeFunctionOnNextCall(foo);
  assertInstanceof(foo(), Promise);
  assertOptimized(foo);

  // Now invalidate the stability of a's map.
  const b = makeObjectWithStableMap();
  b.d = 1;

  if (%IsDictPropertyConstTrackingEnabled()) {
    // TODO(v8:11457) In this mode we weren't able to inline the access, yet, so
    // it stays optimized. See related TODO in
    // JSNativeContextSpecialization::ReduceJSResolvePromise.
    return;
  }

  // This should deoptimize foo.
  assertUnoptimized(foo);
})();

// Same test with async functions.
(function() {
  // We need an object literal which gets a stable map initially,
  // it needs to be different from the above, otherwise the map
  // is already not stable when we get here.
  function makeObjectWithStableMap() {
    return {x:1, y:1};
  }
  const a = makeObjectWithStableMap();

  async function foo() {
    return a;
  }

  %PrepareFunctionForOptimization(foo);
  assertInstanceof(foo(), Promise);
  assertInstanceof(foo(), Promise);
  %OptimizeFunctionOnNextCall(foo);
  assertInstanceof(foo(), Promise);
  assertOptimized(foo);

  // Now invalidate the stability of a's map.
  const b = makeObjectWithStableMap();
  b.z = 1;

  if (%IsDictPropertyConstTrackingEnabled()) {
    // TODO(v8:11457) In this mode we weren't able to inline the access, yet, so
    // it stays optimized. See related TODO in
    // JSNativeContextSpecialization::ReduceJSResolvePromise.
    return;
  }

  // This should deoptimize foo.
  assertUnoptimized(foo);
})();

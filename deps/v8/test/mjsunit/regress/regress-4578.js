// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --turbofan --no-concurrent-recompilation
// Flags: --no-always-turbofan --no-assert-types

// This weak ref is for checking whether the closure-allocated object o got
// collected as it should.
var weak;

// Return a function which refers to a variable from its enclosing function.
function makeFn() {
  var o = {};
  weak = new WeakRef(o);
  o.num = 0;
  return () => { return ++o.num; };
}

// Simple wrapper function which will cause inlining.
function g(f) {
  f();
}

// Optimize g so that it inlines a specific function instance created by makeFn,
// and then drop all user-visible references to that function instance.
(function () {
  var fn = makeFn();
  %PrepareFunctionForOptimization(g);
  %PrepareFunctionForOptimization(fn);
  g(fn);
  %OptimizeFunctionOnNextCall(g);
  g(fn);
})();

// WeakRefs created during the current microtask are strong, so defer the rest.
setTimeout(() => {
  // Since the function inlined by g no longer exists, it should deopt and
  // release the inner function.
  // We need to invoke GC asynchronously and wait for it to finish, so that
  // it doesn't need to scan the stack. Otherwise, the objects may not be
  // reclaimed because of conservative stack scanning and the test may not
  // work as intended.
  (async function () {
    await gc({ type: 'major', execution: 'async' });

    // Check that the memory leak described in v8:4578 no longer happens.
    assertEquals(undefined, weak.deref());
  })();
}, 0);

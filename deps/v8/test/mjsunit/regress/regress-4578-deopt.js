// Copyright 2023 the V8 project authors. All rights reserved.
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

// This is the opposite case of regress-4578.js: if an optimized function's
// Code is currently running at the time of gc, then it must keep its
// deoptimization data alive.

// A wrapper function which causes a GC internally.
function h(f) {
  gcAndCheckState();  // Non-inlined call
  f();  // Inlined call
}

var doCheck = false;
function gcAndCheckState() {
  if (!doCheck) return;

  // It's possible that h has already deoptimized by this point if
  // --stress-incremental-marking caused additional gc cycles.
  var optimized = isOptimized(h);

  gc();

  // If the optimized code for h is still on the stack, then it must not clear
  // out its own deoptimization data. Eventually h will deopt due to the wrong
  // function being passed, but only after this function has returned.
  if (optimized) {
    assertNotEquals(undefined, weak.deref());
  } else {
    // Due to conservative stack scanning, which could keep objects from being
    // reclaimed by the above GC, it is not always guaranteed that the weak
    // reference will have been cleared if the function has been deoptimized.
  }
}

// Don't inline gcAndCheckState, to avoid the possibility that its content
// could cause h to deoptimize.
%NeverOptimizeFunction(gcAndCheckState);

// Optimize h to inline a specific function instance, and then drop all
// user-visible references to that inlined function.
(function () {
  var fn = makeFn();
  %PrepareFunctionForOptimization(h);
  %PrepareFunctionForOptimization(fn);
  h(fn);
  %OptimizeFunctionOnNextCall(h);
  h(fn);
})();

// Defer again so the WeakRef can expire.
setTimeout(() => {
  doCheck = true;
  h(() => {});
}, 0);

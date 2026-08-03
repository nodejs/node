// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --allow-natives-syntax --verify-heap

var o = { f : 0 };

var shouldDeopt = true;

// This tests a scenario where a function has an embedded object reference,
// the function is lazy-deoptimized, the object is then collected, but the
// code object keeps the dangling pointer.

function deopt() {
  if (shouldDeopt) {
    // Change the global object. This deoptimizes function f because
    // it optimistically embedded the reference to o as a constant.
    o = { f : 2 };
    // Collect the original object o; at this point, f should invalidate
    // its invalid reference to the original object.
    gc();
  }
}

// Forwarding function to make sure that function f is not the topomost
// optimized frame (GC treats reference from topmost optimized code strongly).
function dummy_opt() { deopt(); }
function dummy() { dummy_opt(); }
%NeverOptimizeFunction(deopt);
%NeverOptimizeFunction(dummy);

// When optimized, the function f embeds the constant reference
// to the original object o.
function f() {
  dummy();
  return o.f;
}

shouldDeopt = false;
%PrepareFunctionForOptimization(dummy_opt);
f();
f();
%OptimizeFunctionOnNextCall(dummy_opt);
f();
%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);

shouldDeopt = true;
assertEquals(2, f());

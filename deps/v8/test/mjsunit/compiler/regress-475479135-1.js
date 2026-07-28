// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar(o, b) {
  if (b) {
    // Transitioning o.x to representation Tagged instead of HeapObject.
    o.x = 42;
  }
}
%NeverOptimizeFunction(bar);

function foo(o, b) {
  bar(o, b);
  // Storing a new out-of-object property (will resize backing store).
  o.a = 42;
}

let o1 = { };
let o2 = { };
let o3 = { };
// The first properties will be in-object
for (let i = 0; i < 4; i++) {
  o1["p" + i] = 42;
  o2["p" + i] = 42;
  o3["p" + i] = 42;
}
// Recording 3 out-of-object properties (so that the next added property needs
// to resizes the property backing store).
for (let p of ["x", "y", "z"]) {
  o1[p] = {};
  o2[p] = {};
  o3[p] = {};
}

%PrepareFunctionForOptimization(foo);
foo(o1, false);

%OptimizeFunctionOnNextCall(foo);
foo(o2, false);

// Triggering the transition.
foo(o3, true);

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

// Creating an object with a non-const HeapObject field.
let o = { x : "abc" };
o.x = [];

function foo(c) {
  let phi = c ? 0x7fffffff : 42;
  if (c) {
    // HeapObject store.
    o.x = phi;
  } else {
    // Int32 use.
    phi + 1;
  }
}

%PrepareFunctionForOptimization(foo);
foo(false);
foo(true);

%OptimizeFunctionOnNextCall(foo);
foo(false);

o.x = "abc";
foo(true);
assertEquals(0x7fffffff, o.x);

// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --maglev --allow-natives-syntax

function f(a, b) {
  // This access site has seen PACKED_DOUBLE_ELEMENTS (transition to
  // HOLEY_DOUBLE_ELEMENTS).
  b[13] = 0;

  // This access site has seen HOLEY_DOUBLE_ELEMENTS and HOLEY_ELEMENTS.
  // HOLEY_DOUBLE_ELEMENTS can transition to HOLEY_ELEMENTS.
  a[0] = 0;

  // This access site has seen only HOLEY_DOUBLE_ELEMENTS.
  b[0] = 9.431092e-317;
}
%PrepareFunctionForOptimization(f);

// Call 1
let o1a = new Array(1);
o1a[0] = 'a'; // HOLEY_ELEMENTS

let o1b = [2147483648]; // PACKED_DOUBLE_ELEMENTS

// Before:
// HOLEY_ELEMENTS, PACKED_DOUBLE_ELEMENTS
f(o1a, o1b);
// After:
// HOLEY_ELEMENTS, HOLEY_DOUBLE_ELEMENTS

// Call 2
let o2a = [0.1]; // PACKED_DOUBLE_ELEMENTS

// Before:
// PACKED_DOUBLE_ELEMENTS, PACKED_DOUBLE_ELEMENTS
f(o2a, o2a);
// After:
// HOLEY_DOUBLE_ELEMENTS, HOLEY_DOUBLE_ELEMENTS

// Call 3
let o3 = [, 0.2];

%OptimizeMaglevOnNextCall(f);

// Before: HOLEY_DOUBLE_ELEMENTS, HOLEY_DOUBLE_ELEMENTS.
f(o3, o3);

// The bug was: after the first store (b[13] = 0) we know what b has the map
// HOLEY_DOUBLE_ELEMENTS. However, we failed to track that that map is
// unstable. For the second store (a[0] = 0), we create a transition from
// HOLEY_DOUBLE_ELEMENTS to HOLEY_ELEMENTS. This will also transition b because
// they're the same object. And since we failed to track that the
// HOLEY_DOUBLE_ELEMENTS map is unstable, we didn't take into account that b
// might change maps while the code is executing.

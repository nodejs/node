// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax

function foo(arr, do_store) {
  // Load from arr.
  arr[0];

  if (do_store) {
    // Store to arr.
    arr[0] = 0.2;

    arr instanceof foo;
  }
}

%PrepareFunctionForOptimization(foo);
// Initialise the load and store with PACKED_DOUBLE_ELEMENTS feedback.
foo([0.5], true);
// Transition just the load to PACKED_ELEMENTS feedback.
foo([{}], false);
%OptimizeMaglevOnNextCall(foo);
// The compilation should succeed here despite `arr instanceof foo` trying to
// read an impossible map (the load checks for PACKED_ELEMENTS, the store checks
// for PACKED_DOUBLE_ELEMENTS).
foo([{}], false);

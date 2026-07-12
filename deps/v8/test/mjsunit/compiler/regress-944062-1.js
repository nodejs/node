// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const array = [42, 2.1];  // non-stable map (PACKED_DOUBLE)
let b = false;

function f() {
  if (b) array[100000] = 4.2;  // go to dictionary mode
  return 42;
};
%NeverOptimizeFunction(f);

function includes() {
  return array.includes(f());
};
%PrepareFunctionForOptimization(includes);
assertTrue(includes());
assertTrue(includes());
%OptimizeFunctionOnNextCall(includes);
assertTrue(includes());
b = true;
assertTrue(includes());

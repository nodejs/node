// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-object-tracking --maglev

let f64 = new Float64Array(1);
f64[0] = 4;

function foo() {
  let x = Math.sqrt(f64[0]);
  let arr = new Array(x);
  let iter = arr.values();
  return iter.next();
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeMaglevOnNextCall(foo);
foo();
assertOptimized(foo);

// Triggering deopt
f64[0] = 2.26; // not a perfect square
assertThrows(() => foo(), RangeError, "Invalid array length");
assertUnoptimized(foo);

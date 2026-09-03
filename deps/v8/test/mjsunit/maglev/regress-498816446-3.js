// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-object-tracking --maglev

let ta = new Int32Array(4);

function foo(ta) {
  let x = ta.length;
  let arr = new Array(x);
  let iter = arr.values();
  return iter.next();
}

%PrepareFunctionForOptimization(foo);
foo(ta);
foo(ta);

%OptimizeMaglevOnNextCall(foo);
foo(ta);
assertOptimized(foo);

// Triggering deopt.
var skip = false;
try {
  ta = new Int32Array(0xffffffff+1);
} catch (e) {
  if (/Array buffer allocation failed/.test(e.message)) {
    skip = true;  // We don't have enough memory, just skip the test.
  } else {
    throw e;
  }
}

if (!skip) {
  assertThrows(() => foo(ta), RangeError, "Invalid array length");
  assertUnoptimized(foo);
}

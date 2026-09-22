// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-object-tracking --maglev

let hf_arr = [, 2.3, , 4.5];
hf_arr[1] = 4.5;

function foo(early_return) {
  let x = hf_arr[0];
  if (early_return) return;
  let arr = new Array(x);
  let iter = arr.values();
  return iter.next();
}

%PrepareFunctionForOptimization(foo);
foo(true);
hf_arr[0] = 4;
foo(false);

%OptimizeMaglevOnNextCall(foo);
foo(false);
assertOptimized(foo);

// Triggering deopt for a non-uint32 float64.
hf_arr[0] = 3.35;
assertThrows(() => foo(false), RangeError, "Invalid array length");
assertUnoptimized(foo);

// Reoptimizing.
hf_arr[0] = 4;

%OptimizeMaglevOnNextCall(foo);
foo(false);
assertOptimized(foo);

// Triggering deopt for a hole.
delete hf_arr[0];
foo(false);
assertUnoptimized(foo);

// (yes, there is a deopt loop)

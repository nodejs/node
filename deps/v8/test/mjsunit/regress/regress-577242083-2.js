// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --maglev --maglev-non-eager-inlining --allow-natives-syntax

let holey_smi = [1, , 2];
const startsWith = String.prototype.startsWith;

function id(a) { return a; }
%PrepareFunctionForOptimization(id);
%DisableEagerInlining(id);

function foo(arr, b) {
  let x = arr[0];
  let u = x | id(0);
  let y = id(x);
  if (b) {
    startsWith.call(y, "a");
    return u + 0.5;
  }
  return u;
}
%PrepareFunctionForOptimization(foo);
foo(holey_smi, true);
foo(holey_smi, false);
%OptimizeMaglevOnNextCall(foo);
foo(holey_smi, false);

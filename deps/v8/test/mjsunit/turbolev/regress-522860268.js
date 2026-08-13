// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-non-eager-inlining

const entries_fn = Array.prototype.entries;

function helper() {
  let x = 1;
  x += 1; x += 1; x += 1; x += 1; x += 1;
  x += 1; x += 1; x += 1; x += 1; x += 1;
  return [1, 2];
}
function foo(cond) {
  let array = helper();
  let it1;
  if (cond) {
    it1 = entries_fn.call(array);
  }
  let it2 = entries_fn.call(array);
  return [it1, it2];
}

%PrepareFunctionForOptimization(helper);
%PrepareFunctionForOptimization(foo);
foo(true);
%OptimizeMaglevOnNextCall(foo);
foo(false);

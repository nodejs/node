// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function f(value) {
  return value.x === 'abc';
}

let o1 = { x : 'abc' };
let o2 = { y : 'no' };

%PrepareFunctionForOptimization(f);
f(o1);
f(o2);
f(4);
// The feedback of `f` now contains 2 kNotFound maps (one for `o2`'s map, and
// one for HeapNumber map).

%OptimizeMaglevOnNextCall(f);
assertEquals(f(4), false);
assertOptimized(f);

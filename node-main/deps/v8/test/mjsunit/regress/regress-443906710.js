// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let obj = {
  y: 19.5
};

let undef = [undefined];
function foo(max) {
  let val = undef[0];
  for (let i = 0; i < max; i += 1) {
    i += val;
    val = 40;
  }
  obj.y = val;
}

%PrepareFunctionForOptimization(foo);
foo(1);
%OptimizeMaglevOnNextCall(foo);
foo(0);
assertEquals(undefined, obj.y);

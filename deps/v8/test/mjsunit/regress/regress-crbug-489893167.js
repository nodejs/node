// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var t2 = { prop_a: 1, prop_b: {}, prop_c: 1 };
function f() {
  return {
    prop_a: 1,
    prop_b: { prop_a: 1.1, prop_b: {}, prop_c: 1 },
    prop_c: {}
  };
}
for (let i = 0; i < 100; i++) {
  f();
}
%PrepareFunctionForOptimization(f);
%OptimizeFunctionOnNextCall(f);
f();

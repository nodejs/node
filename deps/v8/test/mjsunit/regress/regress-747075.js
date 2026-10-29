// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

r = [
  14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
  14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14
];

function f() {
  r2 = r.map(function(y) {
    return y / 64;
  });
  assertTrue(r2[0] < 1);
};
%PrepareFunctionForOptimization(f);
for (let i = 0; i < 1000; ++i) f();
for (let i = 0; i < 1000; ++i) f();
%OptimizeFunctionOnNextCall(f);
for (let i = 0; i < 1000; ++i) f();

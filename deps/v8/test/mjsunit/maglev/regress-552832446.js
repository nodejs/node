// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(obj, ...args) {
  obj['toString'](...args);
}
%PrepareFunctionForOptimization(f);

function g(x) {
  /1/.test(x);
  for (let i = 0; i < x.length; ++i) {
    for (let j = 0; j < 1; j++) {
      x--;
    }
  }
  f(true, x);
}
%PrepareFunctionForOptimization(g);
g(1);
%OptimizeMaglevOnNextCall(g);
g(1);

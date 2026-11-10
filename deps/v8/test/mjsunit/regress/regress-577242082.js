// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --maglev --allow-natives-syntax

[1].values().extra = 1;

function sink(...args) {}
%PrepareFunctionForOptimization(sink);

function helper(o, cond) {
  if (cond) {
    sink(o.a);
  }
}
%PrepareFunctionForOptimization(helper);

function g(cond) {
  let iter = [1].values();
  delete {}.a;
  helper(iter, cond);
}
%PrepareFunctionForOptimization(g);

helper({ a: 42 }, true);

g(false);
%OptimizeMaglevOnNextCall(g);
g(false);

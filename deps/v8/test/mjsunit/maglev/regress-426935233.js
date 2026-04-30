// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function empty() {}
%PrepareFunctionForOptimization(empty);
function f0(a1, p) {
  if (p) {
    a1 == "stuff";
    empty(12345 == !4);
  }
}
%PrepareFunctionForOptimization(f0);
f0("a", true);
function f13(p) {
  f0(12345, p);
}
%PrepareFunctionForOptimization(f13);
f13(false);
%OptimizeMaglevOnNextCall(f13);
f13(false);

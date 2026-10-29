// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var g;
function noneager() {
  if (g()) fail(x);
  if (x) {}
};

function greedy() {
  noneager();
}

function foo() {
  try {
    greedy();
  } catch (e) {}
}

%PrepareFunctionForOptimization(noneager);
%PrepareFunctionForOptimization(greedy);
%PrepareFunctionForOptimization(foo);
foo();
%OptimizeMaglevOnNextCall(foo);
foo();

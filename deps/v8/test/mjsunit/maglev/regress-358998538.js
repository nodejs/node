// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function bar() {
  let x = undefined;
  x++;
  glo = x;
  return x;
}

const o = {};
o[Symbol.iterator] = bar;

function foo() {
  for (let i = 0; i < 2; i++) {
    try { [] = o; } catch {}
  }
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo();
%OptimizeMaglevOnNextCall(foo);
foo();

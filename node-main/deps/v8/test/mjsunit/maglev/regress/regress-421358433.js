// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

'use strict';
function inl(x) {
  try {
    return x();
  } catch (e) {}
}
function foo() {
  for (let i = 0; i < 25; i++) {
    const x = inl(() => undef + 1);
    try {
      undef = x | foo;
    } catch (e) {}
  }
}
%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(inl);
foo();
%OptimizeFunctionOnNextCall(foo);
foo();

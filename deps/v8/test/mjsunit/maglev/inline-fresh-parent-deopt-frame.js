// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --maglev-inlining

function inlined(x) {
  return x + x;
}

function foo(y) {
  let a = inlined(1);
  let b = inlined(y);
  return a + b;
}

%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(inlined);
assertEquals(6, foo(2));
%OptimizeMaglevOnNextCall(foo);
assertEquals(6, foo(2));
assertEquals(6.2, foo(2.1));

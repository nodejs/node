// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function foo(x) {
  return NaN ** x;
}

%PrepareFunctionForOptimization(foo);
assertEquals(NaN, foo(1));
assertEquals(1, foo(0));
assertEquals(1, foo(-0));
%OptimizeFunctionOnNextCall(foo);
assertEquals(NaN, foo(1));
assertEquals(1, foo(0));
assertEquals(1, foo(-0));

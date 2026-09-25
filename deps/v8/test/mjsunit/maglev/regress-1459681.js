// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function foo(x) {
  return x >> 32; // We should not shift anything here!
}
%PrepareFunctionForOptimization(foo);
assertEquals(42, foo(42));
%OptimizeMaglevOnNextCall(foo);
assertEquals(42, foo(42));

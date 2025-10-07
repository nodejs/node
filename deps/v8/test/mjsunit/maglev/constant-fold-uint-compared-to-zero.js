// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function foo(a) {
  return (a >>> 2) >= 0;
}
%PrepareFunctionForOptimization(foo);
assertTrue(foo(100));
%OptimizeMaglevOnNextCall(foo);
assertTrue(foo(100));

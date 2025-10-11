// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(param) {
  let val1 = 0x7fffffff
  var val2 = param & -5e-324;
  val1 += param;
  return val1 & 0x7fffffff | val2;
}
%PrepareFunctionForOptimization(foo);
foo(1);
%OptimizeFunctionOnNextCall(foo);
assertEquals(2147483645, foo(-1.9));

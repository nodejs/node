// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap --expose-gc

function f() {
  var o = [{
    [Symbol.toPrimitive]() {}
  }];
  %_DeoptimizeNow();
  return o.length;
}
%PrepareFunctionForOptimization(f);
assertEquals(1, f());
assertEquals(1, f());
%OptimizeFunctionOnNextCall(f);
assertEquals(1, f());
gc();

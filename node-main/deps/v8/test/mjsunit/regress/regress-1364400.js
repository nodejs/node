// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(deopt, x) {
  x = x >>> 0;
  return deopt ? Math.max(x) : x;
}

function bar(deopt) {
  return foo(deopt, 4294967295);
};

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
bar(false);
%OptimizeFunctionOnNextCall(bar);
// bar will bailout because of insufficient type feedback for generic named
// access. The HeapNumber should be correctly rematerialized in deoptimzer.
assertEquals(4294967295, bar(true));

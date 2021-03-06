// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-escape

function f(apply) {
  var value = 23;
  apply(function bogeyman() { value = 42 });
  return value;
}
%PrepareFunctionForOptimization(f);
function apply(fun) { fun() }
assertEquals(42, f(apply));
assertEquals(42, f(apply));
%NeverOptimizeFunction(apply);
%OptimizeFunctionOnNextCall(f);
assertEquals(42, f(apply));

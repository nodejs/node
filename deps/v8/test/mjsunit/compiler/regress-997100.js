// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function C() { return this };
function foo() {
  return new C() instanceof function(){};
}
%PrepareFunctionForOptimization(C);
%PrepareFunctionForOptimization(foo);
assertFalse(foo());
%OptimizeFunctionOnNextCall(foo);
assertFalse(foo());

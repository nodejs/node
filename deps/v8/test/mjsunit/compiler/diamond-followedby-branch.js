// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-assert-types
// Flags: --turboshaft-enable-debug-features

// Check that the branch elimination replace the redundant branch condition with
// a phi node, and then the branch is folded in EffectControlLinearizationPhase.
function foo(cond, v1, v2) {
  cond = cond | 0;
  var a = cond == 1 ? v1 : v2;
  if(cond == 1) {
    %TurbofanStaticAssert(a == v1);
  } else {
    %TurbofanStaticAssert(a == v2);
  }
}

%PrepareFunctionForOptimization(foo);
foo(1, 10, 20); foo(2, 30, 40);
%OptimizeFunctionOnNextCall(foo);
foo(1, 10, 20); foo(2, 30, 40);

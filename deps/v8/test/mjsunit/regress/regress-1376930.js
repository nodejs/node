// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

function f() {
  let a = 42n;
  // JSDecrement should be typed as BigInt.
  let b = a--;
  let c = -42n && 42n;
  // JSDecrement was typed as Numeric instead of BigInt so the node could not
  // be eliminated because of possible deoptimization.
  let d = c & a;
};

%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);
f();

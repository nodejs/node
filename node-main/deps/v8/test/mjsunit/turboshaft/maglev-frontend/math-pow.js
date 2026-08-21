// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function pow(y, x) {
  return Math.pow(y, x);
}

%PrepareFunctionForOptimization(pow);
assertEquals(8, pow(2, 3));
%OptimizeFunctionOnNextCall(pow);
assertEquals(8, pow(2, 3));
assertOptimized(pow);

let o = { valueOf: function() { %DeoptimizeFunction(pow); return 4 } };

assertEquals(16, pow(o, 2));
assertUnoptimized(pow);
assertEquals(16, pow(o, 2));

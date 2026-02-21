// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function atan2(y, x) {
  return Math.atan2(y, x);
}

%PrepareFunctionForOptimization(atan2);
assertEquals(Math.PI/4, atan2(1, 1));
%OptimizeFunctionOnNextCall(atan2);
assertEquals(Math.PI/4, atan2(1, 1));
assertOptimized(atan2);

let o = { valueOf: function() { %DeoptimizeFunction(atan2); return -1 } };

assertEquals(-Math.PI / 4, atan2(o, 1));
assertUnoptimized(atan2);
assertEquals(-Math.PI / 4, atan2(o, 1));

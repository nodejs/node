// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-maglev --no-turbolev

// An out-of-bounds length deopt in an inlined Array(n) call must stop
// Turbofan from inlining the call again.

function f(n) {
  return Array(n);
}

%PrepareFunctionForOptimization(f);
f(1);
f(2);
%OptimizeFunctionOnNextCall(f);
f(3);
assertOptimized(f);
assertEquals(100000, f(100000).length);
assertUnoptimized(f);

%OptimizeFunctionOnNextCall(f);
f(3);
assertOptimized(f);
assertEquals(100000, f(100000).length);
assertOptimized(f);

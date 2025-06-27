// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --allow-natives-syntax --no-always-turbofan

function f(b) {
 if (b == 1) {
   let a = a = 20;
 }
}

f(0);
assertThrows(() => {f(1)}, ReferenceError);

%PrepareFunctionForOptimization(f);
f(0);
f(0);
%OptimizeFunctionOnNextCall(f);
f(0);
assertOptimized(f);
// Check that hole checks are handled correctly in optimized code.
assertThrows(() => {f(1)}, ReferenceError);
assertOptimized(f);

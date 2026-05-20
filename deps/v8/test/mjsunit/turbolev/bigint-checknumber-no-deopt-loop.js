// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev-future --turbofan

function foo(a) {
  (a++).constructor;
}
%PrepareFunctionForOptimization(foo);

let bi = -536870912n;
foo(bi);
%OptimizeFunctionOnNextCall(foo);
foo(bi);

assertOptimized(foo);

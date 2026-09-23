// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev-future --no-maglev-cse

function f(...args) {
  let a = arguments;
  return args.length + a.length;
}
%PrepareFunctionForOptimization(f);
assertEquals(6, f(1, 2, 3));
%OptimizeFunctionOnNextCall(f);
assertEquals(6, f(1, 2, 3));

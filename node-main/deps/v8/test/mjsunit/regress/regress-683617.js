// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

global = 'n';
function f(deopt) {
  let it = global[Symbol.iterator]();
  if (deopt) {
    return it.next().value;
  }
};
%PrepareFunctionForOptimization(f);
f();
f();
%OptimizeFunctionOnNextCall(f);
assertEquals('n', f(true));

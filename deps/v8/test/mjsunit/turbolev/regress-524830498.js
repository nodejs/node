// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

const arr = [];
function getAt() {
  return Array.prototype.at;
}
function f(o) {
  o[0];
  %OptimizeFunctionOnNextCall(f);
  return getAt().call(o, undefined);
}
%PrepareFunctionForOptimization(getAt);
%PrepareFunctionForOptimization(f);
f(arr);
f(arr);

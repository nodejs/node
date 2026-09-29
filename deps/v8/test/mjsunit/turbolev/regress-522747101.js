// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

const arr = (() => [])();
function getAt() {
  return Array.prototype.at;
}
function bar() {}
function foo(o) {
  o[0];
  return getAt().call(o, bar());
}
%PrepareFunctionForOptimization(getAt);
%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo(arr);
%OptimizeFunctionOnNextCall(foo);
foo(arr);

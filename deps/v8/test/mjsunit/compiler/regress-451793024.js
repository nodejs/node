// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan

function F() {}

const kBoundArgs = 65525;
const args = new Array(kBoundArgs);

let bound;
const args_for_apply = [null, ...args];
bound = Function.prototype.bind.apply(F, args_for_apply);

function g() {
    new bound(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
}

%PrepareFunctionForOptimization(F);
%PrepareFunctionForOptimization(g);
try {
  g();
} catch (e) {
  // Note that this only throws on some architectures, and not all of them
  // (depends on the stack size), so we use a try-catch rather than an
  // assertThrows.
  assertException(e, RangeError, "Maximum call stack size exceeded");
}

%OptimizeFunctionOnNextCall(g);
try {
  g();
} catch (e) {
  // Note that this only throws on some architectures, and not all of them
  // (depends on the stack size), so we use a try-catch rather than an
  // assertThrows.
  assertException(e, RangeError, "Maximum call stack size exceeded");
}

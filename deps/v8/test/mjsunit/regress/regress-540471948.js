// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const generic = { x: 29 };
generic.x = function() {};

function foo() {
  var o = { x: 1 };
  for (var i = 0; i < 1; i = {}) {
    i += o.x + 4294967295;
    i += o.x + 4294967295;
  }
}

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
foo();

// Same None handling, reached with a live value use: the replacement for a
// None typed node is an Unreachable, and a live value use of one has to be a
// DeadValue. Here the loop phi keeps the addition alive.
const maxFeedback = 1125899906842623;  // 2^50 - 1
const word = 1099511627775;            // 2^40 - 1

function liveUse(a, n) {
  let v = 0;
  for (let i = 0; i < (n & 3); i++) {
    v = v + ((a | 0) + maxFeedback);
  }
  return v | 0;
}

const args = [[0, 0], [word, -word], [word - 4, word - 3], [word, 1]];
%PrepareFunctionForOptimization(liveUse);
for (let k = 0; k < 10; k++) liveUse(...args[k % 4]);
%OptimizeFunctionOnNextCall(liveUse);
assertEquals(-2, liveUse(...args[1]));

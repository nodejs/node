// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function* gen(inner) {
  yield* inner;
  yield "gen yielded";
}

let calls = 0;
let inner = {
  [Symbol.iterator]() { return this; },
  next() {
    calls++;
    if (calls === 1) {
      return { value: "first", done: false };
    } else if (calls === 2) {
      %DeoptimizeFunction(callNext);
      return { value: "second", done: false };
    } else {
      return { value: undefined, done: true };
    }
  }
};

function callNext(g) {
  return g.next();
}

%PrepareFunctionForOptimization(callNext);

let g = gen(inner);
callNext(g);

%OptimizeMaglevOnNextCall(callNext);

// This lazy deopts.
callNext(g);
assertUnoptimized(callNext);

// Call next from unoptimized code.
let nextYielded = g.next();

assertEquals("gen yielded", nextYielded.value);

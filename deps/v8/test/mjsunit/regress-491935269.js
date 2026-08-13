// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function* gen() {
  yield 1;
}

function* outer() {
  yield* gen();
}

function test() {
  for (let obj of outer()) {
  }
}

%PrepareFunctionForOptimization(outer);
test();
test();
%OptimizeFunctionOnNextCall(outer);
test();

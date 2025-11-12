// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function* foo() {
  yield;
  new Set();
  for (let x of []) {
    for (let y of []) {
      yield;
    }
  }
}

%PrepareFunctionForOptimization(foo);
let gaga = foo();
gaga.next();
%OptimizeFunctionOnNextCall(foo);
gaga.next();

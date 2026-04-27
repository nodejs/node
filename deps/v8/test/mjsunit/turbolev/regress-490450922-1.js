// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

const o = {};

function foo() {
  for (let i = 0; i < 5; i++) {
    const x = i > 10 ? i : o;
    x.y = x;
  }
}

%PrepareFunctionForOptimization(foo);
foo();

%OptimizeFunctionOnNextCall(foo);
foo();

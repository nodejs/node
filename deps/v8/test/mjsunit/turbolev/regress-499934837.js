// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const obj = {};
obj[Symbol.hasInstance] = () => {};

function bar(x) {
  try {
    x.toString();
  } catch (e) {}
  return obj;
}

function foo() {
  for (let i = 0; i < 5; i++) {
    const o = bar();
    %OptimizeOsr();
    o instanceof o;
  }
}

%PrepareFunctionForOptimization(foo);
foo();

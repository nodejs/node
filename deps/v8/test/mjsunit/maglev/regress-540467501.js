// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


function foo(x) {
  return x !== this.length;
}

%PrepareFunctionForOptimization(foo);
foo("test");

for (let i = 0; i < 5; i++) {
  %PrepareFunctionForOptimization(%GetFunctionForCurrentFrame());
  %OptimizeOsr();
  const arr = Array();
  arr.forEach(foo, arr);
}

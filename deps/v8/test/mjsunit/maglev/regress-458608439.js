// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let x = 4.5;
let y = 0;

function foo(n) {
  x = n;
  y = n;
}

%PrepareFunctionForOptimization(foo);

%OptimizeMaglevOnNextCall(foo);
foo();

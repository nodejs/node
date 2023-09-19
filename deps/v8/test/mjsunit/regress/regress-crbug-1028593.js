// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  try {
    return "number".charCodeAt(0n);
  } catch(e) {}
}
%PrepareFunctionForOptimization(foo);
for (let i = 0; i < 3; i++) {
  foo();
}
%OptimizeFunctionOnNextCall(foo);
foo();

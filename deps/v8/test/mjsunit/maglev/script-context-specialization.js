// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-specialize-for-script-context --maglev
// Flags: --allow-natives-syntax

const scriptConst = 42;

function createClosure() {
  return function foo() {
    return scriptConst;
  };
}

// Create the closure twice to avoid function context specialization.
const f1 = createClosure();
const f2 = createClosure();

%PrepareFunctionForOptimization(f1);
assertEquals(42, f1());
%OptimizeMaglevOnNextCall(f1);
assertEquals(42, f1());
assertMaglevved(f1);

%PrepareFunctionForOptimization(f2);
assertEquals(42, f2());
%OptimizeMaglevOnNextCall(f2);
assertEquals(42, f2());
assertMaglevved(f2);

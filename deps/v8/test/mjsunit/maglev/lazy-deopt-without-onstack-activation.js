// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-always-opt

var x = 1;

function f(o) {
  return x;
}

%PrepareFunctionForOptimization(f);
assertEquals(1, f());

%OptimizeMaglevOnNextCall(f);
assertEquals(1, f());
assertTrue(isMaglevved(f));

// Trigger a lazy deopt now, so that f() deopts on its next call.
x = 2;
assertEquals(2, f());
assertFalse(isMaglevved(f));
assertUnoptimized(f);

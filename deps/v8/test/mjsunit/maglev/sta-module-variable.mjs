// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-always-turbofan

export let x = 1;
function foo(y) {
  x = x + 1;
  return x + y;
};

%PrepareFunctionForOptimization(foo);
assertEquals(3, foo(1));
assertEquals(4, foo(1));
%OptimizeMaglevOnNextCall(foo);
assertEquals(5, foo(1));
assertEquals(6, foo(1));
assertTrue(isMaglevved(foo));


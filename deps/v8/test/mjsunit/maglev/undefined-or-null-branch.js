// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-always-turbofan

function foo(x) {
  return x ?? 10
}

%PrepareFunctionForOptimization(foo);
assertEquals(10, foo(undefined));
assertEquals(10, foo(null));
assertEquals(1, foo(1));
%OptimizeMaglevOnNextCall(foo);
assertEquals(10, foo(undefined));
assertEquals(10, foo(null));
assertEquals(1, foo(1));
assertTrue(isMaglevved(foo));

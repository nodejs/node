// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function foo(a) {
  return a.at(1);
}
%PrepareFunctionForOptimization(foo);
foo([1, 2, 3,,4]);

%OptimizeFunctionOnNextCall(foo);
foo([1, 2, 3,,4]);

Array.prototype[1] = 3333;
assertEquals(foo([1,, 3,, 4]), 3333);

// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev

function bar(i) {
  if (i == 5) { ThrowSomething(); }
}

function foo(x) {
  let b = x + 2;
  try {
    for (let i = 0; i < 42; i++) {
      b >>= 0.0;
      bar(i);
    }
  } catch(e) {
    return b + 42;
  }
}

%NeverOptimizeFunction(bar);
%PrepareFunctionForOptimization(foo);
assertEquals(49, foo(5.3));
assertEquals(49, foo(5.3));

%OptimizeFunctionOnNextCall(foo);
assertEquals(49, foo(5.3));

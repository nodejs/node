// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function foo(n) {
  let s = "a";
  for (let i = 0; i < n; i++) {
    s = "b";
  }
  let c = s + "x";
  return c + "0123456789abc";
}

%PrepareFunctionForOptimization(foo);
foo(2);
%OptimizeFunctionOnNextCall(foo);
assertEquals("bx0123456789abc", foo(2));

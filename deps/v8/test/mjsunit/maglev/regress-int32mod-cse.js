// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function foo(x) {
  if (x < 0) return 0;
  let a = (x % 7) | 0;
  let b = (x % 8) | 0;
  return b;
}

%PrepareFunctionForOptimization(foo);
for (let i = 0; i < 20; i++) {
  foo(i);
}
%OptimizeFunctionOnNextCall(foo);
foo(0);

assertEquals(0, foo(8));

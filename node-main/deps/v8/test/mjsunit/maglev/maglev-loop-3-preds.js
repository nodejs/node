// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --no-maglev-loop-peeling
// Flags: --max-inlined-bytecode-size-small=0

function bar() {
  try { fail(); } catch(e) {};
}

function foo(c) {
  let v = 1;
  if (c == 1) {
    v = 2;
  }
  while (v < 10) {
    v += 2;
    bar();
  }
  return v;
}
%PrepareFunctionForOptimization(foo);
assertEquals(10, foo(1));
assertEquals(11, foo(2));

%PrepareFunctionForOptimization(bar);
%OptimizeMaglevOnNextCall(foo);
assertEquals(10, foo(1));
assertEquals(11, foo(2));

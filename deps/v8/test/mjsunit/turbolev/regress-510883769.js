// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

(function() {
  let captured = 0;

  function get_builtin() {
    for (let i = 0; i < 5; i++);
    captured++;
    return BigInt;
  }
  function mycall(f, x) { captured++; return f(x); }
  function foo(x) { return mycall(get_builtin(), x); }

  %PrepareFunctionForOptimization(get_builtin);
  %PrepareFunctionForOptimization(mycall);
  %PrepareFunctionForOptimization(foo);

  assertEquals(1n, foo(1));
  assertEquals(2n, foo(2));

  mycall(() => 0n, 0);

  %OptimizeFunctionOnNextCall(foo);
  assertEquals(3n, foo(3));
  assertEquals(4n, foo(4));
  assertOptimized(foo);
})();

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --jit-fuzzing --turbolev-future

function foo(x, ...restfoo) {
  function bar(...restbar) {
    function baz(...restbaz) {
      const v16 = { a: x, e: restbar };
    }
    baz(restfoo);
    baz();
  }
  bar();
  bar(restfoo);
}

%PrepareFunctionForOptimization(foo);
foo(0, 0);

%OptimizeFunctionOnNextCall(foo);
foo(0, 0);

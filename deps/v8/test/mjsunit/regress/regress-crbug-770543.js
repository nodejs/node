// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function FunctionCallerFromInlinedBuiltin() {
  function f() {
    function g() {
      Object.getOwnPropertyDescriptor(g, "caller");
    };
    [0].forEach(g);
  };
  %PrepareFunctionForOptimization(f);
  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  f();
})();

(function FunctionArgumentsFromInlinedBuiltin() {
  function g() {
    g.arguments;
  }
  function f() {
    [0].forEach(g);
  };
  %PrepareFunctionForOptimization(f);
  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  f();
})();

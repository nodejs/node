// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  var use_symbol = {
    [Symbol.hasInstance] : () => true
  };
  var use_symbol_double = {
    [Symbol.hasInstance] : 0.1
  };
  function bar(b) {
    if (b) return {} instanceof use_symbol_double;
  }
  %PrepareFunctionForOptimization(bar);
  function foo(b) { return bar(); }
  %PrepareFunctionForOptimization(foo);
  foo();
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();

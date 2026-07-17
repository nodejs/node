// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

assertThrows = function assertThrows(code) {};
%PrepareFunctionForOptimization(assertThrows);

function foo(val) {
  var arr = [];
  function bar() {
    function m1() {}
    %PrepareFunctionForOptimization(m1);
    assertThrows(m1);
    0 in arr;
  }
  %PrepareFunctionForOptimization(bar);
  bar();  // virtual context distance of 1 from native_context
  gc();
  bar(true);
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();

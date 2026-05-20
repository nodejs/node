// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(...v) {
  function bar() {
    return arguments.length;
  }
  Array.prototype.splice.apply(v, v);
  Array.prototype.splice.apply(v, v);
  return bar(...v);
}
%PrepareFunctionForOptimization(foo);
assertEquals(3, foo(1, 2, 3, 4));
%OptimizeFunctionOnNextCall(foo);
assertEquals(3, foo(1, 2, 3, 4));
assertEquals(3, foo(1, 2, 3, 4));

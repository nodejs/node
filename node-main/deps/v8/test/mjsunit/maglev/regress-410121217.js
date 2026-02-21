// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function test() {
  const array = [];
  function foo(o) {
    o.prop1 = o;
    o.prop2 = 1.797;
  }
  %PrepareFunctionForOptimization(foo);
  foo.prop3 = foo;
  foo(foo);
  foo(array);
}
%PrepareFunctionForOptimization(test);
test();
test();
%OptimizeMaglevOnNextCall(test);
test();

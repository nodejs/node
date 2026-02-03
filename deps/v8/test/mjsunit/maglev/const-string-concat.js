// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function foo1(a, b) {
  return a + b;
}
%PrepareFunctionForOptimization(foo1);

function foo2() {
  return foo1('foo', 'bar');
}
%PrepareFunctionForOptimization(foo2);

assertEquals('foobar', foo2());
%OptimizeMaglevOnNextCall(foo2);
assertEquals('foobar', foo2());
assertTrue(isMaglevved(foo2));

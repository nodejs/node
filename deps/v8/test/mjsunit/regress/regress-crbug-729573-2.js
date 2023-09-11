// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar(x) {
  "use strict";
  return this + x;
}

function foo(f) {
  var a = bar.bind(42, 1);
  return f() ? 0 : a;
};
%PrepareFunctionForOptimization(foo);
function t() {
  return true;
}

assertEquals(0, foo(t));
assertEquals(0, foo(t));
%OptimizeFunctionOnNextCall(foo);
var a = foo(_ => false);
assertEquals(43, a());

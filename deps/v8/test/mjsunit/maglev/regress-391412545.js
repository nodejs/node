// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

var str = new String();
function bar(a, b) {
  return a == b;
}
function baz(a) {
  return str === a;
}

function foo(a, b) {
  bar(a, b);
  bar(b, b);
  bar(a, b);
  baz();
  baz();
  return baz(b);
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
foo();
foo(() => true, () => false);
%OptimizeFunctionOnNextCall(foo);
assertTrue(foo(() => true, str));

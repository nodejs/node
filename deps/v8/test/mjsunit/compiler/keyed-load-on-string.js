// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var s = "hello";

function foo() {
  return s[4];
}
assertTrue("o" === foo());
assertTrue("o" === foo());
%OptimizeFunctionOnNextCall(foo);
assertTrue("o" === foo());

function bar() {
  return s[5];
}
assertSame(undefined, bar());
assertSame(undefined, bar());
%OptimizeFunctionOnNextCall(bar);
assertSame(undefined, bar());

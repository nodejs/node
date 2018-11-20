// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var expected = undefined;

function foo() {
  var a = [0,,{}];
  a.shift();
  assertEquals(expected, a[0]);
}
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();

expected = 42;
Array.prototype[0] = 153;
Array.prototype[1] = expected;
foo();

function bar() {
  var a = [0,,{}];
  a.shift();
  assertEquals(expected, a[0]);
}
bar();
bar();
%OptimizeFunctionOnNextCall(bar);
bar();
